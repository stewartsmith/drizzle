/* Copyright (C) 2000-2002, 2004-2005 MySQL AB

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

/* Update current record in heap-database */

#include "heap_priv.h"

using namespace drizzled;

int heap_update(HP_INFO *info, const unsigned char *old_record, const unsigned char *new_record)
{
  HP_KEYDEF *keydef, *end, *p_lastinx;
  unsigned char *pos;
  bool auto_key_changed= 0;
  HP_SHARE *share= info->getShare();
  uint32_t old_length, new_length;
  uint32_t old_chunk_count, new_chunk_count;

  test_active(info);
  pos=info->current_ptr;

  if (info->opt_flag & READ_CHECK_USED && hp_rectest(info,old_record))
    return(errno);				/* Record changed */

  old_length = hp_get_encoded_data_length(share, old_record, &old_chunk_count);
  new_length = hp_get_encoded_data_length(share, new_record, &new_chunk_count);

  if (new_chunk_count > old_chunk_count) {
    /* extend the old chunkset size as necessary, but do not shrink yet */
    if (hp_reallocate_chunkset(&share->recordspace, new_chunk_count, pos)) {
      return(errno);                          /* Out of memory or table space */
    }
  }

  if (--(share->records) < share->blength >> 1) share->blength>>= 1;
  share->changed=1;

  p_lastinx= share->keydef + info->lastinx;
  for (keydef= share->keydef, end= keydef + share->keys; keydef < end; keydef++)
  {
    if (hp_rec_key_cmp(keydef, old_record, new_record, 0))
    {
      if ((*keydef->delete_key)(info, keydef, old_record, pos, keydef == p_lastinx) ||
          (*keydef->write_key)(info, keydef, new_record, pos))
        goto err;
      if (share->auto_key == (uint) (keydef - share->keydef + 1))
        auto_key_changed= 1;
    }
  }
  hp_copy_record_data_to_chunkset(share, new_record, pos);
  if (++(share->records) == share->blength) share->blength+= share->blength;

  if (new_chunk_count < old_chunk_count) {
    /* Shrink the chunkset to its new size */
    hp_reallocate_chunkset(&share->recordspace, new_chunk_count, pos);
  }

  if (auto_key_changed)
    heap_update_auto_increment(info, new_record);
  return(0);

 err:
  if (errno == HA_ERR_FOUND_DUPP_KEY)
  {
    info->errkey = (int) (keydef - share->keydef);
    while (keydef >= share->keydef)
    {
      if (hp_rec_key_cmp(keydef, old_record, new_record, 0))
      {
	if ((*keydef->delete_key)(info, keydef, new_record, pos, 0) ||
	    (*keydef->write_key)(info, keydef, old_record, pos))
	  break;
      }
      keydef--;
    }
  }

  if (++(share->records) == share->blength)
    share->blength+= share->blength;

  if (new_chunk_count > old_chunk_count) {
    /* Shrink the chunkset to its original size */
    hp_reallocate_chunkset(&share->recordspace, old_chunk_count, pos);
  }

  return(errno);
} /* heap_update */
