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

#include <drizzled/item/cache_decimal.h>

namespace drizzled
{

void Item_cache_decimal::store(Item *item)
{
  type::Decimal *val= item->val_decimal_result(&decimal_value);
  if (!(null_value= item->null_value) && val != &decimal_value)
    class_decimal2decimal(val, &decimal_value);
}

double Item_cache_decimal::val_real()
{
  assert(fixed);
  double res;
  class_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &res);
  return res;
}

int64_t Item_cache_decimal::val_int()
{
  assert(fixed);
  int64_t res;
  decimal_value.val_int32(E_DEC_FATAL_ERROR, unsigned_flag, &res);
  return res;
}

String* Item_cache_decimal::val_str(String *str)
{
  assert(fixed);
  class_decimal_round(E_DEC_FATAL_ERROR, &decimal_value, decimals, false,
                   &decimal_value);
  class_decimal2string(&decimal_value, 0, str);
  return str;
}

type::Decimal *Item_cache_decimal::val_decimal(type::Decimal *)
{
  assert(fixed);
  return &decimal_value;
}


} /* namespace drizzled */
