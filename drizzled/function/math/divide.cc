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

#include <drizzled/function/math/divide.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

#include <algorithm>

using namespace std;

namespace drizzled {

double Item_func_div::real_op()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  double val2= args[1]->val_real();
  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0.0;
  if (val2 == 0.0)
  {
    signal_divide_by_null();
    return 0.0;
  }
  return fix_result(value/val2);
}


type::Decimal *Item_func_div::decimal_op(type::Decimal *decimal_value)
{
  type::Decimal value1, *val1;
  type::Decimal value2, *val2;
  int err;

  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if ((null_value= args[1]->null_value))
    return 0;
  if ((err= class_decimal_div(E_DEC_FATAL_ERROR & ~E_DEC_DIV_ZERO, decimal_value,
                           val1, val2, prec_increment)) > 3)
  {
    if (err == E_DEC_DIV_ZERO)
      signal_divide_by_null();
    null_value= 1;
    return 0;
  }
  return decimal_value;
}


void Item_func_div::result_precision()
{
  uint32_t precision= min(args[0]->decimal_precision() + prec_increment,
                          (unsigned int)DECIMAL_MAX_PRECISION);
  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag= args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag= args[0]->unsigned_flag & args[1]->unsigned_flag;

  decimals= min(args[0]->decimals + prec_increment, (unsigned int)DECIMAL_MAX_SCALE);
  max_length= class_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}


void Item_func_div::fix_length_and_dec()
{
  prec_increment= session->variables.div_precincrement;
  Item_num_op::fix_length_and_dec();

  switch(hybrid_type)
  {
  case REAL_RESULT:
  {
    decimals= max(args[0]->decimals,args[1]->decimals)+prec_increment;
    set_if_smaller(decimals, NOT_FIXED_DEC);
    max_length= args[0]->max_length - args[0]->decimals + decimals;
    uint32_t tmp= float_length(decimals);
    set_if_smaller(max_length,tmp);
    break;
  }
  case INT_RESULT:
    hybrid_type= DECIMAL_RESULT;
    result_precision();
    break;
  case DECIMAL_RESULT:
    result_precision();
    break;
  default:
    assert(0);
  }
  maybe_null= 1; // devision by zero
  return;
}

} /* namespace drizzled */
