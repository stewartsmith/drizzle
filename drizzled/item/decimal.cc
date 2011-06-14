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
#include <drizzled/item/decimal.h>

namespace drizzled
{

Item_decimal::Item_decimal(const char *str_arg, uint32_t length,
                           const charset_info_st * const charset)
{
  decimal_value.store(E_DEC_FATAL_ERROR, str_arg, length, charset);
  name= (char*) str_arg;
  decimals= (uint8_t) decimal_value.frac;
  fixed= 1;
  max_length= class_decimal_precision_to_length(decimal_value.intg + decimals,
                                             decimals, unsigned_flag);
}

Item_decimal::Item_decimal(int64_t val, bool unsig)
{
  int2_class_decimal(E_DEC_FATAL_ERROR, val, unsig, &decimal_value);
  decimals= (uint8_t) decimal_value.frac;
  fixed= 1;
  max_length= class_decimal_precision_to_length(decimal_value.intg + decimals,
                                             decimals, unsigned_flag);
}


Item_decimal::Item_decimal(double val, int, int)
{
  double2_class_decimal(E_DEC_FATAL_ERROR, val, &decimal_value);
  decimals= (uint8_t) decimal_value.frac;
  fixed= 1;
  max_length= class_decimal_precision_to_length(decimal_value.intg + decimals,
                                             decimals, unsigned_flag);
}

Item_decimal::Item_decimal(const char *str, const type::Decimal *val_arg,
                           uint32_t decimal_par, uint32_t length)
{
  class_decimal2decimal(val_arg, &decimal_value);
  name= (char*) str;
  decimals= (uint8_t) decimal_par;
  max_length= length;
  fixed= 1;
}


Item_decimal::Item_decimal(type::Decimal *value_par)
{
  class_decimal2decimal(value_par, &decimal_value);
  decimals= (uint8_t) decimal_value.frac;
  fixed= 1;
  max_length= class_decimal_precision_to_length(decimal_value.intg + decimals,
                                             decimals, unsigned_flag);
}


Item_decimal::Item_decimal(const unsigned char *bin, int precision, int scale)
{
  binary2_class_decimal(E_DEC_FATAL_ERROR, bin,
                    &decimal_value, precision, scale);
  decimals= (uint8_t) decimal_value.frac;
  fixed= 1;
  max_length= class_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}

int64_t Item_decimal::val_int()
{
  int64_t result;
  decimal_value.val_int32(E_DEC_FATAL_ERROR, unsigned_flag, &result);
  return result;
}

double Item_decimal::val_real()
{
  double result;
  class_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &result);
  return result;
}

String *Item_decimal::val_str(String *result)
{
  result->set_charset(&my_charset_bin);
  class_decimal2string(&decimal_value, 0, result);
  return result;
}

void Item_decimal::print(String *str)
{
  class_decimal2string(&decimal_value, 0, &str_value);
  str->append(str_value);
}

bool Item_decimal::eq(const Item *item, bool) const
{
  if (type() == item->type() && item->basic_const_item())
  {
    /*
      We need to cast off const to call val_decimal(). This should
      be OK for a basic constant. Additionally, we can pass 0 as
      a true decimal constant will return its internal decimal
      storage and ignore the argument.
    */
    Item *arg= (Item*) item;
    type::Decimal *value= arg->val_decimal(0);
    return !class_decimal_cmp(&decimal_value, value);
  }
  return 0;
}


void Item_decimal::set_decimal_value(type::Decimal *value_par)
{
  class_decimal2decimal(value_par, &decimal_value);
  decimals= (uint8_t) decimal_value.frac;
  unsigned_flag= !decimal_value.sign();
  max_length= class_decimal_precision_to_length(decimal_value.intg + decimals,
                                             decimals, unsigned_flag);
}

int Item_decimal::save_in_field(Field *field, bool)
{
  field->set_notnull();
  return field->store_decimal(&decimal_value);
}


} /* namespace drizzled */
