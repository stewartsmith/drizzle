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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "heap_priv.h"

#include <string.h>

	/* Read first record with the current key */

using namespace drizzled;

int heap_rlast(HP_INFO *info, unsigned char *record, int inx)
{
  HP_SHARE *share=    info->getShare();
  HP_KEYDEF *keyinfo= share->keydef + inx;

  info->lastinx= inx;
  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    unsigned char *pos;

    if ((pos = (unsigned char *)tree_search_edge(&keyinfo->rb_tree,
                                                 info->parents,
                                                 &info->last_pos,
                                                 offsetof(TREE_ELEMENT, right))))
    {
      memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos),
	     sizeof(unsigned char*));
      info->current_ptr = pos;
      hp_extract_record(share, record, pos);
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
    info->current_ptr=0;
    info->current_hash_ptr=0;
    info->update=HA_STATE_NEXT_FOUND;
    return(heap_rprev(info,record));
  }
}
