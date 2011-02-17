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
#include <limits.h>

#include <limits>
#include <algorithm>

#include <drizzled/function/math/round.h>
#include <drizzled/util/test.h>

namespace drizzled
{

extern const double log_10[309];


using namespace std;

void Item_func_round::fix_length_and_dec()
{
  int      decimals_to_set;
  int64_t val1;
  bool     val1_unsigned;

  unsigned_flag= args[0]->unsigned_flag;
  if (!args[1]->const_item())
  {
    max_length= args[0]->max_length;
    decimals= args[0]->decimals;
    if (args[0]->result_type() == DECIMAL_RESULT)
    {
      max_length++;
      hybrid_type= DECIMAL_RESULT;
    }
    else
      hybrid_type= REAL_RESULT;
    return;
  }

  val1= args[1]->val_int();
  val1_unsigned= args[1]->unsigned_flag;
  if (val1 < 0)
    decimals_to_set= val1_unsigned ? INT_MAX : 0;
  else
    decimals_to_set= (val1 > INT_MAX) ? INT_MAX : (int) val1;

  if (args[0]->decimals == NOT_FIXED_DEC)
  {
    max_length= args[0]->max_length;
    decimals= min(decimals_to_set, (int)NOT_FIXED_DEC);
    hybrid_type= REAL_RESULT;
    return;
  }

  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case STRING_RESULT:
    hybrid_type= REAL_RESULT;
    decimals= min(decimals_to_set, (int)NOT_FIXED_DEC);
    max_length= float_length(decimals);
    break;
  case INT_RESULT:
    if ((!decimals_to_set && truncate) || (args[0]->decimal_precision() < DECIMAL_LONGLONG_DIGITS))
    {
      int length_can_increase= test(!truncate && (val1 < 0) && !val1_unsigned);
      max_length= args[0]->max_length + length_can_increase;
      /* Here we can keep INT_RESULT */
      hybrid_type= INT_RESULT;
      decimals= 0;
      break;
    }
    /* fall through */
  case DECIMAL_RESULT:
  {
    hybrid_type= DECIMAL_RESULT;
    decimals_to_set= min(DECIMAL_MAX_SCALE, decimals_to_set);
    int decimals_delta= args[0]->decimals - decimals_to_set;
    int precision= args[0]->decimal_precision();
    int length_increase= ((decimals_delta <= 0) || truncate) ? 0:1;

    precision-= decimals_delta - length_increase;
    decimals= min(decimals_to_set, DECIMAL_MAX_SCALE);
    max_length= class_decimal_precision_to_length(precision, decimals,
                                               unsigned_flag);
    break;
  }
  default:
    assert(0); /* This result type isn't handled */
  }
}

double my_double_round(double value, int64_t dec, bool dec_unsigned,
                       bool truncate)
{
  double tmp;
  bool dec_negative= (dec < 0) && !dec_unsigned;
  uint64_t abs_dec= dec_negative ? -dec : dec;
  /*
    tmp2 is here to avoid return the value with 80 bit precision
    This will fix that the test round(0.1,1) = round(0.1,1) is true
  */
  double tmp2;

  tmp=(abs_dec < array_elements(log_10) ?
       log_10[abs_dec] : pow(10.0,(double) abs_dec));

  double value_times_tmp= value * tmp;

  /*
    NOTE: This is a workaround for a gcc 4.3 bug on Intel x86 32bit
    See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39228
    See http://bugs.mysql.com/bug.php?id=42965

    This forces the compiler to store/load the value as 64bit and avoids
    an optimisation that *could* have the infinite check be done on the 80bit
    representation.
   */
  if(sizeof(double) < sizeof(double_t))
  {
    volatile double t= value_times_tmp;
    value_times_tmp= t;
  }

  double infinity= numeric_limits<double>::infinity();
  if (dec_negative && (tmp == infinity))
    tmp2= 0;
  else if (!dec_negative && (value_times_tmp == infinity))
    tmp2= value;
  else if (truncate)
  {
    if (value >= 0)
      tmp2= dec < 0 ? floor(value/tmp)*tmp : floor(value*tmp)/tmp;
    else
      tmp2= dec < 0 ? ceil(value/tmp)*tmp : ceil(value*tmp)/tmp;
  }
  else
    tmp2=dec < 0 ? rint(value/tmp)*tmp : rint(value*tmp)/tmp;
  return tmp2;
}


double Item_func_round::real_op()
{
  double value= args[0]->val_real();

  if (!(null_value= args[0]->null_value || args[1]->null_value))
    return my_double_round(value, args[1]->val_int(), args[1]->unsigned_flag,
                           truncate);

  return 0.0;
}

/*
  Rounds a given value to a power of 10 specified as the 'to' argument,
  avoiding overflows when the value is close to the uint64_t range boundary.
*/

static inline uint64_t my_unsigned_round(uint64_t value, uint64_t to)
{
  uint64_t tmp= value / to * to;
  return (value - tmp < (to >> 1)) ? tmp : tmp + to;
}


int64_t Item_func_round::int_op()
{
  int64_t value= args[0]->val_int();
  int64_t dec= args[1]->val_int();
  decimals= 0;
  uint64_t abs_dec;
  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0;
  if ((dec >= 0) || args[1]->unsigned_flag)
    return value; // integer have not digits after point

  abs_dec= -dec;
  int64_t tmp;

  if(abs_dec >= array_elements(log_10_int))
    return 0;

  tmp= log_10_int[abs_dec];

  if (truncate)
    value= (unsigned_flag) ?
      (int64_t)(((uint64_t) value / tmp) * tmp) : (value / tmp) * tmp;
  else
    value= (unsigned_flag || value >= 0) ?
      (int64_t)(my_unsigned_round((uint64_t) value, tmp)) :
      -(int64_t) my_unsigned_round((uint64_t) -value, tmp);
  return value;
}


type::Decimal *Item_func_round::decimal_op(type::Decimal *decimal_value)
{
  type::Decimal val, *value= args[0]->val_decimal(&val);
  int64_t dec= args[1]->val_int();

  if (dec >= 0 || args[1]->unsigned_flag)
    dec= min(dec, (int64_t) decimals);
  else if (dec < INT_MIN)
    dec= INT_MIN;

  if (!(null_value= (args[0]->null_value || args[1]->null_value ||
                     class_decimal_round(E_DEC_FATAL_ERROR, value, (int) dec,
                                      truncate, decimal_value) > 1)))
  {
    decimal_value->frac= decimals;
    return decimal_value;
  }
  return 0;
}

} /* namespace drizzled */
