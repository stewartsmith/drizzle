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
#include <drizzled/internal/thread_var.h>
#include <drizzled/error.h>
#include <cerrno>

namespace drizzled
{
namespace internal
{

	/* Write a chunk of bytes to a file */

size_t my_write(int Filedes, const unsigned char *Buffer, size_t Count, myf MyFlags)
{
  size_t writenbytes, written;
  written=0;

  /* The behavior of write(fd, buf, 0) is not portable */
  if (unlikely(!Count))
    return 0;

  for (;;)
  {
    if ((writenbytes= write(Filedes, Buffer, Count)) == Count)
      break;
    if (writenbytes != (size_t) -1)
    {						/* Safeguard */
      written+=writenbytes;
      Buffer+=writenbytes;
      Count-=writenbytes;
    }
    errno=errno;
    if (MyFlags & (MY_NABP | MY_FNABP))
    {
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	my_error(EE_WRITE, MYF(ME_BELL+ME_WAITTANG),
		 "unknown", errno);
      }
      return(MY_FILE_ERROR);		/* Error on read */
    }
    else
      break;					/* Return bytes written */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    return 0;			/* Want only errors */
  return(writenbytes+written);
} /* my_write */

} /* namespace internal */
} /* namespace drizzled */
