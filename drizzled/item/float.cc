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

#include <math.h>

#include <drizzled/error.h>
#include <drizzled/field.h>
#include <drizzled/item/float.h>
#include <drizzled/item/num.h>
#include <drizzled/item/string.h>

namespace drizzled
{

extern const charset_info_st *system_charset_info;

static uint32_t nr_of_decimals(const char *str, const char *end)
{
  const char *decimal_point;

  /* Find position for '.' */
  for (;;)
  {
    if (str == end)
      return 0;
    if (*str == 'e' || *str == 'E')
      return NOT_FIXED_DEC;
    if (*str++ == '.')
      break;
  }
  decimal_point= str;
  for (; my_isdigit(system_charset_info, *str) ; str++)
    ;
  if (*str == 'e' || *str == 'E')
    return NOT_FIXED_DEC;
  return (uint32_t) (str - decimal_point);
}

String *Item_float::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  str->set_real(value,decimals,&my_charset_bin);
  return str;
}


int64_t Item_float::val_int()
{
  assert(fixed == 1);
  if (value <= (double) INT64_MIN)
  {
     return INT64_MIN;
  }
  else if (value >= (double) (uint64_t) INT64_MAX)
  {
    return INT64_MAX;
  }
  return (int64_t) rint(value);
}

type::Decimal *Item_float::val_decimal(type::Decimal *decimal_value)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  double2_class_decimal(E_DEC_FATAL_ERROR, value, decimal_value);
  return (decimal_value);
}

/**
  This function is only called during parsing. We will signal an error if
  value is not a true double value (overflow)
*/

Item_float::Item_float(const char *str_arg, uint32_t length)
{
  int error;
  char *end_not_used;
  value= my_strntod(&my_charset_bin, (char*) str_arg, length, &end_not_used,
                    &error);
  if (error)
  {
    /*
      Note that we depend on that str_arg is null terminated, which is true
      when we are in the parser
    */
    assert(str_arg[length] == 0);
    my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "double", (char*) str_arg);
  }
  presentation= name=(char*) str_arg;
  decimals=(uint8_t) nr_of_decimals(str_arg, str_arg+length);
  max_length=length;
  fixed= 1;
}

int Item_float::save_in_field(Field *field, bool)
{
  double nr= val_real();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(nr);
}


void Item_float::print(String *str)
{
  if (presentation)
  {
    str->append(presentation);
    return;
  }
  char buffer[20];
  String num(buffer, sizeof(buffer), &my_charset_bin);
  num.set_real(value, decimals, &my_charset_bin);
  str->append(num);
}

/*
  hex item
  In string context this is a binary string.
  In number context this is a int64_t value.
*/

bool Item_float::eq(const Item *arg, bool) const
{
  if (arg->basic_const_item() && arg->type() == type())
  {
    /*
      We need to cast off const to call val_int(). This should be OK for
      a basic constant.
    */
    Item *item= (Item*) arg;
    return item->val_real() == value;
  }
  return false;
}

Item *Item_static_float_func::safe_charset_converter(const charset_info_st * const)
{
  Item_string *conv;
  char buf[64];
  String *s, tmp(buf, sizeof(buf), &my_charset_bin);
  s= val_str(&tmp);
  conv= new Item_static_string_func(func_name, s->ptr(), s->length(), s->charset());
  conv->str_value.copy();
  conv->str_value.mark_as_const();
  return conv;
}

} /* namespace drizzled */
