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

/**
 * @file
 * Routines to handle mallocing of results which will be freed the same time 
 */

#include "config.h"

#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/m_string.h"

#include <algorithm>

using namespace std;

namespace drizzled
{

static const unsigned int MAX_BLOCK_TO_DROP= 4096;
static const unsigned int MAX_BLOCK_USAGE_BEFORE_DROP= 10;

/**
 * @brief
 * Initialize memory root
 *
 * @details
 * This function prepares memory root for further use, sets initial size of
 * chunk for memory allocation and pre-allocates first block if specified.
 * Altough error can happen during execution of this function if
 * pre_alloc_size is non-0 it won't be reported. Instead it will be
 * reported as error in first alloc_root() on this memory root.
 *
 * @param  mem_root       memory root to initialize
 * @param  block_size     size of chunks (blocks) used for memory allocation
 *                       (It is external size of chunk i.e. it should include
 *                      memory required for internal structures, thus it
 *                      should be no less than memory::ROOT_MIN_BLOCK_SIZE)
 *
 */
void memory::Root::init_alloc_root(size_t block_size_arg)
{
  free= used= pre_alloc= 0;
  min_malloc= 32;
  block_size= block_size_arg - memory::ROOT_MIN_BLOCK_SIZE;
  error_handler= 0;
  block_num= 4;			/* We shift this with >>2 */
  first_block_usage= 0;
}


/**
 * @details
 * Function aligns and assigns new value to block size; then it tries to
 * reuse one of existing blocks as prealloc block, or malloc new one of
 * requested size. If no blocks can be reused, all unused blocks are freed
 * before allocation.
 *
 * @param  mem_root        memory root to change defaults of
 * @param  block_size      new value of block size. Must be greater or equal
 *                         than ALLOC_ROOT_MIN_BLOCK_SIZE (this value is about
 *                         68 bytes and depends on platform and compilation flags)
 * @param pre_alloc_size  new size of preallocated block. If not zero,
 *                        must be equal to or greater than block size,
 *                        otherwise means 'no prealloc'.
 */
void memory::Root::reset_root_defaults(size_t block_size_arg, size_t pre_alloc_size)
{
  block_size= block_size_arg - memory::ROOT_MIN_BLOCK_SIZE;
  if (pre_alloc_size)
  {
    size_t size= pre_alloc_size + ALIGN_SIZE(sizeof(memory::internal::UsedMemory));
    if (not pre_alloc || pre_alloc->size != size)
    {
      memory::internal::UsedMemory *mem, **prev= &this->free;
      /*
        Free unused blocks, so that consequent calls
        to reset_root_defaults won't eat away memory.
      */
      while (*prev)
      {
        mem= *prev;
        if (mem->size == size)
        {
          /* We found a suitable block, no need to do anything else */
          pre_alloc= mem;
          return;
        }
        if (mem->left + ALIGN_SIZE(sizeof(memory::internal::UsedMemory)) == mem->size)
        {
          /* remove block from the list and free it */
          *prev= mem->next;
          std::free(mem);
        }
        else
          prev= &mem->next;
      }
      /* Allocate new prealloc block and add it to the end of free list */
      if ((mem= static_cast<memory::internal::UsedMemory *>(malloc(size))))
      {
        mem->size= size;
        mem->left= pre_alloc_size;
        mem->next= *prev;
        *prev= pre_alloc= mem;
      }
      else
      {
        pre_alloc= 0;
      }
    }
  }
  else
  {
    pre_alloc= 0;
  }
}

/**
 * @brief 
 * Allocate a chunk of memory from the Root structure provided, 
 * obtaining more memory from the heap if necessary
 *
 * @pre
 * mem_root must have been initialised via init_alloc_root()
 *
 * @param  mem_root  The memory Root to allocate from
 * @param  length    The size of the block to allocate
 *
 * @todo Would this be more suitable as a member function on the
 * Root class?
 */
void *memory::Root::alloc_root(size_t length)
{
  unsigned char* point;
  memory::internal::UsedMemory *next= NULL;
  memory::internal::UsedMemory **prev;
  assert(alloc_root_inited());

  length= ALIGN_SIZE(length);
  if ((*(prev= &this->free)) != NULL)
  {
    if ((*prev)->left < length &&
	this->first_block_usage++ >= MAX_BLOCK_USAGE_BEFORE_DROP &&
	(*prev)->left < MAX_BLOCK_TO_DROP)
    {
      next= *prev;
      *prev= next->next;			/* Remove block from list */
      next->next= this->used;
      this->used= next;
      this->first_block_usage= 0;
    }
    for (next= *prev ; next && next->left < length ; next= next->next)
      prev= &next->next;
  }
  if (! next)
  {						/* Time to alloc new block */
    size_t get_size, tmp_block_size;

    tmp_block_size= this->block_size * (this->block_num >> 2);
    get_size= length+ALIGN_SIZE(sizeof(memory::internal::UsedMemory));
    get_size= max(get_size, tmp_block_size);

    if (!(next = static_cast<memory::internal::UsedMemory *>(malloc(get_size))))
    {
      if (this->error_handler)
	(*this->error_handler)();
      return NULL;
    }
    this->block_num++;
    next->next= *prev;
    next->size= get_size;
    next->left= get_size-ALIGN_SIZE(sizeof(memory::internal::UsedMemory));
    *prev=next;
  }

  point= (unsigned char*) ((char*) next+ (next->size-next->left));
  /** @todo next part may be unneeded due to this->first_block_usage counter*/
  if ((next->left-= length) < this->min_malloc)
  {						/* Full block */
    *prev= next->next;				/* Remove block from list */
    next->next= this->used;
    this->used= next;
    this->first_block_usage= 0;
  }

  return point;
}


/**
 * @brief
 * Allocate many pointers at the same time.
 *
 * @details
 * The variable arguments are a list of alternating pointers and lengths,
 * terminated by a null pointer:
 * @li <tt>char ** pointer1</tt>
 * @li <tt>uint length1</tt>
 * @li <tt>char ** pointer2</tt>
 * @li <tt>uint length2</tt>
 * @li <tt>...</tt>
 * @li <tt>NULL</tt>
 *
 * @c pointer1, @c pointer2 etc. all point into big allocated memory area
 *
 * @param root  Memory root
 *
 * @return
 * A pointer to the beginning of the allocated memory block in case of 
 * success or NULL if out of memory
 */
void *memory::Root::multi_alloc_root(int unused, ...)
{
  va_list args;
  char **ptr, *start, *res;
  size_t tot_length, length;

  (void)unused; // For some reason Sun Studio registers unused as not used.
  va_start(args, unused);
  tot_length= 0;
  while ((ptr= va_arg(args, char **)))
  {
    length= va_arg(args, uint);
    tot_length+= ALIGN_SIZE(length);
  }
  va_end(args);

  if (!(start= (char*) this->alloc_root(tot_length)))
    return(0);

  va_start(args, unused);
  res= start;
  while ((ptr= va_arg(args, char **)))
  {
    *ptr= res;
    length= va_arg(args, uint);
    res+= ALIGN_SIZE(length);
  }
  va_end(args);
  return((void*) start);
}

#define TRASH_MEM(X) TRASH(((char*)(X) + ((X)->size-(X)->left)), (X)->left)

/**
 * @brief
 * Mark all data in blocks free for reusage 
 */
void memory::Root::mark_blocks_free()
{
  memory::internal::UsedMemory *next;
  memory::internal::UsedMemory **last;

  /* iterate through (partially) free blocks, mark them free */
  last= &free;
  for (next= free; next; next= *(last= &next->next))
  {
    next->left= next->size - ALIGN_SIZE(sizeof(memory::internal::UsedMemory));
    TRASH_MEM(next);
  }

  /* Combine the free and the used list */
  *last= next= used;

  /* now go through the used blocks and mark them free */
  for (; next; next= next->next)
  {
    next->left= next->size - ALIGN_SIZE(sizeof(memory::internal::UsedMemory));
    TRASH_MEM(next);
  }

  /* Now everything is set; Indicate that nothing is used anymore */
  used= 0;
  first_block_usage= 0;
}

/**
 * @brief
 * Deallocate everything used by memory::alloc_root or just move
 * used blocks to free list if called with MY_USED_TO_FREE
 *
 * @note
 * One can call this function either with root block initialised with
 * init_alloc_root() or with a zero:ed block.
 * It's also safe to call this multiple times with the same mem_root.
 *
 * @param   root     Memory root
 * @param   MyFlags  Flags for what should be freed:
 *   @li   MARK_BLOCKS_FREED	Don't free blocks, just mark them free
 *   @li   KEEP_PREALLOC        If this is not set, then free also the
 *        		        preallocated block
 */
void memory::Root::free_root(myf MyFlags)
{
  memory::internal::UsedMemory *next,*old;

  if (MyFlags & memory::MARK_BLOCKS_FREE)
  {
    this->mark_blocks_free();
    return;
  }
  if (!(MyFlags & memory::KEEP_PREALLOC))
    this->pre_alloc=0;

  for (next=this->used; next ;)
  {
    old=next; next= next->next ;
    if (old != this->pre_alloc)
      std::free(old);
  }
  for (next=this->free ; next ;)
  {
    old=next; next= next->next;
    if (old != this->pre_alloc)
      std::free(old);
  }
  this->used=this->free=0;
  if (this->pre_alloc)
  {
    this->free=this->pre_alloc;
    this->free->left=this->pre_alloc->size-ALIGN_SIZE(sizeof(memory::internal::UsedMemory));
    TRASH_MEM(this->pre_alloc);
    this->free->next=0;
  }
  this->block_num= 4;
  this->first_block_usage= 0;
}

/**
 * @brief
 * Duplicate a null-terminated string into memory allocated from within the
 * specified Root
 */
char *memory::Root::strdup_root(const char *str)
{
  return strmake_root(str, strlen(str));
}

/**
 * @brief
 * Copy the (not necessarily null-terminated) string into memory allocated
 * from within the specified Root
 *
 * @details
 * Note that the string is copied according to the length specified, so
 * null-termination is ignored. The duplicated string will be null-terminated,
 * even if the original string wasn't (one additional byte is allocated for
 * this purpose).
 */
char *memory::Root::strmake_root(const char *str, size_t len)
{
  char *pos;
  if ((pos= (char *)alloc_root(len+1)))
  {
    memcpy(pos,str,len);
    pos[len]=0;
  }
  return pos;
}

/**
 * @brief
 * Duplicate the provided block into memory allocated from within the specified
 * Root
 *
 * @return
 * non-NULL pointer to a copy of the data if memory could be allocated, otherwise
 * NULL
 */
void *memory::Root::memdup_root(const void *str, size_t len)
{
  void *pos;

  if ((pos= this->alloc_root(len)))
    memcpy(pos,str,len);

  return pos;
}

} /* namespace drizzled */
