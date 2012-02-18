/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2011 Matthew Rheaume
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hopethat it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <drizzled/function/func.h>
#include <drizzled/temporal.h>

namespace drizzled {
  
class Item_time :public Item_func
{
public:
  using Item_func::tmp_table_field;

  Item_time() :Item_func() {}
  Item_time(Item *a) :Item_func(a) {}
  enum Item_result result_type() const { return STRING_RESULT; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_TIME; }
  String *val_str(String *str);
  int64_t val_int();
  double val_real() { return val_real_from_decimal(); }
  const char *func_name() const { return "time"; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=Time::MAX_STRING_LENGTH*MY_CHARSET_BIN_MB_MAXLEN;
  }

  virtual bool get_temporal(Time &temporal)=0;
  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_int64_t() { return true; }
  type::Decimal *val_decimal(type::Decimal *decimal_value)
  {
    assert(fixed == 1);
    return val_decimal_from_time(decimal_value);
  }
  int save_in_field(Field *field,
                    bool )
  {
    return save_time_in_field(field);
  }
};

} /* namespace drizzled */
