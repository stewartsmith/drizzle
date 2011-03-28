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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* Update current record in heap-database */

#include "heap_priv.h"
#include <drizzled/error_t.h>

using namespace drizzled;

int heap_update(HP_INFO *info, const unsigned char *old_record, const unsigned char *new_record)
{
  HP_KEYDEF *keydef, *end, *p_lastinx;
  unsigned char *pos;
  bool auto_key_changed= 0;
  HP_SHARE *share= info->getShare();

  test_active(info);
  pos=info->current_ptr;

  if (info->opt_flag & READ_CHECK_USED && hp_rectest(info,old_record))
    return(errno);				/* Record changed */

  if (--(share->records) < share->blength >> 1) share->blength>>= 1;
  share->changed=1;

  p_lastinx= share->keydef + info->lastinx;
  for (keydef= share->keydef, end= keydef + share->keys; keydef < end; keydef++)
  {
    if (hp_rec_key_cmp(keydef, old_record, new_record, 0))
    {
      if (hp_delete_key(info, keydef, old_record, pos, keydef == p_lastinx) ||
          hp_write_key(info, keydef, new_record, pos))
      {
        goto err;
      }
      if (share->auto_key == (uint) (keydef - share->keydef + 1))
        auto_key_changed= 1;
    }
  }
  hp_copy_record_data_to_chunkset(share, new_record, pos);
  if (++(share->records) == share->blength) share->blength+= share->blength;

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
	if (hp_delete_key(info, keydef, new_record, pos, 0) ||
	    hp_write_key(info, keydef, old_record, pos))
	  break;
      }
      keydef--;
    }
  }

  if (++(share->records) == share->blength)
    share->blength+= share->blength;

  return(errno);
} /* heap_update */
