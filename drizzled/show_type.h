/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

namespace drizzled
{

/*
  declarations for SHOW STATUS support in plugins
*/
typedef enum enum_drizzle_show_type
{
  SHOW_UNDEF, SHOW_BOOL, SHOW_INT, SHOW_LONG,
  SHOW_LONGLONG, SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_FUNC,
  SHOW_LONG_STATUS, SHOW_DOUBLE_STATUS,
  SHOW_MY_BOOL, SHOW_HA_ROWS, SHOW_SYS, SHOW_INT_NOFLUSH,
  SHOW_LONGLONG_STATUS, SHOW_DOUBLE, SHOW_SIZE
} SHOW_TYPE;

struct drizzle_show_var {
  const char *name;
  char *value;
  SHOW_TYPE type;
};


static const int SHOW_VAR_FUNC_BUFF_SIZE= 1024;
typedef int (*drizzle_show_var_func)(drizzle_show_var *, char *);

struct st_show_var_func_container
{
  drizzle_show_var_func func;
};

}

