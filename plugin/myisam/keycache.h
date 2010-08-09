/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Key cache variable structures */

#ifndef PLUGIN_MYISAM_KEYCACHE_H
#define PLUGIN_MYISAM_KEYCACHE_H

enum flush_type
{
  FLUSH_KEEP,           /* flush block and keep it in the cache */
  FLUSH_RELEASE,        /* flush block and remove it from the cache */
  FLUSH_IGNORE_CHANGED, /* remove block from the cache */
  /*
 *     As my_disable_flush_pagecache_blocks is always 0, the following option
 *         is strictly equivalent to FLUSH_KEEP
 *           */
  FLUSH_FORCE_WRITE
};


/* declare structures that is used by st_key_cache */

struct st_block_link;
typedef struct st_block_link BLOCK_LINK;
struct st_keycache_page;
typedef struct st_keycache_page KEYCACHE_PAGE;
struct st_hash_link;
typedef struct st_hash_link HASH_LINK;

namespace drizzled
{
namespace internal
{
typedef uint64_t my_off_t;
struct st_my_thread_var;
}

/* info about requests in a waiting queue */
typedef struct st_keycache_wqueue
{
  drizzled::internal::st_my_thread_var *last_thread;  /* circular list of waiting threads */
} KEYCACHE_WQUEUE;

#define CHANGED_BLOCKS_HASH 128             /* must be power of 2 */

/*
  The key cache structure
  It also contains read-only statistics parameters.
*/

typedef struct st_key_cache
{
  bool key_cache_inited;
  bool in_resize;             /* true during resize operation             */
  bool resize_in_flush;       /* true during flush of resize operation    */
  bool can_be_used;           /* usage of cache for read/write is allowed */
  uint32_t hash_entries;             /* max number of entries in the hash table  */
  uint32_t key_cache_mem_size;      /* specified size of the cache memory       */
  uint32_t key_cache_block_size;     /* size of the page buffer of a cache block */
  int disk_blocks;               /* max number of blocks in the cache        */
  ulong min_warm_blocks;         /* min number of warm blocks;               */
  ulong age_threshold;           /* age threshold for hot blocks             */
  uint64_t keycache_time;       /* total number of block link operations    */
  int hash_links;                /* max number of hash links                 */
  int hash_links_used;           /* number of hash links currently used      */
  ulong blocks_used; /* maximum number of concurrently used blocks */
  ulong blocks_unused; /* number of currently unused blocks */
  ulong blocks_changed;          /* number of currently dirty blocks         */
  ulong warm_blocks;             /* number of blocks in warm sub-chain       */
  ulong cnt_for_resize_op;       /* counter to block resize operation        */
  long blocks_available;      /* number of blocks available in the LRU chain */
  HASH_LINK **hash_root;         /* arr. of entries into hash table buckets  */
  HASH_LINK *hash_link_root;     /* memory for hash table links              */
  HASH_LINK *free_hash_list;     /* list of free hash links                  */
  BLOCK_LINK *free_block_list;   /* list of free blocks */
  BLOCK_LINK *block_root;        /* memory for block links                   */
  unsigned char *block_mem;     /* memory for block buffers                 */
  BLOCK_LINK *used_last;         /* ptr to the last block of the LRU chain   */
  BLOCK_LINK *used_ins;          /* ptr to the insertion block in LRU chain  */
  pthread_mutex_t cache_lock;    /* to lock access to the cache structure    */
  KEYCACHE_WQUEUE resize_queue;  /* threads waiting during resize operation  */
  /*
    Waiting for a zero resize count. Using a queue for symmetry though
    only one thread can wait here.
  */
  KEYCACHE_WQUEUE waiting_for_resize_cnt;
  KEYCACHE_WQUEUE waiting_for_hash_link; /* waiting for a free hash link     */
  KEYCACHE_WQUEUE waiting_for_block;    /* requests waiting for a free block */
  BLOCK_LINK *changed_blocks[CHANGED_BLOCKS_HASH]; /* hash for dirty file bl.*/
  BLOCK_LINK *file_blocks[CHANGED_BLOCKS_HASH];    /* hash for other file bl.*/

  /*
    The following variables are and variables used to hold parameters for
    initializing the key cache.
  */

  uint32_t param_buff_size;    /* size the memory allocated for the cache  */
  uint32_t param_block_size;   /* size of the blocks in the key cache      */
  uint32_t param_division_limit; /* min. percentage of warm blocks           */
  uint32_t param_age_threshold;    /* determines when hot block is downgraded  */

  int blocks;                   /* max number of blocks in the cache        */
  /* Statistics variables. These are reset in reset_key_cache_counters(). */
  ulong global_blocks_changed;	/* number of currently dirty blocks         */
  uint64_t global_cache_w_requests;/* number of write requests (write hits) */
  uint64_t global_cache_write;     /* number of writes from cache to files  */
  uint64_t global_cache_r_requests;/* number of read requests (read hits)   */
  uint64_t global_cache_read;      /* number of reads from files to cache   */

  bool in_init;		/* Set to 1 in MySQL during init/resize     */
} KEY_CACHE;

} /* namespace drizzled */

/* The default key cache */
extern int init_key_cache(drizzled::KEY_CACHE *keycache, uint32_t key_cache_block_size,
			  size_t use_mem, uint32_t division_limit,
			  uint32_t age_threshold);
extern int resize_key_cache(drizzled::KEY_CACHE *keycache, uint32_t key_cache_block_size,
			    size_t use_mem, uint32_t division_limit,
			    uint32_t age_threshold);
extern unsigned char *key_cache_read(drizzled::KEY_CACHE *keycache,
                            int file, drizzled::internal::my_off_t filepos, int level,
                            unsigned char *buff, uint32_t length,
			    uint32_t block_length,int return_buffer);
extern int key_cache_insert(drizzled::KEY_CACHE *keycache,
                            int file, drizzled::internal::my_off_t filepos, int level,
                            unsigned char *buff, uint32_t length);
extern int key_cache_write(drizzled::KEY_CACHE *keycache,
                           int file, drizzled::internal::my_off_t filepos, int level,
                           unsigned char *buff, uint32_t length,
			   uint32_t block_length,int force_write);
extern int flush_key_blocks(drizzled::KEY_CACHE *keycache,
                            int file, enum flush_type type);
extern void end_key_cache(drizzled::KEY_CACHE *keycache, bool cleanup);

/*
  Next highest power of two

  SYNOPSIS
    my_round_up_to_next_power()
    v		Value to check

  RETURN
    Next or equal power of 2
    Note: 0 will return 0

  NOTES
    Algorithm by Sean Anderson, according to:
    http://graphics.stanford.edu/~seander/bithacks.html
    (Orignal code public domain)

    Comments shows how this works with 01100000000000000000000000001011
*/

static inline uint32_t my_round_up_to_next_power(uint32_t v)
{
  v--;			/* 01100000000000000000000000001010 */
  v|= v >> 1;		/* 01110000000000000000000000001111 */
  v|= v >> 2;		/* 01111100000000000000000000001111 */
  v|= v >> 4;		/* 01111111110000000000000000001111 */
  v|= v >> 8;		/* 01111111111111111100000000001111 */
  v|= v >> 16;		/* 01111111111111111111111111111111 */
  return v+1;		/* 10000000000000000000000000000000 */
}


#endif /* PLUGIN_MYISAM_KEYCACHE_H */
