/* Copyright (C) 2000-2004, 2006 MySQL AB

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

#include "heap_priv.h"

#include <string.h>

using namespace drizzled;

int heap_rkey(HP_INFO *info, unsigned char *record, int inx, const unsigned char *key,
              key_part_map keypart_map, enum ha_rkey_function find_flag)
{
  unsigned char *pos;
  HP_SHARE *share= info->getShare();
  HP_KEYDEF *keyinfo= share->keydef + inx;

  if ((uint) inx >= share->keys)
  {
    return(errno= HA_ERR_WRONG_INDEX);
  }
  info->lastinx= inx;
  info->current_record= UINT32_MAX;		/* For heap_rrnd() */

  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    heap_rb_param custom_arg;

    custom_arg.keyseg= info->getShare()->keydef[inx].seg;
    custom_arg.key_length= info->lastkey_len=
      hp_rb_pack_key(keyinfo, &info->lastkey[0],
		     (unsigned char*) key, keypart_map);
    custom_arg.search_flag= SEARCH_FIND | SEARCH_SAME;
    /* for next rkey() after deletion */
    if (find_flag == HA_READ_AFTER_KEY)
      info->last_find_flag= HA_READ_KEY_OR_NEXT;
    else if (find_flag == HA_READ_BEFORE_KEY)
      info->last_find_flag= HA_READ_KEY_OR_PREV;
    else
      info->last_find_flag= find_flag;
    if (!(pos= (unsigned char *)tree_search_key(&keyinfo->rb_tree,
                                                &info->lastkey[0], info->parents,
			                        &info->last_pos,
                                                find_flag, &custom_arg)))
    {
      info->update= 0;
      return(errno= HA_ERR_KEY_NOT_FOUND);
    }
    memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos), sizeof(unsigned char*));
    info->current_ptr= pos;
  }
  else
  {
    if (!(pos= hp_search(info, share->keydef + inx, key, 0)))
    {
      info->update= 0;
      return(errno);
    }
    if (!(keyinfo->flag & HA_NOSAME))
      memcpy(&info->lastkey[0], key, (size_t) keyinfo->length);
  }
  hp_extract_record(share, record, pos);
  info->update= HA_STATE_AKTIV;
  return(0);
}


	/* Quick find of record */

unsigned char* heap_find(HP_INFO *info, int inx, const unsigned char *key)
{
  return hp_search(info, info->getShare()->keydef + inx, key, 0);
}
