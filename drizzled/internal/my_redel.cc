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
#include <drizzled/internal/m_string.h>
#include <drizzled/error.h>
#if defined(HAVE_UTIME_H)
#include <utime.h>
#elif defined(HAVE_SYS_UTIME_H)
#include <sys/utime.h>
#elif !defined(HPUX10)
struct utimbuf {
  time_t actime;
  time_t modtime;
};
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

namespace drizzled
{
namespace internal
{

	/*
	  Rename with copy stat form old file
	  Copy stats from old file to new file, deletes orginal and
	  changes new file name to old file name

	  if MY_REDEL_MAKE_COPY is given, then the orginal file
	  is renamed to org_name-'current_time'.BAK
	*/

int my_redel(const char *org_name, const char *tmp_name, myf MyFlags)
{
  int error=1;

  if (my_copystat(org_name,tmp_name,MyFlags) < 0)
    goto end;
  if (my_delete(org_name, MyFlags))
      goto end;
  if (my_rename(tmp_name,org_name,MyFlags))
    goto end;

  error=0;
end:
  return(error);
} /* my_redel */


	/* Copy stat from one file to another */
	/* Return -1 if can't get stat, 1 if wrong type of file */

int my_copystat(const char *from, const char *to, int MyFlags)
{
  struct stat statbuf;

  if (stat((char*) from, &statbuf))
  {
    errno=errno;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_STAT, MYF(ME_BELL+ME_WAITTANG),from,errno);
    return -1;				/* Can't get stat on input file */
  }
  if ((statbuf.st_mode & S_IFMT) != S_IFREG)
    return 1;
  chmod(to, statbuf.st_mode & 07777);		/* Copy modes */

  if (statbuf.st_nlink > 1 && MyFlags & MY_LINK_WARNING)
  {
    if (MyFlags & MY_LINK_WARNING)
      my_error(EE_LINK_WARNING,MYF(ME_BELL+ME_WAITTANG),from,statbuf.st_nlink);
  }
  if(chown(to, statbuf.st_uid, statbuf.st_gid)!=0)
    return 1;

#ifndef __ZTC__
  if (MyFlags & MY_COPYTIME)
  {
    struct utimbuf timep;
    timep.actime  = statbuf.st_atime;
    timep.modtime = statbuf.st_mtime;
    utime((char*) to, &timep);/* Update last accessed and modified times */
  }
#else
  if (MyFlags & MY_COPYTIME)
  {
    time_t time[2];
    time[0]= statbuf.st_atime;
    time[1]= statbuf.st_mtime;
    utime((char*) to, time);/* Update last accessed and modified times */
  }
#endif
  return 0;
} /* my_copystat */

} /* namespace internal */
} /* namespace drizzled */
