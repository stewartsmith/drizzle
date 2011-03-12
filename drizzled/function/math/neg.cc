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

#include <drizzled/function/math/neg.h>

namespace drizzled
{

double Item_func_neg::real_op()
{
  double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return -value;
}


int64_t Item_func_neg::int_op()
{
  int64_t value= args[0]->val_int();
  null_value= args[0]->null_value;

  return -value;
}


type::Decimal *Item_func_neg::decimal_op(type::Decimal *decimal_value)
{
  type::Decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= args[0]->null_value))
  {
    class_decimal2decimal(value, decimal_value);
    class_decimal_neg(decimal_value);
    return decimal_value;
  }
  return 0;
}


void Item_func_neg::fix_num_length_and_dec()
{
  decimals= args[0]->decimals;
  /* 1 add because sign can appear */
  max_length= args[0]->max_length + 1;
}


void Item_func_neg::fix_length_and_dec()
{
  Item_func_num1::fix_length_and_dec();

  /*
    If this is in integer context keep the context as integer if possible
    (This is how multiplication and other integer functions works)
    Use val() to get value as arg_type doesn't mean that item is
    Item_int or Item_real due to existence of Item_param.
  */
  if (hybrid_type == INT_RESULT && args[0]->const_item())
  {
    int64_t val= args[0]->val_int();
    if ((uint64_t) val >= (uint64_t) INT64_MIN &&
        ((uint64_t) val != (uint64_t) INT64_MIN ||
          args[0]->type() != INT_ITEM))
    {
      /*
        Ensure that result is converted to DECIMAL, as int64_t can't hold
        the negated number
      */
      hybrid_type= DECIMAL_RESULT;
    }
  }
  unsigned_flag= false;
}

} /* namespace drizzled */
