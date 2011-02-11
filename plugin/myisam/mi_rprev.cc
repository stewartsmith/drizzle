/* Copyright (C) 2000-2001, 2004 MySQL AB

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

#include "myisam_priv.h"

	/*
	   Read previous row with the same key as previous read
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int mi_rprev(MI_INFO *info, unsigned char *buf, int inx)
{
  int error,changed;
  register uint32_t flag;
  MYISAM_SHARE *share=info->s;

  if ((inx = _mi_check_index(info,inx)) < 0)
    return(errno);
  flag=SEARCH_SMALLER;				/* Read previous */
  if (info->lastpos == HA_OFFSET_ERROR && info->update & HA_STATE_NEXT_FOUND)
    flag=0;					/* Read last */

  if (fast_mi_readinfo(info))
    return(errno);
  changed=_mi_test_if_changed(info);
  if (!flag)
    error=_mi_search_last(info, share->keyinfo+inx,
			  share->state.key_root[inx]);
  else if (!changed)
    error=_mi_search_next(info,share->keyinfo+inx,info->lastkey,
			  info->lastkey_length,flag,
			  share->state.key_root[inx]);
  else
    error=_mi_search(info,share->keyinfo+inx,info->lastkey,
		     USE_WHOLE_KEY, flag, share->state.key_root[inx]);

  if (share->concurrent_insert)
  {
    if (!error)
    {
      while (info->lastpos >= info->state->data_file_length)
      {
	/* Skip rows that are inserted by other threads since we got a lock */
	if  ((error=_mi_search_next(info,share->keyinfo+inx,info->lastkey,
				    info->lastkey_length,
				    SEARCH_SMALLER,
				    share->state.key_root[inx])))
	  break;
      }
    }
  }
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= HA_STATE_PREV_FOUND;
  if (error)
  {
    if (errno ==  drizzled::HA_ERR_KEY_NOT_FOUND)
      errno= drizzled::HA_ERR_END_OF_FILE;
  }
  else if (!buf)
  {
    return(info->lastpos==HA_OFFSET_ERROR ? errno : 0);
  }
  else if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    return(0);
  }
  return(errno);
} /* mi_rprev */
