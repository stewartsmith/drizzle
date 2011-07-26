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

#include <fcntl.h>

#include <drizzled/internal/m_string.h>
#if defined(HAVE_UTIME_H)
#include <utime.h>
#elif defined(HAVE_SYS_UTIME_H)
#include <sys/utime.h>
#elif !defined(HPUX10)
#include <time.h>
struct utimbuf {
  time_t actime;
  time_t modtime;
};
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#include <drizzled/util/test.h>

namespace drizzled
{
namespace internal
{

/*
  int my_copy(const char *from, const char *to, myf MyFlags)

  NOTES
    Ordinary ownership and accesstimes are copied from 'from-file'
    If MyFlags & MY_HOLD_ORIGINAL_MODES is set and to-file exists then
    the modes of to-file isn't changed
    If MyFlags & MY_DONT_OVERWRITE_FILE is set, we will give an error
    if the file existed.

  WARNING
    Don't set MY_FNABP or MY_NABP bits on when calling this function !

  RETURN
    0	ok
    #	Error

*/

int my_copy(const char *from, const char *to, myf MyFlags)
{
  uint32_t Count;
  bool new_file_stat= 0; /* 1 if we could stat "to" */
  int create_flag;
  int from_file,to_file;
  unsigned char buff[IO_SIZE];
  struct stat stat_buff,new_stat_buff;

  from_file=to_file= -1;
  assert(!(MyFlags & (MY_FNABP | MY_NABP))); /* for my_read/my_write */
  if (MyFlags & MY_HOLD_ORIGINAL_MODES)		/* Copy stat if possible */
    new_file_stat= test(!stat((char*) to, &new_stat_buff));

  if ((from_file=my_open(from,O_RDONLY,MyFlags)) >= 0)
  {
    if (stat(from, &stat_buff))
    {
      errno=errno;
      goto err;
    }
    if (MyFlags & MY_HOLD_ORIGINAL_MODES && new_file_stat)
      stat_buff=new_stat_buff;
    create_flag= (MyFlags & MY_DONT_OVERWRITE_FILE) ? O_EXCL : O_TRUNC;

    if ((to_file=  my_create(to,(int) stat_buff.st_mode,
			     O_WRONLY | create_flag,
			     MyFlags)) < 0)
      goto err;

    while ((Count= static_cast<uint32_t>(my_read(from_file, buff,
            sizeof(buff), MyFlags))) != 0)
    {
	if (Count == (uint32_t) -1 ||
	    my_write(to_file,buff,Count,MYF(MyFlags | MY_NABP)))
	goto err;
    }

    if (my_close(from_file,MyFlags) | my_close(to_file,MyFlags))
      return(-1);				/* Error on close */

    /* Copy modes if possible */

    if (MyFlags & MY_HOLD_ORIGINAL_MODES && !new_file_stat)
	return 0;			/* File copyed but not stat */
    chmod(to, stat_buff.st_mode & 07777); /* Copy modes */
    if(chown(to, stat_buff.st_uid,stat_buff.st_gid)!=0)
        return 0;
    if (MyFlags & MY_COPYTIME)
    {
      struct utimbuf timep;
      timep.actime  = stat_buff.st_atime;
      timep.modtime = stat_buff.st_mtime;
      utime((char*) to, &timep); /* last accessed and modified times */
    }
    return 0;
  }

err:
  if (from_file >= 0) my_close(from_file,MyFlags);
  if (to_file >= 0)
  {
    my_close(to_file, MyFlags);
    /* attempt to delete the to-file we've partially written */
    my_delete(to, MyFlags);
  }
  return(-1);
} /* my_copy */

} /* namespace internal */
} /* namespace drizzled */
