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
#include "abs.h"
#include <drizzled/type/decimal.h>

namespace drizzled
{

double Item_func_abs::real_op()
{
  double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return fabs(value);
}


int64_t Item_func_abs::int_op()
{
  int64_t value= args[0]->val_int();
  if ((null_value= args[0]->null_value))
    return 0;
  return (value >= 0) || unsigned_flag ? value : -value;
}


type::Decimal *Item_func_abs::decimal_op(type::Decimal *decimal_value)
{
  type::Decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= args[0]->null_value))
  {
    class_decimal2decimal(value, decimal_value);
    if (decimal_value->sign())
      class_decimal_neg(decimal_value);
    return decimal_value;
  }
  return 0;
}


void Item_func_abs::fix_length_and_dec()
{
  Item_func_num1::fix_length_and_dec();
  unsigned_flag= args[0]->unsigned_flag;
}

} /* namespace drizzled */
