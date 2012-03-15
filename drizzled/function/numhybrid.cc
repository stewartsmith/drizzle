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
#include <drizzled/function/numhybrid.h>

namespace drizzled
{

void Item_func_numhybrid::fix_num_length_and_dec()
{}

void Item_func_numhybrid::fix_length_and_dec()
{
  fix_num_length_and_dec();
  find_num_type();
}

String *Item_func_numhybrid::val_str(String *str)
{
  assert(fixed == 1);
  switch (hybrid_type) {
  case DECIMAL_RESULT:
    {
      type::Decimal decimal_value, *val;
      if (!(val= decimal_op(&decimal_value)))
        return 0;                                 // null is set
      class_decimal_round(E_DEC_FATAL_ERROR, val, decimals, false, val);
      class_decimal2string(val, 0, str);
      break;
    }
  case INT_RESULT:
    {
      int64_t nr= int_op();
      if (null_value)
        return 0;
      str->set_int(nr, unsigned_flag, &my_charset_bin);
      break;
    }
  case REAL_RESULT:
    {
      double nr= real_op();
      if (null_value)
        return 0;
      str->set_real(nr,decimals,&my_charset_bin);
      break;
    }
  case STRING_RESULT:
    return str_op(&str_value);
  case ROW_RESULT:
    assert(0);
  }
  return str;
}


double Item_func_numhybrid::val_real()
{
  assert(fixed == 1);
  switch (hybrid_type) {
  case DECIMAL_RESULT:
    {
      type::Decimal decimal_value, *val;
      double result;
      if (!(val= decimal_op(&decimal_value)))
        return 0.0;                               // null is set
      class_decimal2double(E_DEC_FATAL_ERROR, val, &result);
      return result;
    }
  case INT_RESULT:
    {
      int64_t result= int_op();
      return unsigned_flag ? (double) ((uint64_t) result) : (double) result;
    }
  case REAL_RESULT:
    return real_op();
  case STRING_RESULT:
    {
      char *end_not_used;
      int err_not_used;
      String *res= str_op(&str_value);
      return (res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
                               &end_not_used, &err_not_used) : 0.0);
    }
  case ROW_RESULT:
    assert(0);
  }

  return 0.0;
}


int64_t Item_func_numhybrid::val_int()
{
  assert(fixed == 1);
  switch (hybrid_type) {
  case DECIMAL_RESULT:
    {
      type::Decimal decimal_value, *val;
      if (!(val= decimal_op(&decimal_value)))
        return 0;                                 // null is set
      int64_t result;
      val->val_int32(E_DEC_FATAL_ERROR, unsigned_flag, &result);
      return result;
    }
  case INT_RESULT:
    return int_op();
  case REAL_RESULT:
    return (int64_t) rint(real_op());
  case STRING_RESULT:
    {
      int err_not_used;
      String *res;
      if (!(res= str_op(&str_value)))
        return 0;

      char *end= (char*) res->ptr() + res->length();
      const charset_info_st * const cs= str_value.charset();
      return (*(cs->cset->strtoll10))(cs, res->ptr(), &end, &err_not_used);
    }
  case ROW_RESULT:
    assert(0);
  }
  return 0;
}


type::Decimal *Item_func_numhybrid::val_decimal(type::Decimal *decimal_value)
{
  type::Decimal *val= decimal_value;
  assert(fixed == 1);

  switch (hybrid_type) {
  case DECIMAL_RESULT:
    val= decimal_op(decimal_value);
    break;
  case INT_RESULT:
    {
      int64_t result= int_op();
      int2_class_decimal(E_DEC_FATAL_ERROR, result, unsigned_flag, decimal_value);
      break;
    }
  case REAL_RESULT:
    {
      double result= (double)real_op();
      double2_class_decimal(E_DEC_FATAL_ERROR, result, decimal_value);
      break;
    }
  case STRING_RESULT:
    {
      String *res;
      if (!(res= str_op(&str_value)))
        return NULL;

      decimal_value->store(E_DEC_FATAL_ERROR, (char*) res->ptr(),
                           res->length(), res->charset());
      break;
    }
  case ROW_RESULT:
    assert(0);
  }

  return val;
}

} /* namespace drizzled */
