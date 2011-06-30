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

/* close a isam-database */
/*
  TODO:
   We need to have a separate mutex on the closed file to allow other threads
   to open other files during the time we flush the cache and close this file
*/

#include "myisam_priv.h"
#include <cstdlib>

using namespace drizzled;

int mi_close(MI_INFO *info)
{
  int error=0,flag;
  MYISAM_SHARE *share=info->s;

  THR_LOCK_myisam.lock();
  if (info->lock_type == F_EXTRA_LCK)
    info->lock_type=F_UNLCK;			/* HA_EXTRA_NO_USER_CHANGE */

  if (share->reopen == 1 && share->kfile >= 0)
    _mi_decrement_open_count(info);

  if (info->lock_type != F_UNLCK)
  {
    if (mi_lock_database(info,F_UNLCK))
      error=errno;
  }

  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    share->r_locks--;
    share->tot_locks--;
  }
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
  {
    if (info->rec_cache.end_io_cache())
      error=errno;
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  }
  flag= !--share->reopen;
  myisam_open_list.remove(info);

  void * rec_buff_ptr= mi_get_rec_buff_ptr(info, info->rec_buff);
  free(rec_buff_ptr);
  if (flag)
  {
    if (share->kfile >= 0 &&
	flush_key_blocks(share->getKeyCache(), share->kfile,
			 share->temporary ? FLUSH_IGNORE_CHANGED :
			 FLUSH_RELEASE))
      error=errno;
    end_key_cache(share->getKeyCache(), true);
    if (share->kfile >= 0)
    {
      /*
        If we are crashed, we can safely flush the current state as it will
        not change the crashed state.
        We can NOT write the state in other cases as other threads
        may be using the file at this point
      */
      if (share->mode != O_RDONLY && mi_is_crashed(info))
	mi_state_info_write(share->kfile, &share->state, 1);
      if (internal::my_close(share->kfile,MYF(0)))
        error = errno;
    }
    if (share->decode_trees)
    {
      free((unsigned char*) share->decode_trees);
      free((unsigned char*) share->decode_tables);
    }
    delete info->s->in_use;
    free((unsigned char*) info->s);
  }
  THR_LOCK_myisam.unlock();

  if (info->dfile >= 0 && internal::my_close(info->dfile,MYF(0)))
    error = errno;

  free((unsigned char*) info);

  if (error)
  {
    return(errno=error);
  }
  return(0);
} /* mi_close */
