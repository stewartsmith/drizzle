/* Copyright (C) 2000-2002, 2004-2006 MySQL AB

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

/* Write a record to heap-databas */

#include "heap_priv.h"
#ifdef __WIN__
#include <fcntl.h>
#endif

#include <drizzled/error_t.h>

#define LOWFIND 1
#define LOWUSED 2
#define HIGHFIND 4
#define HIGHUSED 8

using namespace drizzled;

static HASH_INFO *hp_find_free_hash(HP_SHARE *info, HP_BLOCK *block,
				     uint32_t records);

int heap_write(HP_INFO *info, const unsigned char *record)
{
  HP_KEYDEF *keydef, *end;
  unsigned char *pos;
  HP_SHARE *share=info->getShare();

  if ((share->records >= share->max_records && share->max_records) ||
    (share->recordspace.total_data_length + share->index_length >= share->max_table_size))
  {
    return(errno=HA_ERR_RECORD_FILE_FULL);
  }

  if (!(pos=hp_allocate_chunkset(&share->recordspace, 1)))
    return(errno);
  share->changed=1;

  for (keydef = share->keydef, end = keydef + share->keys; keydef < end;
       keydef++)
  {
    if (hp_write_key(info, keydef, record, pos))
      goto err;
  }

  hp_copy_record_data_to_chunkset(share, record, pos);

  if (++share->records == share->blength)
    share->blength+= share->blength;

  info->current_ptr=pos;
  info->current_hash_ptr=0;
  info->update|=HA_STATE_AKTIV;
  if (share->auto_key)
    heap_update_auto_increment(info, record);
  return(0);

err:
  info->errkey= keydef - share->keydef;
  while (keydef >= share->keydef)
  {
    if (hp_delete_key(info, keydef, record, pos, 0))
      break;
    keydef--;
  }

  hp_free_chunks(&share->recordspace, pos);

  return(errno);
} /* heap_write */

/*
  Write a hash-key to the hash-index
  SYNOPSIS
    info     Heap table info
    keyinfo  Key info
    record   Table record to added
    recpos   Memory buffer where the table record will be stored if added
             successfully
  NOTE
    Hash index uses HP_BLOCK structure as a 'growable array' of HASH_INFO
    structs. Array size == number of entries in hash index.
    hp_mask(hp_rec_hashnr()) maps hash entries values to hash array positions.
    If there are several hash entries with the same hash array position P,
    they are connected in a linked list via HASH_INFO::next_key. The first
    list element is located at position P, next elements are located at
    positions for which there is no record that should be located at that
    position. The order of elements in the list is arbitrary.

  RETURN
    0  - OK
    -1 - Out of memory
    HA_ERR_FOUND_DUPP_KEY - Duplicate record on unique key. The record was
    still added and the caller must call hp_delete_key for it.
*/

int hp_write_key(HP_INFO *info, HP_KEYDEF *keyinfo,
		 const unsigned char *record, unsigned char *recpos)
{
  HP_SHARE *share = info->getShare();
  int flag;
  uint32_t halfbuff,hashnr,first_index;
  unsigned char *ptr_to_rec= NULL,*ptr_to_rec2= NULL;
  HASH_INFO *empty, *gpos= NULL, *gpos2= NULL, *pos;

  flag=0;
  if (!(empty= hp_find_free_hash(share,&keyinfo->block,share->records)))
    return(-1);				/* No more memory */
  halfbuff= (long) share->blength >> 1;
  pos= hp_find_hash(&keyinfo->block,(first_index=share->records-halfbuff));

  /*
    We're about to add one more hash array position, with hash_mask=#records.
    The number of hash positions will change and some entries might need to
    be relocated to the newly added position. Those entries are currently
    members of the list that starts at #first_index position (this is
    guaranteed by properties of hp_mask(hp_rec_hashnr(X)) mapping function)
    At #first_index position currently there may be either:
    a) An entry with hashnr != first_index. We don't need to move it.
    or
    b) A list of items with hash_mask=first_index. The list contains entries
       of 2 types:
       1) entries that should be relocated to the list that starts at new
          position we're adding ('uppper' list)
       2) entries that should be left in the list starting at #first_index
          position ('lower' list)
  */
  if (pos != empty)				/* If some records */
  {
    do
    {
      hashnr = hp_rec_hashnr(keyinfo, pos->ptr_to_rec);
      if (flag == 0)
      {
        /*
          First loop, bail out if we're dealing with case a) from above
          comment
        */
	if (hp_mask(hashnr, share->blength, share->records) != first_index)
	  break;
      }
      /*
        flag & LOWFIND - found a record that should be put into lower position
        flag & LOWUSED - lower position occupied by the record
        Same for HIGHFIND and HIGHUSED and 'upper' position

        gpos  - ptr to last element in lower position's list
        gpos2 - ptr to last element in upper position's list

        ptr_to_rec - ptr to last entry that should go into lower list.
        ptr_to_rec2 - same for upper list.
      */
      if (!(hashnr & halfbuff))
      {
        /* Key should be put into 'lower' list */
	if (!(flag & LOWFIND))
	{
          /* key is the first element to go into lower position */
	  if (flag & HIGHFIND)
	  {
	    flag=LOWFIND | HIGHFIND;
	    /* key shall be moved to the current empty position */
	    gpos=empty;
	    ptr_to_rec=pos->ptr_to_rec;
	    empty=pos;				/* This place is now free */
	  }
	  else
	  {
            /*
              We can only get here at first iteration: key is at 'lower'
              position pos and should be left here.
            */
	    flag=LOWFIND | LOWUSED;
	    gpos=pos;
	    ptr_to_rec=pos->ptr_to_rec;
	  }
	}
	else
        {
          /* Already have another key for lower position */
	  if (!(flag & LOWUSED))
	  {
	    /* Change link of previous lower-list key */
	    gpos->ptr_to_rec=ptr_to_rec;
	    gpos->next_key=pos;
	    flag= (flag & HIGHFIND) | (LOWFIND | LOWUSED);
	  }
	  gpos=pos;
	  ptr_to_rec=pos->ptr_to_rec;
	}
      }
      else
      {
        /* key will be put into 'higher' list */
	if (!(flag & HIGHFIND))
	{
	  flag= (flag & LOWFIND) | HIGHFIND;
	  /* key shall be moved to the last (empty) position */
	  gpos2= empty;
          empty= pos;
	  ptr_to_rec2=pos->ptr_to_rec;
	}
	else
	{
	  if (!(flag & HIGHUSED))
	  {
	    /* Change link of previous upper-list key and save */
	    gpos2->ptr_to_rec=ptr_to_rec2;
	    gpos2->next_key=pos;
	    flag= (flag & LOWFIND) | (HIGHFIND | HIGHUSED);
	  }
	  gpos2=pos;
	  ptr_to_rec2=pos->ptr_to_rec;
	}
      }
    }
    while ((pos=pos->next_key));

    if ((flag & (LOWFIND | HIGHFIND)) == (LOWFIND | HIGHFIND))
    {
      /*
        If both 'higher' and 'lower' list have at least one element, now
        there are two hash buckets instead of one.
      */
      keyinfo->hash_buckets++;
    }

    if ((flag & (LOWFIND | LOWUSED)) == LOWFIND)
    {
      gpos->ptr_to_rec=ptr_to_rec;
      gpos->next_key=0;
    }
    if ((flag & (HIGHFIND | HIGHUSED)) == HIGHFIND)
    {
      gpos2->ptr_to_rec=ptr_to_rec2;
      gpos2->next_key=0;
    }
  }
  /* Check if we are at the empty position */

  pos=hp_find_hash(&keyinfo->block, hp_mask(hp_rec_hashnr(keyinfo, record),
					 share->blength, share->records + 1));
  if (pos == empty)
  {
    pos->ptr_to_rec=recpos;
    pos->next_key=0;
    keyinfo->hash_buckets++;
  }
  else
  {
    /* Check if more records in same hash-nr family */
    empty[0]=pos[0];
    gpos=hp_find_hash(&keyinfo->block,
		      hp_mask(hp_rec_hashnr(keyinfo, pos->ptr_to_rec),
			      share->blength, share->records + 1));
    if (pos == gpos)
    {
      pos->ptr_to_rec=recpos;
      pos->next_key=empty;
    }
    else
    {
      keyinfo->hash_buckets++;
      pos->ptr_to_rec=recpos;
      pos->next_key=0;
      hp_movelink(pos, gpos, empty);
    }

    /* Check if duplicated keys */
    if ((keyinfo->flag & HA_NOSAME) && pos == gpos &&
	(!(keyinfo->flag & HA_NULL_PART_KEY) ||
	 !hp_if_null_in_key(keyinfo, record)))
    {
      pos=empty;
      do
      {
	if (! hp_rec_key_cmp(keyinfo, record, pos->ptr_to_rec, 1))
	{
	  return(errno=HA_ERR_FOUND_DUPP_KEY);
	}
      } while ((pos=pos->next_key));
    }
  }
  return(0);
}

	/* Returns ptr to block, and allocates block if neaded */

static HASH_INFO *hp_find_free_hash(HP_SHARE *info,
				     HP_BLOCK *block, uint32_t records)
{
  uint32_t block_pos;
  size_t length;

  if (records < block->last_allocated)
    return hp_find_hash(block,records);
  if (!(block_pos=(records % block->records_in_block)))
  {
    if (hp_get_new_block(block,&length))
      return(NULL);
    info->index_length+=length;
  }
  block->last_allocated=records+1;
  return((HASH_INFO*) ((unsigned char*) block->level_info[0].last_blocks+
		       block_pos*block->recbuffer));
}
