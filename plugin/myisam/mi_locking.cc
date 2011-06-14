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

/*
  locking of isam-tables.
  reads info from a isam-table. Must be first request before doing any furter
  calls to any isamfunktion.  Is used to allow many process use the same
  isamdatabase.
*/

#include "myisam_priv.h"
#include <drizzled/charset.h>
#include <drizzled/util/test.h>

using namespace std;
using namespace drizzled;

	/* lock table by F_UNLCK, F_RDLCK or F_WRLCK */

int mi_lock_database(MI_INFO *info, int lock_type)
{
  int error;
  uint32_t count;
  MYISAM_SHARE *share=info->s;
#if defined(FULL_LOG) || defined(_lint)
  uint32_t flag;
#endif

  if (!info->s->in_use)
    info->s->in_use= new list<Session *>;

  if (lock_type == F_EXTRA_LCK)                 /* Used by TMP tables */
  {
    ++share->w_locks;
    ++share->tot_locks;
    info->lock_type= lock_type;
    info->s->in_use->push_front(info->in_use);
    return(0);
  }
#if defined(FULL_LOG) || defined(_lint)
  flag=0;
#endif

  error=0;
  if (share->kfile >= 0)		/* May only be false on windows */
  {
    switch (lock_type) {
    case F_UNLCK:
      if (info->lock_type == F_RDLCK)
	count= --share->r_locks;
      else
	count= --share->w_locks;
      --share->tot_locks;
      if (info->lock_type == F_WRLCK && !share->w_locks &&
	  !share->delay_key_write && flush_key_blocks(share->getKeyCache(),
						      share->kfile,FLUSH_KEEP))
      {
	error=errno;
        mi_print_error(info->s, HA_ERR_CRASHED);
	mi_mark_crashed(info);		/* Mark that table must be checked */
      }
      if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
      {
	if (info->rec_cache.end_io_cache())
	{
	  error=errno;
          mi_print_error(info->s, HA_ERR_CRASHED);
	  mi_mark_crashed(info);
	}
      }
      if (!count)
      {
	if (share->changed && !share->w_locks)
	{
    if ((info->s->mmaped_length != info->s->state.state.data_file_length) &&
        (info->s->nonmmaped_inserts > MAX_NONMAPPED_INSERTS))
    {
      mi_remap_file(info, info->s->state.state.data_file_length);
      info->s->nonmmaped_inserts= 0;
    }
	  share->state.process= share->last_process=share->this_process;
	  share->state.unique=   info->last_unique=  info->this_unique;
	  share->state.update_count= info->last_loop= ++info->this_loop;
          if (mi_state_info_write(share->kfile, &share->state, 1))
	    error=errno;
	  share->changed=0;
          share->not_flushed=1;
	  if (error)
          {
            mi_print_error(info->s, HA_ERR_CRASHED);
	    mi_mark_crashed(info);
          }
	}
#if defined(FULL_LOG) || defined(_lint)
	if (info->lock_type != F_EXTRA_LCK)
	{
	  if (share->r_locks)
	  {					/* Only read locks left */
	    flag=1;
	  }
	  else if (!share->w_locks)
	  {					/* No more locks */
	    flag=1;
	  }
	}
#endif
      }
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      info->lock_type= F_UNLCK;
      info->s->in_use->remove(info->in_use);
      break;
    case F_RDLCK:
      if (info->lock_type == F_WRLCK)
      {
        /*
          Change RW to READONLY

          mysqld does not turn write locks to read locks,
          so we're never here in mysqld.
        */
#if defined(FULL_LOG) || defined(_lint)
	if (share->w_locks == 1)
	{
	  flag=1;
	}
#endif
	share->w_locks--;
	share->r_locks++;
	info->lock_type=lock_type;
	break;
      }
      if (!share->r_locks && !share->w_locks)
      {
#if defined(FULL_LOG) || defined(_lint)
	flag=1;
#endif
	if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
	{
	  error=errno;
	  break;
	}
	if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
	{
	  error=errno;
	  errno=error;
	  break;
	}
      }
      _mi_test_if_changed(info);
      share->r_locks++;
      share->tot_locks++;
      info->lock_type=lock_type;
      info->s->in_use->push_front(info->in_use);
      break;
    case F_WRLCK:
      if (info->lock_type == F_RDLCK)
      {						/* Change READONLY to RW */
	if (share->r_locks == 1)
	{
#if defined(FULL_LOG) || defined(_lint)
	  flag=1;
#endif
	  share->r_locks--;
	  share->w_locks++;
	  info->lock_type=lock_type;
	  break;
	}
      }
      if (!(share->options & HA_OPTION_READ_ONLY_DATA))
      {
	if (!share->w_locks)
	{
#if defined(FULL_LOG) || defined(_lint)
	  flag=1;
#endif
	  if (!share->r_locks)
	  {
	    if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
	    {
	      error=errno;
	      errno=error;
	      break;
	    }
	  }
	}
      }
      _mi_test_if_changed(info);

      info->lock_type=lock_type;
      share->w_locks++;
      share->tot_locks++;
      info->s->in_use->push_front(info->in_use);
      break;
    default:
      break;				/* Impossible */
    }
  }
#ifdef __WIN__
  else
  {
    /*
       Check for bad file descriptors if this table is part
       of a merge union. Failing to capture this may cause
       a crash on windows if the table is renamed and
       later on referenced by the merge table.
     */
    if( info->owned_by_merge && (info->s)->kfile < 0 )
    {
      error = HA_ERR_NO_SUCH_TABLE;
    }
  }
#endif
#if defined(FULL_LOG) || defined(_lint)
  lock_type|=(int) (flag << 8);		/* Set bit to set if real lock */
  myisam_log_command(MI_LOG_LOCK,info,(unsigned char*) &lock_type,sizeof(lock_type),
		     error);
#endif
  return(error);
} /* mi_lock_database */


/****************************************************************************
 ** functions to read / write the state
****************************************************************************/

int _mi_readinfo(register MI_INFO *info, int lock_type, int check_keybuffer)
{
  if (info->lock_type == F_UNLCK)
  {
    MYISAM_SHARE *share=info->s;
    if (!share->tot_locks)
    {
      if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
      {
	int error=errno ? errno : -1;
	errno=error;
	return(1);
      }
    }
    if (check_keybuffer)
      _mi_test_if_changed(info);
  }
  else if (lock_type == F_WRLCK && info->lock_type == F_RDLCK)
  {
    errno=EACCES;				/* Not allowed to change */
    return(-1);				/* when have read_lock() */
  }
  return(0);
} /* _mi_readinfo */


/*
  Every isam-function that uppdates the isam-database MUST end with this
  request
*/

int _mi_writeinfo(register MI_INFO *info, uint32_t operation)
{
  int error,olderror;
  MYISAM_SHARE *share=info->s;

  error=0;
  if (share->tot_locks == 0)
  {
    olderror=errno;			/* Remember last error */
    if (operation)
    {					/* Two threads can't be here */
      share->state.process= share->last_process=   share->this_process;
      share->state.unique=  info->last_unique=	   info->this_unique;
      share->state.update_count= info->last_loop= ++info->this_loop;
      if ((error=mi_state_info_write(share->kfile, &share->state, 1)))
	olderror=errno;
    }
    errno=olderror;
  }
  else if (operation)
    share->changed= 1;			/* Mark keyfile changed */
  return(error);
} /* _mi_writeinfo */


	/* Test if someone has changed the database */
	/* (Should be called after readinfo) */

int _mi_test_if_changed(register MI_INFO *info)
{
  MYISAM_SHARE *share=info->s;
  if (share->state.process != share->last_process ||
      share->state.unique  != info->last_unique ||
      share->state.update_count != info->last_loop)
  {						/* Keyfile has changed */
    if (share->state.process != share->this_process)
      flush_key_blocks(share->getKeyCache(), share->kfile, FLUSH_RELEASE);
    share->last_process=share->state.process;
    info->last_unique=	share->state.unique;
    info->last_loop=	share->state.update_count;
    info->update|=	HA_STATE_WRITTEN;	/* Must use file on next */
    info->data_changed= 1;			/* For mi_is_changed */
    return 1;
  }
  return (!(info->update & HA_STATE_AKTIV) ||
	  (info->update & (HA_STATE_WRITTEN | HA_STATE_DELETED |
			   HA_STATE_KEY_CHANGED)));
} /* _mi_test_if_changed */


/*
  Put a mark in the .MYI file that someone is updating the table


  DOCUMENTATION

  state.open_count in the .MYI file is used the following way:
  - For the first change of the .MYI file in this process open_count is
    incremented by mi_mark_file_change(). (We have a write lock on the file
    when this happens)
  - In mi_close() it's decremented by _mi_decrement_open_count() if it
    was incremented in the same process.

  This mean that if we are the only process using the file, the open_count
  tells us if the MYISAM file wasn't properly closed.*/


int _mi_mark_file_changed(MI_INFO *info)
{
  unsigned char buff[3];
  register MYISAM_SHARE *share=info->s;

  if (!(share->state.changed & STATE_CHANGED) || ! share->global_changed)
  {
    share->state.changed|=(STATE_CHANGED | STATE_NOT_ANALYZED |
			   STATE_NOT_OPTIMIZED_KEYS);
    if (!share->global_changed)
    {
      share->global_changed=1;
      share->state.open_count++;
    }
    if (!share->temporary)
    {
      mi_int2store(buff,share->state.open_count);
      buff[2]=1;				/* Mark that it's changed */
      return(my_pwrite(share->kfile,buff,sizeof(buff),
                            sizeof(share->state.header),
                            MYF(MY_NABP)));
    }
  }
  return(0);
}


/*
  This is only called by close or by extra(HA_FLUSH) if the OS has the pwrite()
  call.  In these context the following code should be safe!
 */

int _mi_decrement_open_count(MI_INFO *info)
{
  unsigned char buff[2];
  register MYISAM_SHARE *share=info->s;
  int lock_error=0,write_error=0;
  if (share->global_changed)
  {
    uint32_t old_lock=info->lock_type;
    share->global_changed=0;
    lock_error=mi_lock_database(info,F_WRLCK);
    /* Its not fatal even if we couldn't get the lock ! */
    if (share->state.open_count > 0)
    {
      share->state.open_count--;
      mi_int2store(buff,share->state.open_count);
      write_error=my_pwrite(share->kfile,buff,sizeof(buff),
			    sizeof(share->state.header),
			    MYF(MY_NABP));
    }
    if (!lock_error)
      lock_error=mi_lock_database(info,old_lock);
  }
  return test(lock_error || write_error);
}
