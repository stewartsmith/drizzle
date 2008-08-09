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

/* USE_MY_STREAM isn't set because we can't thrust my_fclose! */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <errno.h>
#include <stdio.h>

#ifdef HAVE_FSEEKO
#undef ftell
#undef fseek
#define ftell(A) ftello(A)
#define fseek(A,B,C) fseeko((A),(B),(C))
#endif


/*
  Write a chunk of bytes to a stream

   my_fwrite()
   stream	File descriptor
   Buffer	Buffer to write from
   Count	Number of bytes to write
   MyFlags	Flags on what to do on error

  RETURN
    (size_t) -1 Error
    #		Number of bytes written
*/

size_t my_fwrite(FILE *stream, const uchar *Buffer, size_t Count, myf MyFlags)
{
  size_t writtenbytes =0;
  my_off_t seekptr;
#if !defined(NO_BACKGROUND) && defined(USE_MY_STREAM)
  uint errors;
#endif

#if !defined(NO_BACKGROUND) && defined(USE_MY_STREAM)
  errors=0;
#endif
  seekptr= ftell(stream);
  for (;;)
  {
    size_t written;
    if ((written = (size_t) fwrite((char*) Buffer,sizeof(char),
                                   Count, stream)) != Count)
    {
      my_errno=errno;
      if (written != (size_t) -1)
      {
	seekptr+=written;
	Buffer+=written;
	writtenbytes+=written;
	Count-=written;
      }
#ifdef EINTR
      if (errno == EINTR)
      {
	VOID(my_fseek(stream,seekptr,MY_SEEK_SET,MYF(0)));
	continue;
      }
#endif
#if !defined(NO_BACKGROUND) && defined(USE_MY_STREAM)
      if (my_thread_var->abort)
	MyFlags&= ~ MY_WAIT_IF_FULL;		/* End if aborted by user */
      if ((errno == ENOSPC || errno == EDQUOT) &&
          (MyFlags & MY_WAIT_IF_FULL))
      {
        if (!(errors++ % MY_WAIT_GIVE_USER_A_MESSAGE))
          my_error(EE_DISK_FULL,MYF(ME_BELL | ME_NOREFRESH),
                   "[stream]",my_errno,MY_WAIT_FOR_USER_TO_FIX_PANIC);
        VOID(sleep(MY_WAIT_FOR_USER_TO_FIX_PANIC));
        VOID(my_fseek(stream,seekptr,MY_SEEK_SET,MYF(0)));
        continue;
      }
#endif
      if (ferror(stream) || (MyFlags & (MY_NABP | MY_FNABP)))
      {
	if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
	{
	  my_error(EE_WRITE, MYF(ME_BELL+ME_WAITTANG),
		   my_filename(fileno(stream)),errno);
	}
	writtenbytes= (size_t) -1;        /* Return that we got error */
	break;
      }
    }
    if (MyFlags & (MY_NABP | MY_FNABP))
      writtenbytes= 0;				/* Everything OK */
    else
      writtenbytes+= written;
    break;
  }
  return(writtenbytes);
} /* my_fwrite */


/* Seek to position in file */

my_off_t my_fseek(FILE *stream, my_off_t pos, int whence,
		  myf MyFlags __attribute__((unused)))
{
  return(fseek(stream, (off_t) pos, whence) ?
	      MY_FILEPOS_ERROR : (my_off_t) ftell(stream));
} /* my_seek */
