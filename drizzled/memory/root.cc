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

#include <config.h>

#include <drizzled/definitions.h>
#include <drizzled/memory/root.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/sql_string.h>

#include <algorithm>

using namespace std;

namespace drizzled {
namespace memory {

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
 * reported as error in first alloc() on this memory root.
 *
 * @param  mem_root       memory root to initialize
 * @param  block_size     size of chunks (blocks) used for memory allocation
 *                       (It is external size of chunk i.e. it should include
 *                      memory required for internal structures, thus it
 *                      should be no less than ROOT_MIN_BLOCK_SIZE)
 *
 */
void Root::init(size_t block_size_arg)
{
  free= used= pre_alloc= 0;
  min_malloc= 32;
  block_size= block_size_arg - ROOT_MIN_BLOCK_SIZE;
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
void Root::reset_defaults(size_t block_size_arg, size_t pre_alloc_size)
{
  block_size= block_size_arg - ROOT_MIN_BLOCK_SIZE;
  if (pre_alloc_size)
  {
    size_t size= pre_alloc_size + ALIGN_SIZE(sizeof(internal::UsedMemory));
    if (not pre_alloc || pre_alloc->size != size)
    {
      internal::UsedMemory *mem, **prev= &this->free;
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
        if (mem->left + ALIGN_SIZE(sizeof(internal::UsedMemory)) == mem->size)
        {
          /* remove block from the list and free it */
          *prev= mem->next;
          std::free(mem);
        }
        else
          prev= &mem->next;
      }
      /* Allocate new prealloc block and add it to the end of free list */
      mem= static_cast<internal::UsedMemory *>(malloc(size));
      mem->size= size;
      mem->left= pre_alloc_size;
      mem->next= *prev;
      *prev= pre_alloc= mem;
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
 * mem_root must have been initialised via init()
 *
 * @param  mem_root  The memory Root to allocate from
 * @param  length    The size of the block to allocate
 *
 * @todo Would this be more suitable as a member function on the
 * Root class?
 */
unsigned char* Root::alloc(size_t length)
{
  internal::UsedMemory *next= NULL;
  assert(alloc_root_inited());

  length= ALIGN_SIZE(length);
  internal::UsedMemory **prev= &this->free;
  if (*prev)
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
    for (next= *prev; next && next->left < length; next= next->next)
      prev= &next->next;
  }
  if (! next)
  {						/* Time to alloc new block */
    size_t tmp_block_size= this->block_size * (this->block_num >> 2);
    size_t get_size= length+ALIGN_SIZE(sizeof(internal::UsedMemory));
    get_size= max(get_size, tmp_block_size);

    next = static_cast<internal::UsedMemory *>(malloc(get_size));
    this->block_num++;
    next->next= *prev;
    next->size= get_size;
    next->left= get_size-ALIGN_SIZE(sizeof(internal::UsedMemory));
    *prev=next;
  }

  unsigned char* point= (unsigned char*) ((char*) next+ (next->size-next->left));
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
 * @li <tt>char* * pointer1</tt>
 * @li <tt>uint length1</tt>
 * @li <tt>char* * pointer2</tt>
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
void* Root::multi_alloc(int unused, ...)
{
  va_list args;
  char* *ptr, *start, *res;
  size_t tot_length, length;

  (void)unused; // For some reason Sun Studio registers unused as not used.
  va_start(args, unused);
  tot_length= 0;
  while ((ptr= va_arg(args, char* *)))
  {
    length= va_arg(args, uint);
    tot_length+= ALIGN_SIZE(length);
  }
  va_end(args);

  start= (char*) this->alloc(tot_length);

  va_start(args, unused);
  res= start;
  while ((ptr= va_arg(args, char* *)))
  {
    *ptr= res;
    length= va_arg(args, uint);
    res+= ALIGN_SIZE(length);
  }
  va_end(args);
  return((void*) start);
}

/**
 * @brief
 * Mark all data in blocks free for reusage 
 */
void Root::mark_blocks_free()
{
  internal::UsedMemory *next;
  internal::UsedMemory **last;

  /* iterate through (partially) free blocks, mark them free */
  last= &free;
  for (next= free; next; next= *(last= &next->next))
  {
    next->left= next->size - ALIGN_SIZE(sizeof(internal::UsedMemory));
  }

  /* Combine the free and the used list */
  *last= next= used;

  /* now go through the used blocks and mark them free */
  for (; next; next= next->next)
  {
    next->left= next->size - ALIGN_SIZE(sizeof(internal::UsedMemory));
  }

  /* Now everything is set; Indicate that nothing is used anymore */
  used= 0;
  first_block_usage= 0;
}

/**
 * @brief
 * Deallocate everything used by alloc_root or just move
 * used blocks to free list if called with MY_USED_TO_FREE
 *
 * @note
 * One can call this function either with root block initialised with
 * init() or with a zero:ed block.
 * It's also safe to call this multiple times with the same mem_root.
 *
 * @param   root     Memory root
 * @param   MyFlags  Flags for what should be freed:
 *   @li   MARK_BLOCKS_FREED	Don't free blocks, just mark them free
 *   @li   KEEP_PREALLOC        If this is not set, then free also the
 *        		        preallocated block
 */
void Root::free_root(myf MyFlags)
{
  if (MyFlags & MARK_BLOCKS_FREE)
  {
    this->mark_blocks_free();
    return;
  }
  if (!(MyFlags & KEEP_PREALLOC))
    this->pre_alloc=0;

  for (internal::UsedMemory* next= this->used; next;)
  {
    internal::UsedMemory* old =next; 
    next= next->next;
    if (old != this->pre_alloc)
      std::free(old);
  }
  for (internal::UsedMemory* next=this->free; next;)
  {
    internal::UsedMemory* old= next; 
    next= next->next;
    if (old != this->pre_alloc)
      std::free(old);
  }
  this->used=this->free=0;
  if (this->pre_alloc)
  {
    this->free=this->pre_alloc;
    this->free->left=this->pre_alloc->size-ALIGN_SIZE(sizeof(internal::UsedMemory));
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
char* Root::strdup(const char* str)
{
  return strmake(str, strlen(str));
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
char* Root::strmake(const char* str, size_t len)
{
  char* pos= (char*)alloc(len + 1);
  memcpy(pos, str, len);
  pos[len]= 0;
  return pos;
}

char* Root::strmake(const std::string& v)
{
  return strmake(v.data(), v.size());
}

char* Root::strmake(const String& v)
{
  return strmake(v.ptr(), v.length());
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
void* Root::memdup(const void* str, size_t len)
{
  void* pos= alloc(len);
  memcpy(pos, str, len);
  return pos;
}

}
} /* namespace drizzled */
