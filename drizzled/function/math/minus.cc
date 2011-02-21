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

#include <drizzled/function/math/minus.h>
#include <drizzled/type/decimal.h>

namespace drizzled
{

/**
  The following function is here to allow the user to force
  subtraction of UNSIGNED BIGINT to return negative values.
*/

void Item_func_minus::fix_length_and_dec()
{
  Item_num_op::fix_length_and_dec();
  if (unsigned_flag)
    unsigned_flag= 0;
}


double Item_func_minus::real_op()
{
  double value= args[0]->val_real() - args[1]->val_real();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return fix_result(value);
}


int64_t Item_func_minus::int_op()
{
  int64_t value=args[0]->val_int() - args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return value;
}

/**
  See Item_func_plus::decimal_op for comments.
*/

type::Decimal *Item_func_minus::decimal_op(type::Decimal *decimal_value)
{
  type::Decimal value1, *val1;
  type::Decimal value2, *val2=

  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if (!(null_value= (args[1]->null_value ||
                     (class_decimal_sub(E_DEC_FATAL_ERROR, decimal_value, val1,
                                     val2) > 3))))
    return decimal_value;
  return 0;
}

} /* namespace drizzled */
