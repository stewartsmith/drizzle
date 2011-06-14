/* Copyright (C) 2000-2002, 2004 MySQL AB

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

/* functions on blocks; Keys and records are saved in blocks */

#include "heap_priv.h"

#include <cstdlib>

/*
  Find record according to record-position.

  The record is located by factoring position number pos into (p_0, p_1, ...)
  such that
     pos = SUM_i(block->level_info[i].records_under_level * p_i)
  {p_0, p_1, ...} serve as indexes to descend the blocks tree.
*/

unsigned char *hp_find_block(HP_BLOCK *block, uint32_t pos)
{
  int i;
  HP_PTRS *ptr; /* block base ptr */

  for (i= block->levels-1, ptr=block->root ; i > 0 ; i--)
  {
    ptr=(HP_PTRS*)ptr->blocks[pos/block->level_info[i].records_under_level];
    pos%=block->level_info[i].records_under_level;
  }
  return (unsigned char*) ptr+ pos*block->recbuffer;
}


/*
  Get one new block-of-records. Alloc ptr to block if needed

  SYNOPSIS
    hp_get_new_block()
      block             HP_BLOCK tree-like block
      alloc_length OUT  Amount of memory allocated from the heap

  Interrupts are stopped to allow ha_panic in interrupts
  RETURN
    0  OK
    1  Out of memory
*/

int hp_get_new_block(HP_BLOCK *block, size_t *alloc_length)
{
  uint32_t i;
  HP_PTRS *root;

  for (i=0 ; i < block->levels ; i++)
    if (block->level_info[i].free_ptrs_in_block)
      break;

  /*
    Allocate space for leaf block plus space for upper level blocks up to
    first level that has a free slot to put the pointer.
    In some cases we actually allocate more then we need:
    Consider e.g. a situation where we have one level 1 block and one level 0
    block, the level 0 block is full and this function is called. We only
    need a leaf block in this case. Nevertheless, we will get here with i=1
    and will also allocate sizeof(HP_PTRS) for non-leaf block and will never
    use this space.
    This doesn't add much overhead - with current values of sizeof(HP_PTRS)
    and my_default_record_cache_size we get about 1/128 unused memory.
   */
  *alloc_length=sizeof(HP_PTRS)*i+block->records_in_block* block->recbuffer;
  root=(HP_PTRS*) malloc(*alloc_length);

  if (i == 0)
  {
    block->levels=1;
    block->root=block->level_info[0].last_blocks=root;
  }
  else
  {
    if ((uint) i == block->levels)
    {
      /* Adding a new level on top of the existing ones. */
      block->levels=i+1;
      /*
        Use first allocated HP_PTRS as a top-level block. Put the current
        block tree into the first slot of a new top-level block.
      */
      block->level_info[i].free_ptrs_in_block=HP_PTRS_IN_NOD-1;
      ((HP_PTRS**) root)[0]= block->root;
      block->root=block->level_info[i].last_blocks= root++;
    }
    /* Occupy the free slot we've found at level i */
    block->level_info[i].last_blocks->
      blocks[HP_PTRS_IN_NOD - block->level_info[i].free_ptrs_in_block--]=
	(unsigned char*) root;

    /* Add a block subtree with each node having one left-most child */
    for (uint32_t j= i-1 ; j >0 ; j--)
    {
      block->level_info[j].last_blocks= root++;
      block->level_info[j].last_blocks->blocks[0]=(unsigned char*) root;
      block->level_info[j].free_ptrs_in_block=HP_PTRS_IN_NOD-1;
    }

    /*
      root now points to last (block->records_in_block* block->recbuffer)
      allocated bytes. Use it as a leaf block.
    */
    block->level_info[0].last_blocks= root;
  }
  return 0;
}


	/* free all blocks under level */

unsigned char *hp_free_level(HP_BLOCK *block, uint32_t level, HP_PTRS *pos, unsigned char *last_pos)
{
  int max_pos;
  unsigned char *next_ptr;

  if (level == 1)
  {
    next_ptr=(unsigned char*) pos+block->recbuffer;
  }
  else
  {
    max_pos= (block->level_info[level-1].last_blocks == pos) ?
      HP_PTRS_IN_NOD - block->level_info[level-1].free_ptrs_in_block :
    HP_PTRS_IN_NOD;

    next_ptr=(unsigned char*) (pos+1);
    for (int i= 0; i < max_pos ; i++)
      next_ptr=hp_free_level(block,level-1,
			      (HP_PTRS*) pos->blocks[i],next_ptr);
  }

  if ((unsigned char*) pos != last_pos)
  {
    free((unsigned char*) pos);
    return last_pos;
  }
  return next_ptr;			/* next memory position */
}
