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

#include <config.h>

#include <cassert>

#include <drizzled/sql_string.h>
#include <drizzled/function/math/int.h>
#include <drizzled/charset.h>

namespace drizzled
{

Item_int_func::~Item_int_func() {}

double Item_int_func::val_real()
{
  assert(fixed == 1);

  return unsigned_flag ? (double) ((uint64_t) val_int()) : (double) val_int();
}


String *Item_int_func::val_str(String *str)
{
  assert(fixed == 1);
  int64_t nr=val_int();
  if (null_value)
    return 0;
  str->set_int(nr, unsigned_flag, &my_charset_bin);
  return str;
}

} /* namespace drizzled */
