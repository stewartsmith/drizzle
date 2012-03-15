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

#include <config.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/error.h>

#include <fcntl.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>


namespace drizzled {
namespace internal {

/*
  Open a file

  SYNOPSIS
    my_open()
      FileName	Fully qualified file name
      Flags	Read | write
      MyFlags	Special flags

  RETURN VALUE
    int descriptor
*/

int my_open(const char *FileName, int Flags, myf MyFlags)
				/* Path-name of file */
				/* Read | write .. */
				/* Special flags */
{
  int fd;

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

int my_close(int fd, myf MyFlags)
{
  int err;

  do
  {
    err= close(fd);
  } while (err == -1 && errno == EINTR);

  if (err)
  {
    errno=errno;
    if (MyFlags & (MY_FAE | MY_WME))
      my_error(EE_BADCLOSE, MYF(ME_BELL+ME_WAITTANG), "unknown", errno);
  }

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

int my_register_filename(int fd, const char *FileName, uint32_t error_message_number, myf MyFlags)
{
  if (fd >= 0)
    return fd;
  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
  {
    if (errno == EMFILE)
      error_message_number= EE_OUT_OF_FILERESOURCES;
    my_error(static_cast<drizzled::error_t>(error_message_number), MYF(ME_BELL+ME_WAITTANG), FileName, errno);
  }
  return -1;
}

} /* namespace internal */
} /* namespace drizzled */
