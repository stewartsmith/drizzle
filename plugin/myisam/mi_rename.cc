/* Copyright (C) 2000-2001, 2004 MySQL AB

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

/*
  Rename a table
*/

#include "myisam_priv.h"

using namespace drizzled;

int mi_rename(const char *old_name, const char *new_name)
{
  char from[FN_REFLEN],to[FN_REFLEN];

  internal::fn_format(from,old_name,"",MI_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  internal::fn_format(to,new_name,"",MI_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  if (internal::my_rename_with_symlink(from, to, MYF(MY_WME)))
    return(errno);
  internal::fn_format(from,old_name,"",MI_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  internal::fn_format(to,new_name,"",MI_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  return(internal::my_rename_with_symlink(from, to,MYF(MY_WME)) ? errno : 0);
}
