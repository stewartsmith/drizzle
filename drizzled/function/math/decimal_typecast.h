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

#include <drizzled/function/func.h>
#include <drizzled/type/decimal.h>

namespace drizzled
{

class Item_decimal_typecast :public Item_func
{
  type::Decimal decimal_value;
public:
  Item_decimal_typecast(Item *a, int len, int dec) :Item_func(a)
  {
    decimals= dec;
    max_length= class_decimal_precision_to_length(len, dec, unsigned_flag);
  }
  String *val_str(String *str);
  double val_real();
  int64_t val_int();
  type::Decimal *val_decimal(type::Decimal*);
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_DECIMAL; }
  void fix_length_and_dec() {};
  const char *func_name() const { return "decimal_typecast"; }
  virtual void print(String *str);
};

} /* namespace drizzled */

