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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "heap_priv.h"

#include <string.h>
#include <cassert>

using namespace drizzled;

/* Read first record with the current key */

int heap_rfirst(HP_INFO *info, unsigned char *record, int inx)
{
  HP_SHARE *share = info->getShare();
  HP_KEYDEF *keyinfo = share->keydef + inx;

  info->lastinx= inx;
  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    unsigned char *pos;

    if ((pos = (unsigned char *)tree_search_edge(&keyinfo->rb_tree,
                                                 info->parents,
                                                 &info->last_pos,
                                                 offsetof(TREE_ELEMENT, left))))
    {
      memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos),
	     sizeof(unsigned char*));
      info->current_ptr = pos;
      hp_extract_record(share, record, pos);
      /*
        If we're performing index_first on a table that was taken from
        table cache, info->lastkey_len is initialized to previous query.
        Thus we set info->lastkey_len to proper value for subsequent
        heap_rnext() calls.
        This is needed for DELETE queries only, otherwise this variable is
        not used.
        Note that the same workaround may be needed for heap_rlast(), but
        for now heap_rlast() is never used for DELETE queries.
      */
      info->lastkey_len= 0;
      info->update = HA_STATE_AKTIV;
    }
    else
    {
      errno = HA_ERR_END_OF_FILE;
      return(errno);
    }
    return(0);
  }
  else
  {
    if (!(info->getShare()->records))
    {
      errno=HA_ERR_END_OF_FILE;
      return(errno);
    }
    assert(0); /* TODO fix it */
    info->current_record=0;
    info->current_hash_ptr=0;
    info->update=HA_STATE_PREV_FOUND;
    return(heap_rnext(info,record));
  }
}
