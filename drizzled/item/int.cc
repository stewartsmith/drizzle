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

#include <drizzled/charset.h>
#include <drizzled/field.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/item/int.h>

namespace drizzled
{

/**
  Create an item from a string we KNOW points to a valid int64_t
  end \\0 terminated number string.
  This is always 'signed'. Unsigned values are created with Item_uint()
*/

Item_int::Item_int(const char *str_arg, uint32_t length)
{
  char *end_ptr= (char*) str_arg + length;
  int error;
  value= internal::my_strtoll10(str_arg, &end_ptr, &error);
  max_length= (uint32_t) (end_ptr - str_arg);
  name= (char*) str_arg;
  fixed= 1;
}

type::Decimal *Item_int::val_decimal(type::Decimal *decimal_value)
{
  int2_class_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_value);
  return decimal_value;
}

String *Item_int::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  str->set(value, &my_charset_bin);
  return str;
}

void Item_int::print(String *str)
{
  // my_charset_bin is good enough for numbers
  str_value.set(value, &my_charset_bin);
  str->append(str_value);
}

int Item_int::save_in_field(Field *field, bool)
{
  int64_t nr=val_int();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(nr, unsigned_flag);
}

bool Item_int::eq(const Item *arg, bool) const
{
  /* No need to check for null value as basic constant can't be NULL */
  if (arg->basic_const_item() && arg->type() == type())
  {
    /*
      We need to cast off const to call val_int(). This should be OK for
      a basic constant.
    */
    Item *item= (Item*) arg;
    return item->val_int() == value && item->unsigned_flag == unsigned_flag;
  }
  return false;
}


} /* namespace drizzled */
