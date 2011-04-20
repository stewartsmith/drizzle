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

#include <drizzled/charset.h>
#include <drizzled/item/uint.h>

namespace drizzled
{

Item_uint::Item_uint(const char *str_arg, uint32_t length):
  Item_int(str_arg, length)
{
  unsigned_flag= 1;
}


Item_uint::Item_uint(const char *str_arg, int64_t i, uint32_t length):
  Item_int(str_arg, i, length)
{
  unsigned_flag= 1;
}


String *Item_uint::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  str->set((uint64_t) value, &my_charset_bin);
  return str;
}

void Item_uint::print(String *str)
{
  // latin1 is good enough for numbers
  str_value.set((uint64_t) value, default_charset());
  str->append(str_value);
}

int Item_uint::save_in_field(Field *field, bool no_conversions)
{
  /* Item_int::save_in_field handles both signed and unsigned. */
  return Item_int::save_in_field(field, no_conversions);
}

} /* namespace drizzled */
