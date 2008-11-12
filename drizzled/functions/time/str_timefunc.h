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

#ifndef DRIZZLED_FUNCTIONS_TIME_STR_TIMEFUNC_H
#define DRIZZLED_FUNCTIONS_TIME_STR_TIMEFUNC_H

class Item_str_timefunc :public Item_str_func
{
public:
  Item_str_timefunc() :Item_str_func() {}
  Item_str_timefunc(Item *a) :Item_str_func(a) {}
  Item_str_timefunc(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_str_timefunc(Item *a, Item *b, Item *c) :Item_str_func(a, b ,c) {}
  enum_field_types field_type() const { return DRIZZLE_TYPE_TIME; }
  void fix_length_and_dec()
  {
    decimals= DATETIME_DEC;
    max_length=MAX_TIME_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  }
  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }
  double val_real() { return val_real_from_decimal(); }
  my_decimal *val_decimal(my_decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_time(decimal_value);
  }
  int save_in_field(Field *field,
                    bool no_conversions __attribute__((unused)))
  {
    return save_time_in_field(field);
  }
};

#endif /* DRIZZLED_FUNCTIONS_TIME_STR_TIMEFUNC_H */
