/* Copyright (C) 2000-2006 MySQL AB

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

/* Update an old row in a MyISAM table */

#include "myisam_priv.h"
#include <drizzled/util/test.h>

using namespace drizzled;

int mi_update(register MI_INFO *info, const unsigned char *oldrec, unsigned char *newrec)
{
  int flag,key_changed,save_errno;
  register internal::my_off_t pos;
  uint32_t i;
  unsigned char old_key[MI_MAX_KEY_BUFF],*new_key;
  bool auto_key_changed=0;
  uint64_t changed;
  MYISAM_SHARE *share= info->s;
  ha_checksum old_checksum= 0;

  if (!(info->update & HA_STATE_AKTIV))
  {
    return(errno=HA_ERR_KEY_NOT_FOUND);
  }
  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    return(errno=EACCES);
  }
  if (info->state->key_file_length >= share->base.margin_key_file_length)
  {
    return(errno=HA_ERR_INDEX_FILE_FULL);
  }
  pos=info->lastpos;
  if (_mi_readinfo(info,F_WRLCK,1))
    return(errno);

  if (share->calc_checksum)
    old_checksum=info->checksum=(*share->calc_checksum)(info,oldrec);
  if ((*share->compare_record)(info,oldrec))
  {
    save_errno=errno;
    goto err_end;			/* Record has changed */
  }


  /* Calculate and check all unique constraints */
  key_changed=0;
  for (i=0 ; i < share->state.header.uniques ; i++)
  {
    MI_UNIQUEDEF *def=share->uniqueinfo+i;
    if (mi_unique_comp(def, newrec, oldrec,1) &&
	mi_check_unique(info, def, newrec, mi_unique_hash(def, newrec),
			info->lastpos))
    {
      save_errno=errno;
      goto err_end;
    }
  }
  if (_mi_mark_file_changed(info))
  {
    save_errno=errno;
    goto err_end;
  }

  /* Check which keys changed from the original row */

  new_key=info->lastkey2;
  changed=0;
  for (i=0 ; i < share->base.keys ; i++)
  {
    if (mi_is_key_active(share->state.key_map, i))
    {
      {
	uint32_t new_length=_mi_make_key(info,i,new_key,newrec,pos);
	uint32_t old_length=_mi_make_key(info,i,old_key,oldrec,pos);

        /* The above changed info->lastkey2. Inform mi_rnext_same(). */
        info->update&= ~HA_STATE_RNEXT_SAME;

	if (new_length != old_length ||
	    memcmp(old_key,new_key,new_length))
	{
	  if ((int) i == info->lastinx)
	    key_changed|=HA_STATE_WRITTEN;	/* Mark that keyfile changed */
	  changed|=((uint64_t) 1 << i);
	  share->keyinfo[i].version++;
	  if (share->keyinfo[i].ck_delete(info,i,old_key,old_length)) goto err;
	  if (share->keyinfo[i].ck_insert(info,i,new_key,new_length)) goto err;
	  if (share->base.auto_key == i+1)
	    auto_key_changed=1;
	}
      }
    }
  }
  /*
    If we are running with external locking, we must update the index file
    that something has changed.
  */
  if (changed)
    key_changed|= HA_STATE_CHANGED;

  if (share->calc_checksum)
  {
    info->checksum=(*share->calc_checksum)(info,newrec);
    /* Store new checksum in index file header */
    key_changed|= HA_STATE_CHANGED;
  }
  {
    /*
      Don't update index file if data file is not extended and no status
      information changed
    */
    MI_STATUS_INFO state;
    ha_rows org_split;
    internal::my_off_t org_delete_link;

    memcpy(&state, info->state, sizeof(state));
    org_split=	     share->state.split;
    org_delete_link= share->state.dellink;
    if ((*share->update_record)(info,pos,newrec))
      goto err;
    if (!key_changed &&
	(memcmp(&state, info->state, sizeof(state)) ||
	 org_split != share->state.split ||
	 org_delete_link != share->state.dellink))
      key_changed|= HA_STATE_CHANGED;		/* Must update index file */
  }
  if (auto_key_changed)
    set_if_bigger(info->s->state.auto_increment,
                  retrieve_auto_increment(info, newrec));
  if (share->calc_checksum)
    info->state->checksum+=(info->checksum - old_checksum);

  info->update= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED | HA_STATE_AKTIV |
		 key_changed);
  /*
    Every myisam function that updates myisam table must end with
    call to _mi_writeinfo(). If operation (second param of
    _mi_writeinfo()) is not 0 it sets share->changed to 1, that is
    flags that data has changed. If operation is 0, this function
    equals to no-op in this case.

    mi_update() must always pass !0 value as operation, since even if
    there is no index change there could be data change.
  */
  _mi_writeinfo(info, WRITEINFO_UPDATE_KEYFILE);
  return(0);

err:
  save_errno=errno;
  if (changed)
    key_changed|= HA_STATE_CHANGED;
  if (errno == HA_ERR_FOUND_DUPP_KEY || errno == HA_ERR_OUT_OF_MEM ||
      errno == HA_ERR_RECORD_FILE_FULL)
  {
    info->errkey= (int) i;
    flag=0;
    do
    {
      if (((uint64_t) 1 << i) & changed)
      {
	{
	  uint32_t new_length=_mi_make_key(info,i,new_key,newrec,pos);
	  uint32_t old_length= _mi_make_key(info,i,old_key,oldrec,pos);
	  if ((flag++ && _mi_ck_delete(info,i,new_key,new_length)) ||
	      _mi_ck_write(info,i,old_key,old_length))
	    break;
	}
      }
    } while (i-- != 0);
  }
  else
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    mi_mark_crashed(info);
  }
  info->update= (HA_STATE_CHANGED | HA_STATE_AKTIV | HA_STATE_ROW_CHANGED |
		 key_changed);

 err_end:
  _mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);
  if (save_errno == HA_ERR_KEY_NOT_FOUND)
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    save_errno=HA_ERR_CRASHED;
  }
  return(errno=save_errno);
} /* mi_update */
