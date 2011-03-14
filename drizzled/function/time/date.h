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
#include <drizzled/function/str/strfunc.h>
#include <drizzled/temporal.h>

namespace drizzled
{

/* A function which evaluates to a Date */
class Item_date :public Item_func
{
public:
  using Item_func::tmp_table_field;

  Item_date() :Item_func() {}
  Item_date(Item *a) :Item_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_DATE; }
  String *val_str(String *str);
  int64_t val_int();
  double val_real() { return val_real_from_decimal(); }
  const char *func_name() const { return "date"; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    decimals=0;
    max_length=Date::MAX_STRING_LENGTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  /**
   * All functions which inherit from Item_date must implement
   * their own get_temporal() method, which takes a supplied
   * Date reference and populates it with a correct
   * date based on the semantics of the function.
   *
   * Returns whether the function was able to correctly fill
   * the supplied date temporal with a proper date.
   *
   * @param Reference to a Date to populate
   */
  virtual bool get_temporal(Date &temporal)= 0;
  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_int64_t() { return true; }
  type::Decimal *val_decimal(type::Decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field,
                    bool )
  {
    return save_date_in_field(field);
  }
};

class Item_date_func :public Item_str_func
{
public:
  Item_date_func() :Item_str_func() {}
  Item_date_func(Item *a) :Item_str_func(a) {}
  Item_date_func(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_date_func(Item *a,Item *b, Item *c) :Item_str_func(a,b,c) {}
  enum_field_types field_type() const { return DRIZZLE_TYPE_DATETIME; }

  using Item_func::tmp_table_field;
  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_int64_t() { return true; }
  double val_real() { return (double) val_int(); }
  type::Decimal *val_decimal(type::Decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field,
                    bool )
  {
    return save_date_in_field(field);
  }
};

} /* namespace drizzled */

