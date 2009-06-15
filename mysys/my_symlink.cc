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
#include <mystrings/m_string.h>
#include <errno.h>
#ifdef HAVE_REALPATH
#include <sys/param.h>
#include <sys/stat.h>
#endif

/*
  Resolve all symbolic links in path
  'to' may be equal to 'filename'

  Because purify gives a lot of UMR errors when using realpath(),
  this code is disabled when using purify.

  If MY_RESOLVE_LINK is given, only do realpath if the file is a link.
*/

#if defined(SCO)
#define BUFF_LEN 4097
#elif defined(MAXPATHLEN)
#define BUFF_LEN MAXPATHLEN
#else
#define BUFF_LEN FN_LEN
#endif

int my_realpath(char *to, const char *filename, myf MyFlags)
{
#if defined(HAVE_REALPATH) &&  !defined(HAVE_BROKEN_REALPATH)
  int result=0;
  char buff[BUFF_LEN];
  struct stat stat_buff;

  if (!(MyFlags & MY_RESOLVE_LINK) ||
      (!lstat(filename,&stat_buff) && S_ISLNK(stat_buff.st_mode)))
  {
    char *ptr;
    if ((ptr=realpath(filename,buff)))
    {
      strncpy(to,ptr,FN_REFLEN-1);
    }
    else
    {
      /*
	Realpath didn't work;  Use my_load_path() which is a poor substitute
	original name but will at least be able to resolve paths that starts
	with '.'.
      */
      my_errno=errno;
      if (MyFlags & MY_WME)
	my_error(EE_REALPATH, MYF(0), filename, my_errno);
      my_load_path(to, filename, NULL);
      result= -1;
    }
  }
  return(result);
#else
  my_load_path(to, filename, NULL);
  return 0;
#endif
}

bool test_if_hard_path(const char *dir_name)
{
  if (dir_name[0] == FN_HOMELIB && dir_name[1] == FN_LIBCHAR)
    return (home_dir != NULL && test_if_hard_path(home_dir));
  if (dir_name[0] == FN_LIBCHAR)
    return (true);
  return false;
} /* test_if_hard_path */
