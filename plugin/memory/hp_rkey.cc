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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "heap_priv.h"

#include <string.h>
#include <drizzled/error_t.h>

using namespace drizzled;

int heap_rkey(HP_INFO *info, unsigned char *record, int inx, const unsigned char *key,
              key_part_map , enum ha_rkey_function )
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
