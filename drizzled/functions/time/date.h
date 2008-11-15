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

#ifndef DRIZZLED_FUNCTIONS_TIME_DATE_H
#define DRIZZLED_FUNCTIONS_TIME_DATE_H

/*
  This can't be a Item_str_func, because the val_real() functions are special
*/

class Item_date :public Item_func
{
public:
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
    max_length=MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_int64_t() { return true; }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field,
                    bool no_conversions __attribute__((unused)))
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
  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  bool result_as_int64_t() { return true; }
  double val_real() { return (double) val_int(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field,
                    bool no_conversions __attribute__((unused)))
  {
    return save_date_in_field(field);
  }
};

#endif /* DRIZZLED_FUNCTIONS_TIME_DATE_H */
