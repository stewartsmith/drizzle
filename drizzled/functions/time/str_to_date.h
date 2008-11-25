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

#ifndef DRIZZLED_FUNCTIONS_TIME_STR_TO_DATE_H
#define DRIZZLED_FUNCTIONS_TIME_STR_TO_DATE_H

#include <drizzled/item/strfunc.h>
#include <drizzled/item/timefunc.h>

class Item_func_str_to_date :public Item_str_func
{
  enum_field_types cached_field_type;
  date_time_format_types cached_format_type;
  enum enum_drizzle_timestamp_type cached_timestamp_type;
  bool const_item;
public:
  Item_func_str_to_date(Item *a, Item *b)
    :Item_str_func(a, b), const_item(false)
  {}
  String *val_str(String *str);
  bool get_date(DRIZZLE_TIME *ltime, uint32_t fuzzy_date);
  const char *func_name() const { return "str_to_date"; }
  enum_field_types field_type() const { return cached_field_type; }
  void fix_length_and_dec();
  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 1);
  }
};


#endif /* DRIZZLED_FUNCTIONS_TIME_STR_TO_DATE_H */
