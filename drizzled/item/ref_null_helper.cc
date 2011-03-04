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

#include <config.h>

#include <drizzled/item/ref.h>
#include <drizzled/item/ref_null_helper.h>
#include <drizzled/item/subselect.h>
#include <drizzled/lex_string.h>

namespace drizzled
{

double Item_ref_null_helper::val_real()
{
  assert(fixed == 1);
  double tmp= (*ref)->val_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


int64_t Item_ref_null_helper::val_int()
{
  assert(fixed == 1);
  int64_t tmp= (*ref)->val_int_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


type::Decimal *Item_ref_null_helper::val_decimal(type::Decimal *decimal_value)
{
  assert(fixed == 1);
  type::Decimal *val= (*ref)->val_decimal_result(decimal_value);
  owner->was_null|= null_value= (*ref)->null_value;
  return val;
}

bool Item_ref_null_helper::val_bool()
{
  assert(fixed == 1);
  bool val= (*ref)->val_bool_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return val;
}


String* Item_ref_null_helper::val_str(String* s)
{
  assert(fixed == 1);
  String* tmp= (*ref)->str_result(s);
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}

bool Item_ref_null_helper::get_date(type::Time &ltime, uint32_t fuzzydate)
{
  return (owner->was_null|= null_value= (*ref)->get_date(ltime, fuzzydate));
}

void Item_ref_null_helper::print(String *str)
{
  str->append(STRING_WITH_LEN("<ref_null_helper>("));
  if (ref)
    (*ref)->print(str);
  else
    str->append('?');
  str->append(')');
}

} /* namespace drizzled */
