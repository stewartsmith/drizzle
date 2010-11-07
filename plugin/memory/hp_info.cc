/* Copyright (C) 2000-2004 MySQL AB

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

/* Returns info about database status */

#include "heap_priv.h"


unsigned char *heap_position(HP_INFO *info)
{
  return ((info->update & HA_STATE_AKTIV) ? info->current_ptr :
	  (HEAP_PTR) 0);
}

/* Note that heap_info does NOT return information about the
   current position anymore;  Use heap_position instead */

int heap_info(register HP_INFO *info, register HEAPINFO *x, int flag )
{
  x->records         = info->getShare()->records;
  x->deleted         = info->getShare()->recordspace.del_chunk_count;

  x->reclength     = info->getShare()->recordspace.chunk_dataspace_length;

  x->data_length     = info->getShare()->recordspace.total_data_length;
  x->index_length    = info->getShare()->index_length;
  x->max_records     = info->getShare()->max_records;
  x->errkey          = info->errkey;
  if (flag & HA_STATUS_AUTO)
    x->auto_increment= info->getShare()->auto_increment + 1;
  return(0);
} /* heap_info */
