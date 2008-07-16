/* Copyright (C) 2000-2003 MySQL AB

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

#include <my_global.h>
#include <my_sys.h>
#include <mysys_err.h>


/*
  Lock a part of a file

  RETURN VALUE
    0   Success
    -1  An error has occured and 'my_errno' is set
        to indicate the actual error code.
*/

int my_lock(File fd, int locktype, my_off_t start, my_off_t length,
            myf MyFlags)
{
  int value;
  long alarm_pos=0,alarm_end_pos=MY_HOW_OFTEN_TO_WRITE-1;

  if (my_disable_locking)
    return(0);

  {
    struct flock lock;

    lock.l_type=   (short) locktype;
    lock.l_whence= SEEK_SET;
    lock.l_start=  (off_t) start;
    lock.l_len=    (off_t) length;

    if (MyFlags & MY_DONT_WAIT)
    {
      if (fcntl(fd,F_SETLK,&lock) != -1)	/* Check if we can lock */
	return(0);			/* Ok, file locked */
      while ((value=fcntl(fd,F_SETLKW,&lock)) && !  (alarm_pos++ >= alarm_end_pos) &&
	     errno == EINTR)
      {			/* Setup again so we don`t miss it */
        alarm_end_pos+=MY_HOW_OFTEN_TO_WRITE;
      }
      if (value != -1)
	return(0);
      if (errno == EINTR)
	errno=EAGAIN;
    }
    else if (fcntl(fd,F_SETLKW,&lock) != -1) /* Wait until a lock */
      return(0);
  }

	/* We got an error. We don't want EACCES errors */
  my_errno=(errno == EACCES) ? EAGAIN : errno ? errno : -1;
  if (MyFlags & MY_WME)
  {
    if (locktype == F_UNLCK)
      my_error(EE_CANTUNLOCK,MYF(ME_BELL+ME_WAITTANG),my_errno);
    else
      my_error(EE_CANTLOCK,MYF(ME_BELL+ME_WAITTANG),my_errno);
  }
  return(-1);
} /* my_lock */
