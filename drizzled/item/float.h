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

#include <drizzled/item/num.h>

namespace drizzled
{

class Item_float :public Item_num
{
  char *presentation;
public:
  double value;
  // Item_real() :value(0) {}
  Item_float(const char *str_arg, uint32_t length);
  Item_float(const char *str,double val_arg,uint32_t decimal_par,uint32_t length)
    :value(val_arg)
  {
    presentation= name=(char*) str;
    decimals=(uint8_t) decimal_par;
    max_length=length;
    fixed= 1;
  }
  Item_float(double value_par, uint32_t decimal_par) :presentation(0), value(value_par)
  {
    decimals= (uint8_t) decimal_par;
    fixed= 1;
  }
  int save_in_field(Field *field, bool no_conversions);
  enum Type type() const { return REAL_ITEM; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_DOUBLE; }
  double val_real() { assert(fixed == 1); return value; }
  int64_t val_int();
  String *val_str(String*);
  type::Decimal *val_decimal(type::Decimal *);
  bool basic_const_item() const { return 1; }
  Item *clone_item()
  { return new Item_float(name, value, decimals, max_length); }
  Item_num *neg() { value= -value; return this; }
  virtual void print(String *str);
  bool eq(const Item *, bool binary_cmp) const;
};

class Item_static_float_func :public Item_float
{
  const char *func_name;
public:
  Item_static_float_func(const char *str, double val_arg, uint32_t decimal_par,
                        uint32_t length)
    :Item_float(NULL, val_arg, decimal_par, length), func_name(str)
  {}

  virtual inline void print(String *str)
  {
    str->append(func_name);
  }

  Item *safe_charset_converter(const charset_info_st * const tocs);
};

} /* namespace drizzled */


