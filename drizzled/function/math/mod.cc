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
#include <drizzled/function/math/mod.h>
#include <drizzled/type/decimal.h>

#include <algorithm>

using namespace std;

namespace drizzled
{

int64_t Item_func_mod::int_op()
{
  assert(fixed == 1);
  int64_t value=  args[0]->val_int();
  int64_t val2= args[1]->val_int();
  int64_t result;

  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0;
  if (val2 == 0)
  {
    signal_divide_by_null();
    return 0;
  }

  if (args[0]->unsigned_flag)
    result= args[1]->unsigned_flag ?
      ((uint64_t) value) % ((uint64_t) val2) : ((uint64_t) value) % val2;
  else
    result= args[1]->unsigned_flag ?
      (int64_t)(value % ((uint64_t) val2)) : (int64_t)(value % val2);

  return result;
}

double Item_func_mod::real_op()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  double val2=  args[1]->val_real();
  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0.0;
  if (val2 == 0.0)
  {
    signal_divide_by_null();
    return 0.0;
  }
  return fmod(value,val2);
}


type::Decimal *Item_func_mod::decimal_op(type::Decimal *decimal_value)
{
  type::Decimal value1, *val1;
  type::Decimal value2, *val2;

  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if ((null_value= args[1]->null_value))
    return 0;
  switch (class_decimal_mod(E_DEC_FATAL_ERROR & ~E_DEC_DIV_ZERO, decimal_value,
                         val1, val2)) {
  case E_DEC_TRUNCATED:
  case E_DEC_OK:
    return decimal_value;
  case E_DEC_DIV_ZERO:
    signal_divide_by_null();
  default:
    null_value= 1;
    return 0;
  }
}


void Item_func_mod::result_precision()
{
  decimals= max(args[0]->decimals, args[1]->decimals);
  max_length= max(args[0]->max_length, args[1]->max_length);
}


void Item_func_mod::fix_length_and_dec()
{
  Item_num_op::fix_length_and_dec();
  maybe_null= 1;
  unsigned_flag= args[0]->unsigned_flag;
}

} /* namespace drizzled */
