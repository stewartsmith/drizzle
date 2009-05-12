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

int my_delete(const char *name, myf MyFlags)
{
  int err;

  if ((err = unlink(name)) == -1)
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_DELETE,MYF(ME_BELL+ME_WAITTANG+(MyFlags & ME_NOINPUT)),
	       name,errno);
  }
  else if ((MyFlags & MY_SYNC_DIR) &&
           my_sync_dir_by_file(name, MyFlags))
    err= -1;
  return(err);
} /* my_delete */
