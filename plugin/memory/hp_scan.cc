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

/* Scan through all rows */

#include "heap_priv.h"
#include <drizzled/error_t.h>

/*
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int heap_scan_init(register HP_INFO *info)
{
  info->lastinx= -1;
  info->current_record= UINT32_MAX;		/* No current record */
  info->update=0;
  info->next_block=0;
  return(0);
}

int heap_scan(register HP_INFO *info, unsigned char *record)
{
  HP_SHARE *share=info->getShare();
  uint32_t pos;

  pos= ++info->current_record;
  if (pos < info->next_block)
  {
    info->current_ptr+=share->recordspace.block.recbuffer;
  }
  else
  {
    info->next_block+=share->recordspace.block.records_in_block;
    if (info->next_block >= share->recordspace.chunk_count)
    {
      info->next_block= share->recordspace.chunk_count;
      if (pos >= info->next_block)
      {
	info->update= 0;
	return(errno=  drizzled::HA_ERR_END_OF_FILE);
      }
    }
    hp_find_record(info, pos);
  }
  if (get_chunk_status(&share->recordspace, info->current_ptr) != CHUNK_STATUS_ACTIVE)
  {
    info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    return(errno= drizzled::HA_ERR_RECORD_DELETED);
  }
  info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND | HA_STATE_AKTIV;
  hp_extract_record(share, record, info->current_ptr);
  info->current_hash_ptr=0;			/* Can't use read_next */
  return(0);
} /* heap_scan */
