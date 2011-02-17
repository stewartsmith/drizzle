/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  These functions handle keyblock cacheing for ISAM and MyISAM tables.

  One cache can handle many files.
  It must contain buffers of the same blocksize.
  init_key_cache() should be used to init cache handler.

  The free list (free_block_list) is a stack like structure.
  When a block is freed by free_block(), it is pushed onto the stack.
  When a new block is required it is first tried to pop one from the stack.
  If the stack is empty, it is tried to get a never-used block from the pool.
  If this is empty too, then a block is taken from the LRU ring, flushing it
  to disk, if neccessary. This is handled in find_key_block().
  With the new free list, the blocks can have three temperatures:
  hot, warm and cold (which is free). This is remembered in the block header
  by the enum BLOCK_TEMPERATURE temperature variable. Remembering the
  temperature is neccessary to correctly count the number of warm blocks,
  which is required to decide when blocks are allowed to become hot. Whenever
  a block is inserted to another (sub-)chain, we take the old and new
  temperature into account to decide if we got one more or less warm block.
  blocks_unused is the sum of never used blocks in the pool and of currently
  free blocks. blocks_used is the number of blocks fetched from the pool and
  as such gives the maximum number of in-use blocks at any time.

  Key Cache Locking
  =================

  All key cache locking is done with a single mutex per key cache:
  keycache->cache_lock. This mutex is locked almost all the time
  when executing code in this file (mf_keycache.c).
  However it is released for I/O and some copy operations.

  The cache_lock is also released when waiting for some event. Waiting
  and signalling is done via condition variables. In most cases the
  thread waits on its thread->suspend condition variable. Every thread
  has a my_thread_var structure, which contains this variable and a
  '*next' and '**prev' pointer. These pointers are used to insert the
  thread into a wait queue.

  A thread can wait for one block and thus be in one wait queue at a
  time only.

  Before starting to wait on its condition variable with
  pthread_cond_wait(), the thread enters itself to a specific wait queue
  with link_into_queue() (double linked with '*next' + '**prev') or
  wait_on_queue() (single linked with '*next').

  Another thread, when releasing a resource, looks up the waiting thread
  in the related wait queue. It sends a signal with
  pthread_cond_signal() to the waiting thread.

  NOTE: Depending on the particular wait situation, either the sending
  thread removes the waiting thread from the wait queue with
  unlink_from_queue() or release_whole_queue() respectively, or the waiting
  thread removes itself.

  There is one exception from this locking scheme when one thread wants
  to reuse a block for some other address. This works by first marking
  the block reserved (status= BLOCK_IN_SWITCH) and then waiting for all
  threads that are reading the block to finish. Each block has a
  reference to a condition variable (condvar). It holds a reference to
  the thread->suspend condition variable for the waiting thread (if such
  a thread exists). When that thread is signaled, the reference is
  cleared. The number of readers of a block is registered in
  block->hash_link->requests. See wait_for_readers() / remove_reader()
  for details. This is similar to the above, but it clearly means that
  only one thread can wait for a particular block. There is no queue in
  this case. Strangely enough block->convar is used for waiting for the
  assigned hash_link only. More precisely it is used to wait for all
  requests to be unregistered from the assigned hash_link.

  The resize_queue serves two purposes:
  1. Threads that want to do a resize wait there if in_resize is set.
     This is not used in the server. The server refuses a second resize
     request if one is already active. keycache->in_init is used for the
     synchronization. See set_var.cc.
  2. Threads that want to access blocks during resize wait here during
     the re-initialization phase.
  When the resize is done, all threads on the queue are signalled.
  Hypothetical resizers can compete for resizing, and read/write
  requests will restart to request blocks from the freshly resized
  cache. If the cache has been resized too small, it is disabled and
  'can_be_used' is false. In this case read/write requests bypass the
  cache. Since they increment and decrement 'cnt_for_resize_op', the
  next resizer can wait on the queue 'waiting_for_resize_cnt' until all
  I/O finished.
*/

#include <config.h>
#include <drizzled/error.h>
#include <drizzled/internal/my_sys.h>
#include "keycache.h"
#include <drizzled/internal/m_string.h>
#include <drizzled/internal/my_bit.h>
#include <errno.h>
#include <stdarg.h>

using namespace drizzled;

/*
  Some compilation flags have been added specifically for this module
  to control the following:
  - not to let a thread to yield the control when reading directly
    from key cache, which might improve performance in many cases;
    to enable this add:
    #define SERIALIZED_READ_FROM_CACHE
  - to set an upper bound for number of threads simultaneously
    using the key cache; this setting helps to determine an optimal
    size for hash table and improve performance when the number of
    blocks in the key cache much less than the number of threads
    accessing it;
    to set this number equal to <N> add
      #define MAX_THREADS <N>

  Example of the settings:
    #define SERIALIZED_READ_FROM_CACHE
    #define MAX_THREADS   100
*/

#define STRUCT_PTR(TYPE, MEMBER, a)                                           \
          (TYPE *) ((char *) (a) - offsetof(TYPE, MEMBER))

/* types of condition variables */
#define  COND_FOR_REQUESTED 0
#define  COND_FOR_SAVED     1

typedef pthread_cond_t KEYCACHE_CONDVAR;

/* descriptor of the page in the key cache block buffer */
struct st_keycache_page
{
  int file;               /* file to which the page belongs to  */
  internal::my_off_t filepos;       /* position of the page in the file   */
};

/* element in the chain of a hash table bucket */
struct st_hash_link
{
  struct st_hash_link *next, **prev; /* to connect links in the same bucket  */
  struct st_block_link *block;       /* reference to the block for the page: */
  int file;                         /* from such a file                     */
  internal::my_off_t diskpos;                  /* with such an offset                  */
  uint32_t requests;                     /* number of requests for the page      */
};

/* simple states of a block */
#define BLOCK_ERROR           1 /* an error occured when performing file i/o */
#define BLOCK_READ            2 /* file block is in the block buffer         */
#define BLOCK_IN_SWITCH       4 /* block is preparing to read new page       */
#define BLOCK_REASSIGNED      8 /* blk does not accept requests for old page */
#define BLOCK_IN_FLUSH       16 /* block is selected for flush               */
#define BLOCK_CHANGED        32 /* block buffer contains a dirty page        */
#define BLOCK_IN_USE         64 /* block is not free                         */
#define BLOCK_IN_EVICTION   128 /* block is selected for eviction            */
#define BLOCK_IN_FLUSHWRITE 256 /* block is in write to file                 */
#define BLOCK_FOR_UPDATE    512 /* block is selected for buffer modification */

/* page status, returned by find_key_block */
#define PAGE_READ               0
#define PAGE_TO_BE_READ         1
#define PAGE_WAIT_TO_BE_READ    2

/* block temperature determines in which (sub-)chain the block currently is */
enum BLOCK_TEMPERATURE { BLOCK_COLD /*free*/ , BLOCK_WARM , BLOCK_HOT };

/* key cache block */
struct st_block_link
{
  struct st_block_link
    *next_used, **prev_used;   /* to connect links in the LRU chain (ring)   */
  struct st_block_link
    *next_changed, **prev_changed; /* for lists of file dirty/clean blocks   */
  struct st_hash_link *hash_link; /* backward ptr to referring hash_link     */
  KEYCACHE_WQUEUE wqueue[2]; /* queues on waiting requests for new/old pages */
  uint32_t requests;          /* number of requests for the block                */
  unsigned char *buffer;           /* buffer for the block page                       */
  uint32_t offset;            /* beginning of modified data in the buffer        */
  uint32_t length;            /* end of data in the buffer                       */
  uint32_t status;            /* state of the block                              */
  enum BLOCK_TEMPERATURE temperature; /* block temperature: cold, warm, hot */
  uint32_t hits_left;         /* number of hits left until promotion             */
  uint64_t last_hit_time; /* timestamp of the last hit                      */
  KEYCACHE_CONDVAR *condvar; /* condition variable for 'no readers' event    */
};

#define FLUSH_CACHE         2000            /* sort this many blocks at once */

#define KEYCACHE_HASH(f, pos)                                                 \
(((uint32_t) ((pos) / keycache->key_cache_block_size) +                          \
                                     (uint32_t) (f)) & (keycache->hash_entries-1))
#define FILE_HASH(f)                 ((uint) (f) & (CHANGED_BLOCKS_HASH-1))


#define  keycache_pthread_cond_wait(A,B) (void)A;
#define keycache_pthread_mutex_lock(A) (void)A;
#define keycache_pthread_mutex_unlock(A) (void)A;
#define keycache_pthread_cond_signal(A) (void)A;

static inline uint32_t next_power(uint32_t value)
{
  return my_round_up_to_next_power(value) << 1;
}


/*
  Initialize a key cache

  SYNOPSIS
    init_key_cache()
    keycache			pointer to a key cache data structure
    key_cache_block_size	size of blocks to keep cached data
    use_mem                 	total memory to use for the key cache
    division_limit		division limit (may be zero)
    age_threshold		age threshold (may be zero)

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES.
    if keycache->key_cache_inited != 0 we assume that the key cache
    is already initialized.  This is for now used by myisamchk, but shouldn't
    be something that a program should rely on!

    It's assumed that no two threads call this function simultaneously
    referring to the same key cache handle.

*/

int init_key_cache(KEY_CACHE *keycache, uint32_t key_cache_block_size,
		   size_t use_mem, uint32_t division_limit,
		   uint32_t age_threshold)
{
  (void)keycache;
  (void)key_cache_block_size;
  (void)use_mem;
  (void)division_limit;
  (void)age_threshold;
  memset(keycache, 0, sizeof(KEY_CACHE));
  
  return 0;
}


/*
  Remove key_cache from memory

  SYNOPSIS
    end_key_cache()
    keycache		key cache handle
    cleanup		Complete free (Free also mutex for key cache)

  RETURN VALUE
    none
*/

void end_key_cache(KEY_CACHE *keycache, bool cleanup)
{
  (void)keycache;
  (void)cleanup;
} /* end_key_cache */


/*
  Add a hash link to a bucket in the hash_table
*/

static inline void link_hash(HASH_LINK **start, HASH_LINK *hash_link)
{
  if (*start)
    (*start)->prev= &hash_link->next;
  hash_link->next= *start;
  hash_link->prev= start;
  *start= hash_link;
}


/*
  Read a block of data from a cached file into a buffer;

  SYNOPSIS

    key_cache_read()
      keycache            pointer to a key cache data structure
      file                handler for the file for the block of data to be read
      filepos             position of the block of data in the file
      level               determines the weight of the data
      buff                buffer to where the data must be placed
      length              length of the buffer
      block_length        length of the block in the key cache buffer
      return_buffer       return pointer to the key cache buffer with the data

  RETURN VALUE
    Returns address from where the data is placed if sucessful, 0 - otherwise.

  NOTES.
    The function ensures that a block of data of size length from file
    positioned at filepos is in the buffers for some key cache blocks.
    Then the function either copies the data into the buffer buff, or,
    if return_buffer is true, it just returns the pointer to the key cache
    buffer with the data.
    Filepos must be a multiple of 'block_length', but it doesn't
    have to be a multiple of key_cache_block_size;
*/

unsigned char *key_cache_read(KEY_CACHE *keycache,
                      int file, internal::my_off_t filepos, int level,
                      unsigned char *buff, uint32_t length,
                      uint32_t block_length,
                      int return_buffer)
{
  (void)block_length;
  (void)return_buffer;
  (void)level;
  int error=0;
  unsigned char *start= buff;

  assert (! keycache->key_cache_inited);

  if (!pread(file, (unsigned char*) buff, length, filepos))
    error= 1;
  return(error ? (unsigned char*) 0 : start);
}


/*
  Insert a block of file data from a buffer into key cache

  SYNOPSIS
    key_cache_insert()
    keycache            pointer to a key cache data structure
    file                handler for the file to insert data from
    filepos             position of the block of data in the file to insert
    level               determines the weight of the data
    buff                buffer to read data from
    length              length of the data in the buffer

  NOTES
    This is used by MyISAM to move all blocks from a index file to the key
    cache

  RETURN VALUE
    0 if a success, 1 - otherwise.
*/

int key_cache_insert(KEY_CACHE *keycache,
                     int file, internal::my_off_t filepos, int level,
                     unsigned char *buff, uint32_t length)
{
  (void)file;
  (void)filepos;
  (void)level;
  (void)buff;
  (void)length;

  assert (!keycache->key_cache_inited);
  return 0;
}


/*
  Write a buffer into a cached file.

  SYNOPSIS

    key_cache_write()
      keycache            pointer to a key cache data structure
      file                handler for the file to write data to
      filepos             position in the file to write data to
      level               determines the weight of the data
      buff                buffer with the data
      length              length of the buffer
      dont_write          if is 0 then all dirty pages involved in writing
                          should have been flushed from key cache

  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES.
    The function copies the data of size length from buff into buffers
    for key cache blocks that are  assigned to contain the portion of
    the file starting with position filepos.
    It ensures that this data is flushed to the file if dont_write is false.
    Filepos must be a multiple of 'block_length', but it doesn't
    have to be a multiple of key_cache_block_size;

    dont_write is always true in the server (info->lock_type is never F_UNLCK).
*/

int key_cache_write(KEY_CACHE *keycache,
                    int file, internal::my_off_t filepos, int level,
                    unsigned char *buff, uint32_t length,
                    uint32_t block_length,
                    int dont_write)
{
  (void)block_length;
  (void)level;
  int error=0;

  if (!dont_write)
  {
    /* Not used in the server. */
    /* Force writing from buff into disk. */
    if (pwrite(file, buff, length, filepos) == 0)
      return(1);
  }

  assert (!keycache->key_cache_inited);

  /* Key cache is not used */
  if (dont_write)
  {
    /* Used in the server. */
    if (pwrite(file, (unsigned char*) buff, length, filepos) == 0)
      error=1;
  }

  return(error);
}


/*
  Flush all blocks for a file to disk

  SYNOPSIS

    flush_key_blocks()
      keycache            pointer to a key cache data structure
      file                handler for the file to flush to
      flush_type          type of the flush

  RETURN
    0   ok
    1  error
*/

int flush_key_blocks(KEY_CACHE *keycache,
                     int file, enum flush_type type)
{
  (void)file;
  (void)type;
  assert (!keycache->key_cache_inited);
  return 0;
}
