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

#ifndef DRIZZLED_FUNCTIONS_TIME_ADD_TIME_H
#define DRIZZLED_FUNCTIONS_TIME_ADD_TIME_H

#include <drizzled/item/strfunc.h>

class Item_func_add_time :public Item_str_func
{
  const bool is_date;
  int sign;
  enum_field_types cached_field_type;

public:
  Item_func_add_time(Item *a, Item *b, bool type_arg, bool neg_arg)
    :Item_str_func(a, b), is_date(type_arg) { sign= neg_arg ? -1 : 1; }
  String *val_str(String *str);
  enum_field_types field_type() const { return cached_field_type; }
  void fix_length_and_dec();

  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  virtual void print(String *str, enum_query_type query_type);
  const char *func_name() const { return "add_time"; }
  double val_real() { return val_real_from_decimal(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    assert(fixed == 1);
    if (cached_field_type == DRIZZLE_TYPE_TIME)
      return  val_decimal_from_time(decimal_value);
    if (cached_field_type == DRIZZLE_TYPE_DATETIME)
      return  val_decimal_from_date(decimal_value);
    return Item_str_func::val_decimal(decimal_value);
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    if (cached_field_type == DRIZZLE_TYPE_TIME)
      return save_time_in_field(field);
    if (cached_field_type == DRIZZLE_TYPE_DATETIME)
      return save_date_in_field(field);
    return Item_str_func::save_in_field(field, no_conversions);
  }
};

#endif /* DRIZZLED_FUNCTIONS_TIME_ADD_TIME_H */
