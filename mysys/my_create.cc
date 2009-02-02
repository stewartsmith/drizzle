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

#include "mysys_priv.h"
#include "my_dir.h"
#include "mysys_err.h"

#include <errno.h>
	/*
	** Create a new file
	** Arguments:
	** Path-name of file
	** Read | write on file (umask value)
	** Read & Write on open file
	** Special flags
	*/


File my_create(const char *FileName, int CreateFlags, int access_flags,
               myf MyFlags)
{
  int fd, rc;

#if !defined(NO_OPEN_3)
  fd = open(FileName, access_flags | O_CREAT,
	    CreateFlags ? CreateFlags : my_umask);
#else
  fd = open(FileName, access_flags);
#endif

  if ((MyFlags & MY_SYNC_DIR) && (fd >=0) &&
      my_sync_dir_by_file(FileName, MyFlags))
  {
    my_close(fd, MyFlags);
    fd= -1;
  }

  rc= my_register_filename(fd, FileName, FILE_BY_CREATE,
                           EE_CANTCREATEFILE, MyFlags);
  /*
    my_register_filename() may fail on some platforms even if the call to
    *open() above succeeds. In this case, don't leave the stale file because
    callers assume the file to not exist if my_create() fails, so they don't
    do any cleanups.
  */
  if (unlikely(fd >= 0 && rc < 0))
  {
    int tmp= my_errno;
    my_delete(FileName, MyFlags);
    my_errno= tmp;
  }

  return(rc);
} /* my_create */
