/* Copyright (C) 2000-2001, 2003 MySQL AB

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

using namespace std;
using namespace drizzled;

	/* if flag == HA_PANIC_CLOSE then all misam files are closed */
	/* if flag == HA_PANIC_WRITE then all misam files are unlocked and
	   all changed data in single user misam is written to file */
	/* if flag == HA_PANIC_READ then all misam files that was locked when
	   mi_panic(HA_PANIC_WRITE) was done is locked. A mi_readinfo() is
	   done for all single user files to get changes in database */


int mi_panic(enum ha_panic_function flag)
{
  int error=0;
  MI_INFO *info;

  THR_LOCK_myisam.lock();
  list<MI_INFO *>::iterator it= myisam_open_list.begin();
  while (it != myisam_open_list.end())
  {
    info= *it;
    switch (flag) {
    case HA_PANIC_CLOSE:
      THR_LOCK_myisam.unlock();	/* Not exactly right... */
      if (mi_close(info))
	error=errno;
      THR_LOCK_myisam.lock();
      break;
    case HA_PANIC_WRITE:		/* Do this to free databases */
#ifdef CANT_OPEN_FILES_TWICE
      if (info->s->options & HA_OPTION_READ_ONLY_DATA)
	break;
#endif
      if (flush_key_blocks(info->s->getKeyCache(), info->s->kfile, FLUSH_RELEASE))
	error=errno;
      if (info->opt_flag & WRITE_CACHE_USED)
	if (flush_io_cache(&info->rec_cache))
	  error=errno;
      if (info->opt_flag & READ_CACHE_USED)
      {
	if (flush_io_cache(&info->rec_cache))
	  error=errno;
        info->rec_cache.reinit_io_cache(internal::READ_CACHE,0, (bool) (info->lock_type != F_UNLCK),1);
      }
      if (info->lock_type != F_UNLCK && ! info->was_locked)
      {
	info->was_locked=info->lock_type;
	if (mi_lock_database(info,F_UNLCK))
	  error=errno;
      }
#ifdef CANT_OPEN_FILES_TWICE
      if (info->s->kfile >= 0 && internal::my_close(info->s->kfile,MYF(0)))
	error = errno;
      if (info->dfile >= 0 && internal::my_close(info->dfile,MYF(0)))
	error = errno;
      info->s->kfile=info->dfile= -1;	/* Files aren't open anymore */
      break;
#endif
    case HA_PANIC_READ:			/* Restore to before WRITE */
#ifdef CANT_OPEN_FILES_TWICE
      {					/* Open closed files */
	char name_buff[FN_REFLEN];
	if (info->s->kfile < 0)
	  if ((info->s->kfile= internal::my_open(internal::fn_format(name_buff,info->filename,"",
					      N_NAME_IEXT,4),info->mode,
				    MYF(MY_WME))) < 0)
	    error = errno;
	if (info->dfile < 0)
	{
	  if ((info->dfile= internal::my_open(internal::fn_format(name_buff,info->filename,"",
					      N_NAME_DEXT,4),info->mode,
				    MYF(MY_WME))) < 0)
	    error = errno;
	  info->rec_cache.file=info->dfile;
	}
      }
#endif
      if (info->was_locked)
      {
	if (mi_lock_database(info, info->was_locked))
	  error=errno;
	info->was_locked=0;
      }
      break;
    }
    ++it;
  }
  THR_LOCK_myisam.unlock();
  if (!error)
    return(0);
  return(errno=error);
} /* mi_panic */
