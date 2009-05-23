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
#include <mystrings/m_string.h>
#include "my_static.h"
#include "mysys/mysys_err.h"
#include <stdio.h>
#include <errno.h>
#include <string>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

using namespace std;

/*
  @brief
  Create a temporary file with unique name in a given directory

  @details
  create_temp_file
    to             pointer to buffer where temporary filename will be stored
    dir            directory where to create the file
    prefix         prefix the filename with this
    MyFlags        Magic flags

  @return
    File descriptor of opened file if success
    -1 and sets errno if fails.

  @note
    The behaviour of this function differs a lot between
    implementation, it's main use is to generate a file with
    a name that does not already exist.

    The implementation using mkstemp should be considered the
    reference implementation when adding a new or modifying an
    existing one

*/

File create_temp_file(char *to, const char *dir, const char *prefix, myf MyFlags)
{
  File file= -1;

#if defined(_ZTC__)
  if (!dir)
    dir=getenv("TMPDIR");
  if ((res=tempnam((char*) dir,(char *) prefix)))
  {
    strncpy(to,res,FN_REFLEN-1);
    (*free)(res);
    file=my_create(to, 0, mode | O_EXCL, MyFlags);
  }
#elif defined(HAVE_MKSTEMP)
  {
    File org_file;
    string prefix_str;

    prefix_str= prefix ? prefix : "tmp.";
    prefix_str.append("XXXXXX");

    if (!dir && ! (dir =getenv("TMPDIR")))
      dir= P_tmpdir;
    if (strlen(dir)+prefix_str.length() > FN_REFLEN-2)
    {
      errno=my_errno= ENAMETOOLONG;
      return(file);
    }
    strcpy(convert_dirname(to,dir,NULL),prefix_str.c_str());
    org_file=mkstemp(to);
    /* TODO: This was old behavior, but really don't we want to
     * unlink files immediately under all circumstances?
     * if (mode & O_TEMPORARY)
      (void) my_delete(to, MYF(MY_WME | ME_NOINPUT));
     */
    file=my_register_filename(org_file, to, EE_CANTCREATEFILE, MyFlags);

    /* If we didn't manage to register the name, remove the temp file */
    if (org_file >= 0 && file < 0)
    {
      int tmp=my_errno;
      close(org_file);
      (void) my_delete(to, MYF(MY_WME | ME_NOINPUT));
      my_errno=tmp;
    }
  }
#elif defined(HAVE_TEMPNAM)
  {
    (void)MyFlags;
    char *res,**old_env,*temp_env[1];
    if (dir && !dir[0])
    {				/* Change empty string to current dir */
      to[0]= FN_CURLIB;
      to[1]= 0;
      dir=to;
    }
    old_env= (char**) environ;
    if (dir)
    {				/* Don't use TMPDIR if dir is given */
      environ=(const char**) temp_env;
      temp_env[0]=0;
    }
    if ((res=tempnam((char*) dir, (char*) prefix)))
    {
      strncpy(to,res,FN_REFLEN-1);
      (*free)(res);
      file=my_create(to,0,
		     (int) (O_RDWR | O_TRUNC | O_EXCL),
		     MYF(MY_WME));

    }
    environ=(const char**) old_env;
  }
#else
#error No implementation found for create_temp_file
#endif
  if (file >= 0)
    my_tmp_file_created++;

  return(file);
}
