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

#include <drizzled/function/str/strfunc.h>
#include <drizzled/function/time/typecast.h>
#include <drizzled/temporal.h>

namespace drizzled {
namespace function {
namespace cast {

class Time :public Item_typecast_maybe_null
{
public:
  using Item_func::tmp_table_field;

  Time(Item *a) :
    Item_typecast_maybe_null(a)
  {}

  const char *func_name() const { return "cast_as_time"; }
  String *val_str(String *str);
  bool get_time(type::Time &ltime);
  const char *cast_type() const { return "time"; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_TIME; }

  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }

  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length= 10;
    maybe_null= 1;
  }

  bool result_as_int64_t() { return true; }
  int64_t val_int();
  double val_real() { return (double) val_int(); }

  type::Decimal *val_decimal(type::Decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }

  int save_in_field(Field *field, bool )
  {
    return save_date_in_field(field);
  }
};

} // namespace cast
} // namespace function
} // namespace drizzled

