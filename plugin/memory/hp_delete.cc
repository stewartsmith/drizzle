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

/* remove current record in heap-database */

#include "heap_priv.h"
#include <drizzled/error_t.h>
#include <drizzled/internal/my_sys.h>

int heap_delete(HP_INFO *info, const unsigned char *record)
{
  unsigned char *pos;
  HP_SHARE *share=info->getShare();
  HP_KEYDEF *keydef, *end, *p_lastinx;

  test_active(info);

  if (info->opt_flag & READ_CHECK_USED)
    return(errno);   /* Record changed */
  share->changed=1;

  if ( --(share->records) < share->blength >> 1) share->blength>>=1;
  pos=info->current_ptr;

  p_lastinx = share->keydef + info->lastinx;
  for (keydef = share->keydef, end = keydef + share->keys; keydef < end;
       keydef++)
  {
    if (hp_delete_key(info, keydef, record, pos, keydef == p_lastinx))
      goto err;
  }

  info->update=HA_STATE_DELETED;
  hp_free_chunks(&share->recordspace, pos);
  info->current_hash_ptr=0;

  return(0);
err:
  if (++(share->records) == share->blength)
    share->blength+= share->blength;
  return(errno);
}


/*
  Remove one key from hash-table

  SYNPOSIS
    hp_delete_key()
    info		Hash handler
    keyinfo		key definition of key that we want to delete
    record		row data to be deleted
    recpos		Pointer to heap record in memory
    flag		Is set if we want's to correct info->current_ptr

  RETURN
    0      Ok
    other  Error code
*/

int hp_delete_key(HP_INFO *info, register HP_KEYDEF *keyinfo,
		  const unsigned char *record, unsigned char *recpos, int flag)
{
  uint32_t blength,pos2,pos_hashnr,lastpos_hashnr;
  HASH_INFO *lastpos,*gpos,*pos,*pos3,*empty,*last_ptr;
  HP_SHARE *share=info->getShare();

  blength=share->blength;
  if (share->records+1 == blength)
    blength+= blength;

  /* find the very last HASH_INFO pointer in the index */
  /* note that records has already been decremented */
  lastpos=hp_find_hash(&keyinfo->block,share->records);
  last_ptr=0;

  /* Search after record with key */
  pos= hp_find_hash(&keyinfo->block,
		    hp_mask(hp_rec_hashnr(keyinfo, record), blength,
			    share->records + 1));
  gpos = pos3 = 0;

  while (pos->ptr_to_rec != recpos)
  {
    if (flag && !hp_rec_key_cmp(keyinfo, record, pos->ptr_to_rec, 0))
      last_ptr=pos;				/* Previous same key */
    gpos=pos;
    if (!(pos=pos->next_key))
    {
      return(errno= drizzled::HA_ERR_CRASHED);	/* This shouldn't happend */
    }
  }

  /* Remove link to record */

  if (flag)
  {
    /* Save for heap_rnext/heap_rprev */
    info->current_hash_ptr=last_ptr;
    info->current_ptr = last_ptr ? last_ptr->ptr_to_rec : 0;
  }
  empty=pos;
  if (gpos) {
    /* gpos says we have previous HASH_INFO, change previous to point to next, this way unlinking "empty" */
    gpos->next_key=pos->next_key;
  }
  else if (pos->next_key)
  {
    /* no previous gpos, this pos is the first in the list and it has pointer to "next" */
    /* move next HASH_INFO data to our pos, to free up space at the next position */
    /* remember next pos as "empty", nobody refers to "empty" at this point */
    empty=pos->next_key;
    pos->ptr_to_rec=empty->ptr_to_rec;
    pos->next_key=empty->next_key;
  }
  else
  {
    /* this was the only HASH_INFO at this position */
    keyinfo->hash_buckets--;
  }

  if (empty == lastpos)			/* deleted last hash key */
    return (0);

  /* Move the last key (lastpos) */
  lastpos_hashnr = hp_rec_hashnr(keyinfo, lastpos->ptr_to_rec);
  /* pos is where lastpos should be */
  pos=hp_find_hash(&keyinfo->block, hp_mask(lastpos_hashnr, share->blength,
					    share->records));
  if (pos == empty)			/* Move to empty position. */
  {
    empty[0]=lastpos[0];
    return(0);
  }
  pos_hashnr = hp_rec_hashnr(keyinfo, pos->ptr_to_rec);
  /* pos3 is where the pos should be */
  pos3= hp_find_hash(&keyinfo->block,
		     hp_mask(pos_hashnr, share->blength, share->records));
  if (pos != pos3)
  {					/* pos is on wrong posit */
    empty[0]=pos[0];			/* Save it here */
    pos[0]=lastpos[0];			/* This shold be here */
    hp_movelink(pos, pos3, empty);	/* Fix link to pos */
    return(0);
  }
  pos2= hp_mask(lastpos_hashnr, blength, share->records + 1);
  if (pos2 == hp_mask(pos_hashnr, blength, share->records + 1))
  {					/* Identical key-positions */
    if (pos2 != share->records)
    {
      empty[0]=lastpos[0];
      hp_movelink(lastpos, pos, empty);
      return(0);
    }
    pos3= pos;				/* Link pos->next after lastpos */
  }
  else
  {
    pos3= 0;				/* Different positions merge */
    keyinfo->hash_buckets--;
  }

  empty[0]=lastpos[0];
  hp_movelink(pos3, empty, pos->next_key);
  pos->next_key=empty;
  return(0);
}
