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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "heap_priv.h"

#include <string.h>
#include <cassert>
#include <drizzled/error_t.h>


using namespace drizzled;

/* Read first record with the current key */

int heap_rfirst(HP_INFO *info, unsigned char *record, int inx)
{
  info->lastinx= inx;
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
