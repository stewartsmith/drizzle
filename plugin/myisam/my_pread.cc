/* Copyright (C) 2000 MySQL AB

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
#include <drizzled/error.h>
#include <cerrno>
#include <unistd.h>

using namespace drizzled;

#define MY_WAIT_FOR_USER_TO_FIX_PANIC	60	/* in seconds */
#define MY_WAIT_GIVE_USER_A_MESSAGE	10	/* Every 10 times of prev */

/*
  Read a chunk of bytes from a file from a given position

  SYNOPSIOS
    my_pread()
    Filedes	File decsriptor
    Buffer	Buffer to read data into
    Count	Number of bytes to read
    offset	Position to read from
    MyFlags	Flags

  NOTES
    This differs from the normal pread() call in that we don't care
    to set the position in the file back to the original position
    if the system doesn't support pread().

  RETURN
    (size_t) -1   Error
    #             Number of bytes read
*/

size_t my_pread(int Filedes, unsigned char *Buffer, size_t Count, internal::my_off_t offset,
                myf MyFlags)
{
  size_t readbytes;
  int error= 0;
  for (;;)
  {
    errno=0;					/* Linux doesn't reset this */
    if ((error= ((readbytes= pread(Filedes, Buffer, Count, offset)) != Count)))
      errno= errno ? errno : -1;
    if (error || readbytes != Count)
    {
      if ((readbytes == 0 || readbytes == (size_t) -1) && errno == EINTR)
      {
        continue;                              /* Interrupted */
      }
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	if (readbytes == (size_t) -1)
	  my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG), "unknown", errno);
	else if (MyFlags & (MY_NABP | MY_FNABP))
	  my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG), "unknown", errno);
      }
      if (readbytes == (size_t) -1 || (MyFlags & (MY_FNABP | MY_NABP)))
	return(MY_FILE_ERROR);		/* Return with error */
    }
    if (MyFlags & (MY_NABP | MY_FNABP))
      return(0);				/* Read went ok; Return 0 */
    return(readbytes);
  }
} /* my_pread */


/*
  Write a chunk of bytes to a file at a given position

  SYNOPSIOS
    my_pwrite()
    Filedes	File decsriptor
    Buffer	Buffer to write data from
    Count	Number of bytes to write
    offset	Position to write to
    MyFlags	Flags

  NOTES
    This differs from the normal pwrite() call in that we don't care
    to set the position in the file back to the original position
    if the system doesn't support pwrite()

  RETURN
    (size_t) -1   Error
    #             Number of bytes read
*/

size_t my_pwrite(int Filedes, const unsigned char *Buffer, size_t Count,
                 internal::my_off_t offset, myf MyFlags)
{
  size_t writenbytes, written;
  uint32_t errors;
  errors= 0;
  written= 0;

  for (;;)
  {
    if ((writenbytes= pwrite(Filedes, Buffer, Count,offset)) == Count)
      break;
    errno= errno;
    if (writenbytes != (size_t) -1)
    {					/* Safegueard */
      written+=writenbytes;
      Buffer+=writenbytes;
      Count-=writenbytes;
      offset+=writenbytes;
    }
#ifndef NO_BACKGROUND
    if ((errno == ENOSPC || errno == EDQUOT) &&
        (MyFlags & MY_WAIT_IF_FULL))
    {
      if (!(errors++ % MY_WAIT_GIVE_USER_A_MESSAGE))
	my_error(EE_DISK_FULL,MYF(ME_BELL | ME_NOREFRESH),
		 "unknown", errno, MY_WAIT_FOR_USER_TO_FIX_PANIC);
      sleep(MY_WAIT_FOR_USER_TO_FIX_PANIC);
      continue;
    }
    if ((writenbytes && writenbytes != (size_t) -1) || errno == EINTR)
      continue;					/* Retry */
#endif
    if (MyFlags & (MY_NABP | MY_FNABP))
    {
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	my_error(EE_WRITE, MYF(ME_BELL | ME_WAITTANG), "unknown", errno);
      }
      return(MY_FILE_ERROR);		/* Error on read */
    }
    else
      break;					/* Return bytes written */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    return(0);			/* Want only errors */
  return(writenbytes+written);
} /* my_pwrite */
