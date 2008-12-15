/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_FUNCTION_STR_HEX_H
#define DRIZZLED_FUNCTION_STR_HEX_H

#include <drizzled/function/str/strfunc.h>

class Item_func_hex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_hex(Item *a) :Item_str_func(a) {}
  const char *func_name() const { return "hex"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(default_charset());
    decimals=0;
    max_length=args[0]->max_length*2*collation.collation->mbmaxlen;
  }
};

class Item_func_unhex :public Item_str_func
{
  String tmp_value;
public:
  Item_func_unhex(Item *a) :Item_str_func(a)
  {
    /* there can be bad hex strings */
    maybe_null= 1;
  }
  const char *func_name() const { return "unhex"; }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=(1+args[0]->max_length)/2;
  }
};

#endif /* DRIZZLED_FUNCTION_STR_HEX_H */
