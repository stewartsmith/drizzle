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

	/* Read prev record for key */


int heap_rprev(HP_INFO *info, unsigned char *record)
{
  unsigned char *pos;
  HP_SHARE *share=info->getShare();

  if (info->lastinx < 0)
    return(errno=HA_ERR_WRONG_INDEX);
  {
    if (info->current_ptr || (info->update & HA_STATE_NEXT_FOUND))
    {
      if ((info->update & HA_STATE_DELETED))
        pos= hp_search(info, share->keydef + info->lastinx, &info->lastkey[0], 3);
      else
        pos= hp_search(info, share->keydef + info->lastinx, &info->lastkey[0], 2);
    }
    else
    {
      pos=0;					/* Read next after last */
      errno=HA_ERR_KEY_NOT_FOUND;
    }
  }
  if (!pos)
  {
    info->update=HA_STATE_PREV_FOUND;		/* For heap_rprev */
    if (errno == HA_ERR_KEY_NOT_FOUND)
      errno=HA_ERR_END_OF_FILE;
    return(errno);
  }
  hp_extract_record(share, record, pos);
  info->update=HA_STATE_AKTIV | HA_STATE_PREV_FOUND;
  return(0);
}
