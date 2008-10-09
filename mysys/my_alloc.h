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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* 
   Data structures for mysys/my_alloc.c (root memory allocator)
*/

#ifndef _my_alloc_h
#define _my_alloc_h

#if defined(__cplusplus)
extern "C" {
#endif

#define ALLOC_MAX_BLOCK_TO_DROP			4096
#define ALLOC_MAX_BLOCK_USAGE_BEFORE_DROP	10

typedef struct st_used_mem
{				   /* struct for once_alloc (block) */
  struct st_used_mem *next;	   /* Next block in use */
  unsigned int	left;		   /* memory left in block  */
  unsigned int	size;		   /* size of block */
} USED_MEM;


typedef struct st_mem_root
{
  USED_MEM *free;                  /* blocks with free memory in it */
  USED_MEM *used;                  /* blocks almost without free memory */
  USED_MEM *pre_alloc;             /* preallocated block */
  /* if block have less memory it will be put in 'used' list */
  size_t min_malloc;
  size_t block_size;               /* initial block size */
  unsigned int block_num;          /* allocated blocks counter */
  /* 
     first free block in queue test counter (if it exceed 
     MAX_BLOCK_USAGE_BEFORE_DROP block will be dropped in 'used' list)
  */
  unsigned int first_block_usage;

  void (*error_handler)(void);
} MEM_ROOT;

void init_alloc_root(MEM_ROOT *mem_root, size_t block_size,
                     size_t pre_alloc_size);
void *alloc_root(MEM_ROOT *mem_root, size_t Size);
void *multi_alloc_root(MEM_ROOT *mem_root, ...);
void free_root(MEM_ROOT *root, myf MyFLAGS);
void set_prealloc_root(MEM_ROOT *root, char *ptr);
void reset_root_defaults(MEM_ROOT *mem_root, size_t block_size,
                         size_t prealloc_size);
char *strdup_root(MEM_ROOT *root,const char *str);
char *strmake_root(MEM_ROOT *root,const char *str,size_t len);
void *memdup_root(MEM_ROOT *root,const void *str, size_t len);

#if defined(__cplusplus)
}
#endif
#endif
