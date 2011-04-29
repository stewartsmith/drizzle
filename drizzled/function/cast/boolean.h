/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <drizzled/charset.h>
#include <drizzled/function/str/strfunc.h>

namespace drizzled {
namespace function {
namespace cast {

class Boolean : public Item_str_func
{
public:
  Boolean(Item *a) :
    Item_str_func(a)
  {}

  drizzled::String *val_str(drizzled::String *value);

  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length=args[0]->max_length;
  }
  virtual void print(String *str);
  const char *func_name() const { return "cast_as_boolean"; }

private:
  String *evaluate(const bool &result, String *val_buffer);
};

} // namespace cast
} // namespace function
} // namespace drizzled

