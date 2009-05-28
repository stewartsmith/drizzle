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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys/mysys_priv.h"
#include "mysys/mysys_err.h"
#include "my_dir.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
  Open a file

  SYNOPSIS
    my_open()
      FileName	Fully qualified file name
      Flags	Read | write
      MyFlags	Special flags

  RETURN VALUE
    File descriptor
*/

File my_open(const char *FileName, int Flags, myf MyFlags)
				/* Path-name of file */
				/* Read | write .. */
				/* Special flags */
{
  File fd;

#if !defined(NO_OPEN_3)
  fd = open(FileName, Flags, my_umask);	/* Normal unix */
#else
  fd = open((char *) FileName, Flags);
#endif

  return(my_register_filename(fd, FileName, EE_FILENOTFOUND, MyFlags));
} /* my_open */


/*
  Close a file

  SYNOPSIS
    my_close()
      fd	File sescriptor
      myf	Special Flags

*/

int my_close(File fd, myf MyFlags)
{
  int err;

  do
  {
    err= close(fd);
  } while (err == -1 && errno == EINTR);

  if (err)
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE | MY_WME))
      my_error(EE_BADCLOSE, MYF(ME_BELL+ME_WAITTANG), "unknown", errno);
  }
  my_file_opened--;

  return(err);
} /* my_close */


/*
  TODO: Get rid of

  SYNOPSIS
    my_register_filename()
    fd			   File number opened, -1 if error on open
    FileName		   File name
    type_file_type	   How file was created
    error_message_number   Error message number if caller got error (fd == -1)
    MyFlags		   Flags for my_close()

  RETURN
    -1   error
     #   Filenumber

*/

File my_register_filename(File fd, const char *FileName, uint32_t error_message_number, myf MyFlags)
{
  if ((int) fd >= 0)
  {
    my_file_opened++;
    my_file_total_opened++;

    return fd;
  }
  else
    my_errno= errno;

  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
  {
    if (my_errno == EMFILE)
      error_message_number= EE_OUT_OF_FILERESOURCES;
    my_error(error_message_number, MYF(ME_BELL+ME_WAITTANG),
             FileName, my_errno);
  }
  return -1;
}
