/* Copyright (C) 2000-2002 MySQL AB

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

/* Read next record with the same key */

int heap_rnext(HP_INFO *info, unsigned char *record)
{
  unsigned char *pos;
  HP_SHARE *share=info->getShare();
  HP_KEYDEF *keyinfo;

  if (info->lastinx < 0)
    return(errno=HA_ERR_WRONG_INDEX);

  keyinfo = share->keydef + info->lastinx;
  {
    if (info->current_hash_ptr)
      pos= hp_search_next(info, keyinfo, &info->lastkey[0],
			   info->current_hash_ptr);
    else
    {
      if (!info->current_ptr && (info->update & HA_STATE_NEXT_FOUND))
      {
	pos=0;					/* Read next after last */
	errno=HA_ERR_KEY_NOT_FOUND;
      }
      else if (!info->current_ptr)		/* Deleted or first call */
	pos= hp_search(info, keyinfo, &info->lastkey[0], 0);
      else
	pos= hp_search(info, keyinfo, &info->lastkey[0], 1);
    }
  }
  if (!pos)
  {
    info->update=HA_STATE_NEXT_FOUND;		/* For heap_rprev */
    if (errno == HA_ERR_KEY_NOT_FOUND)
      errno=HA_ERR_END_OF_FILE;
    return(errno);
  }
  hp_extract_record(share, record, pos);
  info->update=HA_STATE_AKTIV | HA_STATE_NEXT_FOUND;
  return(0);
}
