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
#include <errno.h>

namespace drizzled
{
namespace internal
{


/*
  Read a chunk of bytes from a file with retry's if needed

  The parameters are:
    File descriptor
    Buffer to hold at least Count bytes
    Bytes to read
    Flags on what to do on error

    Return:
      -1 on error
      0  if flag has bits MY_NABP or MY_FNABP set
      N  number of bytes read.
*/

size_t my_read(int Filedes, unsigned char *Buffer, size_t Count, myf MyFlags)
{
  size_t readbytes, save_count;
  save_count= Count;

  for (;;)
  {
    errno= 0;					/* Linux doesn't reset this */
    if ((readbytes= read(Filedes, Buffer, Count)) != Count)
    {
      errno= errno ? errno : -1;
      if ((readbytes == 0 || (int) readbytes == -1) && errno == EINTR)
      {
        continue;                              /* Interrupted */
      }
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
        if (readbytes == (size_t) -1)
          my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
                   "unknown", errno);
        else if (MyFlags & (MY_NABP | MY_FNABP))
          my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
                   "unknown", errno);
      }
      if (readbytes == (size_t) -1 ||
          ((MyFlags & (MY_FNABP | MY_NABP)) && !(MyFlags & MY_FULL_IO)))
        return(MY_FILE_ERROR);	/* Return with error */
      if (readbytes != (size_t) -1 && (MyFlags & MY_FULL_IO))
      {
        Buffer+= readbytes;
        Count-= readbytes;
        continue;
      }
    }

    if (MyFlags & (MY_NABP | MY_FNABP))
      readbytes= 0;			/* Ok on read */
    else if (MyFlags & MY_FULL_IO)
      readbytes= save_count;
    break;
  }
  return(readbytes);
} /* my_read */

} /* namespace internal */
} /* namespace drizzled */
