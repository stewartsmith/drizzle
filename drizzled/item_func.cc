/** Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/**
  @file

  @brief
  This file defines all numerical functions
*/

#include <drizzled/server_includes.h>
#include "rpl_mi.h"
#include <mysys/my_bit.h>
#include <drizzled/drizzled_error_messages.h>

bool check_reserved_words(LEX_STRING *name)
{
  if (!my_strcasecmp(system_charset_info, name->str, "GLOBAL") ||
      !my_strcasecmp(system_charset_info, name->str, "LOCAL") ||
      !my_strcasecmp(system_charset_info, name->str, "SESSION"))
    return true;
  return false;
}


/**
  @return
    true if item is a constant
*/

bool
eval_const_cond(COND *cond)
{
  return ((Item_func*) cond)->val_int() ? true : false;
}


void Item_func::fix_num_length_and_dec()
{
  uint32_t fl_length= 0;
  decimals=0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(decimals,args[i]->decimals);
    set_if_bigger(fl_length, args[i]->max_length);
  }
  max_length=float_length(decimals);
  if (fl_length > max_length)
  {
    decimals= NOT_FIXED_DEC;
    max_length= float_length(NOT_FIXED_DEC);
  }
}

/**
  Set max_length/decimals of function if function is fixed point and
  result length/precision depends on argument ones.
*/

void Item_func::count_decimal_length()
{
  int max_int_part= 0;
  decimals= 0;
  unsigned_flag= 1;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(decimals, args[i]->decimals);
    set_if_bigger(max_int_part, args[i]->decimal_int_part());
    set_if_smaller(unsigned_flag, args[i]->unsigned_flag);
  }
  int precision= cmin(max_int_part + decimals, DECIMAL_MAX_PRECISION);
  max_length= my_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}


/**
  Set max_length of if it is maximum length of its arguments.
*/

void Item_func::count_only_length()
{
  max_length= 0;
  unsigned_flag= 0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length, args[i]->max_length);
    set_if_bigger(unsigned_flag, args[i]->unsigned_flag);
  }
}


/**
  Set max_length/decimals of function if function is floating point and
  result length/precision depends on argument ones.
*/

void Item_func::count_real_length()
{
  uint32_t length= 0;
  decimals= 0;
  max_length= 0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    if (decimals != NOT_FIXED_DEC)
    {
      set_if_bigger(decimals, args[i]->decimals);
      set_if_bigger(length, (args[i]->max_length - args[i]->decimals));
    }
    set_if_bigger(max_length, args[i]->max_length);
  }
  if (decimals != NOT_FIXED_DEC)
  {
    max_length= length;
    length+= decimals;
    if (length < max_length)  // If previous operation gave overflow
      max_length= UINT32_MAX;
    else
      max_length= length;
  }
}



void Item_func::signal_divide_by_null()
{
  THD *thd= current_thd;
  push_warning(thd, DRIZZLE_ERROR::WARN_LEVEL_ERROR, ER_DIVISION_BY_ZERO, ER(ER_DIVISION_BY_ZERO));
  null_value= 1;
}


Item *Item_func::get_tmp_table_item(THD *thd)
{
  if (!with_sum_func && !const_item() && functype() != SUSERVAR_FUNC)
    return new Item_field(result_field);
  return copy_or_same(thd);
}

double Item_func_sqrt::val_real()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || value < 0)))
    return 0.0; /* purecov: inspected */
  return sqrt(value);
}

double Item_func_pow::val_real()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  double val2= args[1]->val_real();
  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0.0; /* purecov: inspected */
  return fix_result(pow(value,val2));
}

// Trigonometric functions

double Item_func_acos::val_real()
{
  assert(fixed == 1);
  // the volatile's for BUG #2338 to calm optimizer down (because of gcc's bug)
  volatile double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return acos(value);
}

double Item_func_asin::val_real()
{
  assert(fixed == 1);
  // the volatile's for BUG #2338 to calm optimizer down (because of gcc's bug)
  volatile double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return asin(value);
}

double Item_func_atan::val_real()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  if (arg_count == 2)
  {
    double val2= args[1]->val_real();
    if ((null_value=args[1]->null_value))
      return 0.0;
    return fix_result(atan2(value,val2));
  }
  return atan(value);
}

double Item_func_cos::val_real()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return cos(value);
}

double Item_func_sin::val_real()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return sin(value);
}

double Item_func_tan::val_real()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(tan(value));
}


// Shift-functions, same as << and >> in C/C++


int64_t Item_func_shift_left::val_int()
{
  assert(fixed == 1);
  uint32_t shift;
  uint64_t res= ((uint64_t) args[0]->val_int() <<
		  (shift=(uint) args[1]->val_int()));
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (shift < sizeof(int64_t)*8 ? (int64_t) res : 0L);
}

int64_t Item_func_shift_right::val_int()
{
  assert(fixed == 1);
  uint32_t shift;
  uint64_t res= (uint64_t) args[0]->val_int() >>
    (shift=(uint) args[1]->val_int());
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (shift < sizeof(int64_t)*8 ? (int64_t) res : 0);
}


int64_t Item_func_bit_neg::val_int()
{
  assert(fixed == 1);
  uint64_t res= (uint64_t) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0;
  return ~res;
}


// Conversion functions

void Item_func_integer::fix_length_and_dec()
{
  max_length=args[0]->max_length - args[0]->decimals+1;
  uint32_t tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  decimals=0;
}

void Item_func_int_val::fix_num_length_and_dec()
{
  max_length= args[0]->max_length - (args[0]->decimals ?
                                     args[0]->decimals + 1 :
                                     0) + 2;
  uint32_t tmp= float_length(decimals);
  set_if_smaller(max_length,tmp);
  decimals= 0;
}


void Item_func_int_val::find_num_type()
{
  switch(hybrid_type= args[0]->result_type())
  {
  case STRING_RESULT:
  case REAL_RESULT:
    hybrid_type= REAL_RESULT;
    max_length= float_length(decimals);
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
    /*
      -2 because in most high position can't be used any digit for int64_t
      and one position for increasing value during operation
    */
    if ((args[0]->max_length - args[0]->decimals) >=
        (DECIMAL_LONGLONG_DIGITS - 2))
    {
      hybrid_type= DECIMAL_RESULT;
    }
    else
    {
      unsigned_flag= args[0]->unsigned_flag;
      hybrid_type= INT_RESULT;
    }
    break;
  default:
    assert(0);
  }
  return;
}


int64_t Item_func_ceiling::int_op()
{
  int64_t result;
  switch (args[0]->result_type()) {
  case INT_RESULT:
    result= args[0]->val_int();
    null_value= args[0]->null_value;
    break;
  case DECIMAL_RESULT:
  {
    my_decimal dec_buf, *dec;
    if ((dec= Item_func_ceiling::decimal_op(&dec_buf)))
      my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
    else
      result= 0;
    break;
  }
  default:
    result= (int64_t)Item_func_ceiling::real_op();
  };
  return result;
}


double Item_func_ceiling::real_op()
{
  /*
    the volatile's for BUG #3051 to calm optimizer down (because of gcc's
    bug)
  */
  volatile double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return ceil(value);
}


my_decimal *Item_func_ceiling::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= (args[0]->null_value ||
                     my_decimal_ceiling(E_DEC_FATAL_ERROR, value,
                                        decimal_value) > 1)))
    return decimal_value;
  return 0;
}


int64_t Item_func_floor::int_op()
{
  int64_t result;
  switch (args[0]->result_type()) {
  case INT_RESULT:
    result= args[0]->val_int();
    null_value= args[0]->null_value;
    break;
  case DECIMAL_RESULT:
  {
    my_decimal dec_buf, *dec;
    if ((dec= Item_func_floor::decimal_op(&dec_buf)))
      my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
    else
      result= 0;
    break;
  }
  default:
    result= (int64_t)Item_func_floor::real_op();
  };
  return result;
}


double Item_func_floor::real_op()
{
  /*
    the volatile's for BUG #3051 to calm optimizer down (because of gcc's
    bug)
  */
  volatile double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return floor(value);
}


my_decimal *Item_func_floor::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= (args[0]->null_value ||
                     my_decimal_floor(E_DEC_FATAL_ERROR, value,
                                      decimal_value) > 1)))
    return decimal_value;
  return 0;
}


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
    decimals= cmin(decimals_to_set, NOT_FIXED_DEC);
    hybrid_type= REAL_RESULT;
    return;
  }
  
  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case STRING_RESULT:
    hybrid_type= REAL_RESULT;
    decimals= cmin(decimals_to_set, NOT_FIXED_DEC);
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
    decimals_to_set= cmin(DECIMAL_MAX_SCALE, decimals_to_set);
    int decimals_delta= args[0]->decimals - decimals_to_set;
    int precision= args[0]->decimal_precision();
    int length_increase= ((decimals_delta <= 0) || truncate) ? 0:1;

    precision-= decimals_delta - length_increase;
    decimals= cmin(decimals_to_set, DECIMAL_MAX_SCALE);
    max_length= my_decimal_precision_to_length(precision, decimals,
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
  volatile double tmp2;

  tmp=(abs_dec < array_elements(log_10) ?
       log_10[abs_dec] : pow(10.0,(double) abs_dec));

  if (dec_negative && std::isinf(tmp))
    tmp2= 0;
  else if (!dec_negative && std::isinf(value * tmp))
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
      ((uint64_t) value / tmp) * tmp : (value / tmp) * tmp;
  else
    value= (unsigned_flag || value >= 0) ?
      my_unsigned_round((uint64_t) value, tmp) :
      -(int64_t) my_unsigned_round((uint64_t) -value, tmp);
  return value;
}


my_decimal *Item_func_round::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  int64_t dec= args[1]->val_int();
  if (dec >= 0 || args[1]->unsigned_flag)
    dec= cmin(dec, (int64_t) decimals);
  else if (dec < INT_MIN)
    dec= INT_MIN;
    
  if (!(null_value= (args[0]->null_value || args[1]->null_value ||
                     my_decimal_round(E_DEC_FATAL_ERROR, value, (int) dec,
                                      truncate, decimal_value) > 1))) 
  {
    decimal_value->frac= decimals;
    return decimal_value;
  }
  return 0;
}


void Item_func_rand::seed_random(Item *arg)
{
  /*
    TODO: do not do reinit 'rand' for every execute of PS/SP if
    args[0] is a constant.
  */
  uint32_t tmp= (uint32_t) arg->val_int();
  randominit(rand, (uint32_t) (tmp*0x10001L+55555555L),
             (uint32_t) (tmp*0x10000001L));
}


bool Item_func_rand::fix_fields(THD *thd,Item **ref)
{
  if (Item_real_func::fix_fields(thd, ref))
    return true;
  used_tables_cache|= RAND_TABLE_BIT;
  if (arg_count)
  {					// Only use argument once in query
    /*
      No need to send a Rand log event if seed was given eg: RAND(seed),
      as it will be replicated in the query as such.
    */
    if (!rand && !(rand= (struct rand_struct*)
                   thd->alloc(sizeof(*rand))))
      return true;

    if (args[0]->const_item())
      seed_random (args[0]);
  }
  else
  {
    /*
      Save the seed only the first time RAND() is used in the query
      Once events are forwarded rather than recreated,
      the following can be skipped if inside the slave thread
    */
    if (!thd->rand_used)
    {
      thd->rand_used= 1;
      thd->rand_saved_seed1= thd->rand.seed1;
      thd->rand_saved_seed2= thd->rand.seed2;
    }
    rand= &thd->rand;
  }
  return false;
}

void Item_func_rand::update_used_tables()
{
  Item_real_func::update_used_tables();
  used_tables_cache|= RAND_TABLE_BIT;
}


double Item_func_rand::val_real()
{
  assert(fixed == 1);
  if (arg_count && !args[0]->const_item())
    seed_random (args[0]);
  return my_rnd(rand);
}

int64_t Item_func_sign::val_int()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  null_value=args[0]->null_value;
  return value < 0.0 ? -1 : (value > 0 ? 1 : 0);
}


double Item_func_units::val_real()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0;
  return value*mul+add;
}


void Item_func_min_max::fix_length_and_dec()
{
  int max_int_part=0;
  bool datetime_found= false;
  decimals=0;
  max_length=0;
  maybe_null=0;
  cmp_type=args[0]->result_type();

  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length, args[i]->max_length);
    set_if_bigger(decimals, args[i]->decimals);
    set_if_bigger(max_int_part, args[i]->decimal_int_part());
    if (args[i]->maybe_null)
      maybe_null=1;
    cmp_type=item_cmp_type(cmp_type,args[i]->result_type());
    if (args[i]->result_type() != ROW_RESULT && args[i]->is_datetime())
    {
      datetime_found= true;
      if (!datetime_item || args[i]->field_type() == DRIZZLE_TYPE_DATETIME)
        datetime_item= args[i];
    }
  }
  if (cmp_type == STRING_RESULT)
  {
    agg_arg_charsets(collation, args, arg_count, MY_COLL_CMP_CONV, 1);
    if (datetime_found)
    {
      thd= current_thd;
      compare_as_dates= true;
    }
  }
  else if ((cmp_type == DECIMAL_RESULT) || (cmp_type == INT_RESULT))
    max_length= my_decimal_precision_to_length(max_int_part+decimals, decimals,
                                            unsigned_flag);
  cached_field_type= agg_field_type(args, arg_count);
}


/*
  Compare item arguments in the DATETIME context.

  SYNOPSIS
    cmp_datetimes()
    value [out]   found least/greatest DATE/DATETIME value

  DESCRIPTION
    Compare item arguments as DATETIME values and return the index of the
    least/greatest argument in the arguments array.
    The correct integer DATE/DATETIME value of the found argument is
    stored to the value pointer, if latter is provided.

  RETURN
   0	If one of arguments is NULL
   #	index of the least/greatest argument
*/

uint32_t Item_func_min_max::cmp_datetimes(uint64_t *value)
{
  uint64_t min_max= 0;
  uint32_t min_max_idx= 0;

  for (uint32_t i=0; i < arg_count ; i++)
  {
    Item **arg= args + i;
    bool is_null;
    uint64_t res= get_datetime_value(thd, &arg, 0, datetime_item, &is_null);
    if ((null_value= args[i]->null_value))
      return 0;
    if (i == 0 || (res < min_max ? cmp_sign : -cmp_sign) > 0)
    {
      min_max= res;
      min_max_idx= i;
    }
  }
  if (value)
  {
    *value= min_max;
    if (datetime_item->field_type() == DRIZZLE_TYPE_NEWDATE)
      *value/= 1000000L;
  }
  return min_max_idx;
}


String *Item_func_min_max::val_str(String *str)
{
  assert(fixed == 1);
  if (compare_as_dates)
  {
    String *str_res;
    uint32_t min_max_idx= cmp_datetimes(NULL);
    if (null_value)
      return 0;
    str_res= args[min_max_idx]->val_str(str);
    str_res->set_charset(collation.collation);
    return str_res;
  }
  switch (cmp_type) {
  case INT_RESULT:
  {
    int64_t nr=val_int();
    if (null_value)
      return 0;
    str->set_int(nr, unsigned_flag, &my_charset_bin);
    return str;
  }
  case DECIMAL_RESULT:
  {
    my_decimal dec_buf, *dec_val= val_decimal(&dec_buf);
    if (null_value)
      return 0;
    my_decimal2string(E_DEC_FATAL_ERROR, dec_val, 0, 0, 0, str);
    return str;
  }
  case REAL_RESULT:
  {
    double nr= val_real();
    if (null_value)
      return 0; /* purecov: inspected */
    str->set_real(nr,decimals,&my_charset_bin);
    return str;
  }
  case STRING_RESULT:
  {
    String *res= NULL;

    for (uint32_t i=0; i < arg_count ; i++)
    {
      if (i == 0)
	res=args[i]->val_str(str);
      else
      {
	String *res2;
	res2= args[i]->val_str(res == str ? &tmp_value : str);
	if (res2)
	{
	  int cmp= sortcmp(res,res2,collation.collation);
	  if ((cmp_sign < 0 ? cmp : -cmp) < 0)
	    res=res2;
	}
      }
      if ((null_value= args[i]->null_value))
        return 0;
    }
    res->set_charset(collation.collation);
    return res;
  }
  case ROW_RESULT:
  default:
    // This case should never be chosen
    assert(0);
    return 0;
  }
  return 0;					// Keep compiler happy
}


double Item_func_min_max::val_real()
{
  assert(fixed == 1);
  double value=0.0;
  if (compare_as_dates)
  {
    uint64_t result= 0;
    (void)cmp_datetimes(&result);
    return (double)result;
  }
  for (uint32_t i=0; i < arg_count ; i++)
  {
    if (i == 0)
      value= args[i]->val_real();
    else
    {
      double tmp= args[i]->val_real();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
    if ((null_value= args[i]->null_value))
      break;
  }
  return value;
}


int64_t Item_func_min_max::val_int()
{
  assert(fixed == 1);
  int64_t value=0;
  if (compare_as_dates)
  {
    uint64_t result= 0;
    (void)cmp_datetimes(&result);
    return (int64_t)result;
  }
  for (uint32_t i=0; i < arg_count ; i++)
  {
    if (i == 0)
      value=args[i]->val_int();
    else
    {
      int64_t tmp=args[i]->val_int();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
    if ((null_value= args[i]->null_value))
      break;
  }
  return value;
}


my_decimal *Item_func_min_max::val_decimal(my_decimal *dec)
{
  assert(fixed == 1);
  my_decimal tmp_buf, *tmp, *res= NULL;

  if (compare_as_dates)
  {
    uint64_t value= 0;
    (void)cmp_datetimes(&value);
    uint64_t2decimal(value, dec);
    return dec;
  }
  for (uint32_t i=0; i < arg_count ; i++)
  {
    if (i == 0)
      res= args[i]->val_decimal(dec);
    else
    {
      tmp= args[i]->val_decimal(&tmp_buf);      // Zero if NULL
      if (tmp && (my_decimal_cmp(tmp, res) * cmp_sign) < 0)
      {
        if (tmp == &tmp_buf)
        {
          /* Move value out of tmp_buf as this will be reused on next loop */
          my_decimal2decimal(tmp, dec);
          res= dec;
        }
        else
          res= tmp;
      }
    }
    if ((null_value= args[i]->null_value))
    {
      res= 0;
      break;
    }
  }
  return res;
}


int64_t Item_func_length::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (int64_t) res->length();
}


int64_t Item_func_char_length::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (int64_t) res->numchars();
}


int64_t Item_func_coercibility::val_int()
{
  assert(fixed == 1);
  null_value= 0;
  return (int64_t) args[0]->collation.derivation;
}


void Item_func_locate::fix_length_and_dec()
{
  max_length= MY_INT32_NUM_DECIMAL_DIGITS;
  agg_arg_charsets(cmp_collation, args, 2, MY_COLL_CMP_CONV, 1);
}


int64_t Item_func_locate::val_int()
{
  assert(fixed == 1);
  String *a=args[0]->val_str(&value1);
  String *b=args[1]->val_str(&value2);
  if (!a || !b)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  /* must be int64_t to avoid truncation */
  int64_t start=  0; 
  int64_t start0= 0;
  my_match_t match;

  if (arg_count == 3)
  {
    start0= start= args[2]->val_int() - 1;

    if ((start < 0) || (start > a->length()))
      return 0;

    /* start is now sufficiently valid to pass to charpos function */
    start= a->charpos((int) start);

    if (start + b->length() > a->length())
      return 0;
  }

  if (!b->length())				// Found empty string at start
    return start + 1;
  
  if (!cmp_collation.collation->coll->instr(cmp_collation.collation,
                                            a->ptr()+start,
                                            (uint) (a->length()-start),
                                            b->ptr(), b->length(),
                                            &match, 1))
    return 0;
  return (int64_t) match.mb_len + start0 + 1;
}


void Item_func_locate::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("locate("));
  args[1]->print(str, query_type);
  str->append(',');
  args[0]->print(str, query_type);
  if (arg_count == 3)
  {
    str->append(',');
    args[2]->print(str, query_type);
  }
  str->append(')');
}


int64_t Item_func_field::val_int()
{
  assert(fixed == 1);

  if (cmp_type == STRING_RESULT)
  {
    String *field;
    if (!(field= args[0]->val_str(&value)))
      return 0;
    for (uint32_t i=1 ; i < arg_count ; i++)
    {
      String *tmp_value=args[i]->val_str(&tmp);
      if (tmp_value && !sortcmp(field,tmp_value,cmp_collation.collation))
        return (int64_t) (i);
    }
  }
  else if (cmp_type == INT_RESULT)
  {
    int64_t val= args[0]->val_int();
    if (args[0]->null_value)
      return 0;
    for (uint32_t i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val_int() && !args[i]->null_value)
        return (int64_t) (i);
    }
  }
  else if (cmp_type == DECIMAL_RESULT)
  {
    my_decimal dec_arg_buf, *dec_arg,
               dec_buf, *dec= args[0]->val_decimal(&dec_buf);
    if (args[0]->null_value)
      return 0;
    for (uint32_t i=1; i < arg_count; i++)
    {
      dec_arg= args[i]->val_decimal(&dec_arg_buf);
      if (!args[i]->null_value && !my_decimal_cmp(dec_arg, dec))
        return (int64_t) (i);
    }
  }
  else
  {
    double val= args[0]->val_real();
    if (args[0]->null_value)
      return 0;
    for (uint32_t i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val_real() && !args[i]->null_value)
        return (int64_t) (i);
    }
  }
  return 0;
}


void Item_func_field::fix_length_and_dec()
{
  maybe_null=0; max_length=3;
  cmp_type= args[0]->result_type();
  for (uint32_t i=1; i < arg_count ; i++)
    cmp_type= item_cmp_type(cmp_type, args[i]->result_type());
  if (cmp_type == STRING_RESULT)
    agg_arg_charsets(cmp_collation, args, arg_count, MY_COLL_CMP_CONV, 1);
}


int64_t Item_func_ascii::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (int64_t) (res->length() ? (unsigned char) (*res)[0] : (unsigned char) 0);
}

int64_t Item_func_ord::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (!res->length()) return 0;
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    register const char *str=res->ptr();
    register uint32_t n=0, l=my_ismbchar(res->charset(),str,str+res->length());
    if (!l)
      return (int64_t)((unsigned char) *str);
    while (l--)
      n=(n<<8)|(uint32_t)((unsigned char) *str++);
    return (int64_t) n;
  }
#endif
  return (int64_t) ((unsigned char) (*res)[0]);
}

	/* Search after a string in a string of strings separated by ',' */
	/* Returns number of found type >= 1 or 0 if not found */
	/* This optimizes searching in enums to bit testing! */

void Item_func_find_in_set::fix_length_and_dec()
{
  decimals=0;
  max_length=3;					// 1-999
  agg_arg_charsets(cmp_collation, args, 2, MY_COLL_CMP_CONV, 1);
}

static const char separator=',';

int64_t Item_func_find_in_set::val_int()
{
  assert(fixed == 1);
  if (enum_value)
  {
    uint64_t tmp=(uint64_t) args[1]->val_int();
    if (!(null_value=args[1]->null_value || args[0]->null_value))
    {
      if (tmp & enum_bit)
	return enum_value;
    }
    return 0L;
  }

  String *find=args[0]->val_str(&value);
  String *buffer=args[1]->val_str(&value2);
  if (!find || !buffer)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;

  int diff;
  if ((diff=buffer->length() - find->length()) >= 0)
  {
    my_wc_t wc;
    const CHARSET_INFO * const cs= cmp_collation.collation;
    const char *str_begin= buffer->ptr();
    const char *str_end= buffer->ptr();
    const char *real_end= str_end+buffer->length();
    const unsigned char *find_str= (const unsigned char *) find->ptr();
    uint32_t find_str_len= find->length();
    int position= 0;
    while (1)
    {
      int symbol_len;
      if ((symbol_len= cs->cset->mb_wc(cs, &wc, (unsigned char*) str_end, 
                                       (unsigned char*) real_end)) > 0)
      {
        const char *substr_end= str_end + symbol_len;
        bool is_last_item= (substr_end == real_end);
        bool is_separator= (wc == (my_wc_t) separator);
        if (is_separator || is_last_item)
        {
          position++;
          if (is_last_item && !is_separator)
            str_end= substr_end;
          if (!my_strnncoll(cs, (const unsigned char *) str_begin,
                            str_end - str_begin,
                            find_str, find_str_len))
            return (int64_t) position;
          else
            str_begin= substr_end;
        }
        str_end= substr_end;
      }
      else if (str_end - str_begin == 0 &&
               find_str_len == 0 &&
               wc == (my_wc_t) separator)
        return (int64_t) ++position;
      else
        return 0L;
    }
  }
  return 0;
}

int64_t Item_func_bit_count::val_int()
{
  assert(fixed == 1);
  uint64_t value= (uint64_t) args[0]->val_int();
  if ((null_value= args[0]->null_value))
    return 0; /* purecov: inspected */
  return (int64_t) my_count_bits(value);
}

/*
** User level locks
*/

pthread_mutex_t LOCK_user_locks;
static HASH hash_user_locks;

class User_level_lock
{
  unsigned char *key;
  size_t key_length;

public:
  int count;
  bool locked;
  pthread_cond_t cond;
  my_thread_id thread_id;
  void set_thread(THD *thd) { thread_id= thd->thread_id; }

  User_level_lock(const unsigned char *key_arg,uint32_t length, ulong id) 
    :key_length(length),count(1),locked(1), thread_id(id)
  {
    key= (unsigned char*) my_memdup(key_arg,length,MYF(0));
    pthread_cond_init(&cond,NULL);
    if (key)
    {
      if (my_hash_insert(&hash_user_locks,(unsigned char*) this))
      {
	free(key);
	key=0;
      }
    }
  }
  ~User_level_lock()
  {
    if (key)
    {
      hash_delete(&hash_user_locks,(unsigned char*) this);
      free(key);
    }
    pthread_cond_destroy(&cond);
  }
  inline bool initialized() { return key != 0; }
  friend void item_user_lock_release(User_level_lock *ull);
  friend unsigned char *ull_get_key(const User_level_lock *ull, size_t *length,
                            bool not_used);
};

unsigned char *ull_get_key(const User_level_lock *ull, size_t *length,
                   bool not_used __attribute__((unused)))
{
  *length= ull->key_length;
  return ull->key;
}


static bool item_user_lock_inited= 0;

void item_user_lock_init(void)
{
  pthread_mutex_init(&LOCK_user_locks,MY_MUTEX_INIT_SLOW);
  hash_init(&hash_user_locks, system_charset_info,
	    16,0,0,(hash_get_key) ull_get_key,NULL,0);
  item_user_lock_inited= 1;
}

void item_user_lock_free(void)
{
  if (item_user_lock_inited)
  {
    item_user_lock_inited= 0;
    hash_free(&hash_user_locks);
    pthread_mutex_destroy(&LOCK_user_locks);
  }
}

void item_user_lock_release(User_level_lock *ull)
{
  ull->locked=0;
  ull->thread_id= 0;
  if (--ull->count)
    pthread_cond_signal(&ull->cond);
  else
    delete ull;
}

/**
  Wait until we are at or past the given position in the master binlog
  on the slave.
*/

int64_t Item_master_pos_wait::val_int()
{
  assert(fixed == 1);
  THD* thd = current_thd;
  String *log_name = args[0]->val_str(&value);
  int event_count= 0;

  null_value=0;
  if (thd->slave_thread || !log_name || !log_name->length())
  {
    null_value = 1;
    return 0;
  }
  int64_t pos = (ulong)args[1]->val_int();
  int64_t timeout = (arg_count==3) ? args[2]->val_int() : 0 ;
  if ((event_count = active_mi->rli.wait_for_pos(thd, log_name, pos, timeout)) == -2)
  {
    null_value = 1;
    event_count=0;
  }
  return event_count;
}

#ifdef EXTRA_DEBUG
void debug_sync_point(const char* lock_name, uint32_t lock_timeout)
{
}

#endif


int64_t Item_func_last_insert_id::val_int()
{
  THD *thd= current_thd;
  assert(fixed == 1);
  if (arg_count)
  {
    int64_t value= args[0]->val_int();
    null_value= args[0]->null_value;
    /*
      LAST_INSERT_ID(X) must affect the client's mysql_insert_id() as
      documented in the manual. We don't want to touch
      first_successful_insert_id_in_cur_stmt because it would make
      LAST_INSERT_ID(X) take precedence over an generated auto_increment
      value for this row.
    */
    thd->arg_of_last_insert_id_function= true;
    thd->first_successful_insert_id_in_prev_stmt= value;
    return value;
  }
  return thd->read_first_successful_insert_id_in_prev_stmt();
}


bool Item_func_last_insert_id::fix_fields(THD *thd, Item **ref)
{
  return Item_int_func::fix_fields(thd, ref);
}


/* This function is just used to test speed of different functions */

int64_t Item_func_benchmark::val_int()
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff), &my_charset_bin);
  my_decimal tmp_decimal;
  THD *thd=current_thd;
  uint64_t loop_count;

  loop_count= (uint64_t) args[0]->val_int();

  if (args[0]->null_value ||
      (!args[0]->unsigned_flag && (((int64_t) loop_count) < 0)))
  {
    if (!args[0]->null_value)
    {
      char buff[22];
      llstr(((int64_t) loop_count), buff);
      push_warning_printf(current_thd, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_WRONG_VALUE_FOR_TYPE, ER(ER_WRONG_VALUE_FOR_TYPE),
                          "count", buff, "benchmark");
    }

    null_value= 1;
    return 0;
  }

  null_value=0;
  for (uint64_t loop=0 ; loop < loop_count && !thd->killed; loop++)
  {
    switch (args[1]->result_type()) {
    case REAL_RESULT:
      (void) args[1]->val_real();
      break;
    case INT_RESULT:
      (void) args[1]->val_int();
      break;
    case STRING_RESULT:
      (void) args[1]->val_str(&tmp);
      break;
    case DECIMAL_RESULT:
      (void) args[1]->val_decimal(&tmp_decimal);
      break;
    case ROW_RESULT:
    default:
      // This case should never be chosen
      assert(0);
      return 0;
    }
  }
  return 0;
}


void Item_func_benchmark::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("benchmark("));
  args[0]->print(str, query_type);
  str->append(',');
  args[1]->print(str, query_type);
  str->append(')');
}

#define extra_size sizeof(double)

static user_var_entry *get_variable(HASH *hash, LEX_STRING &name,
				    bool create_if_not_exists)
{
  user_var_entry *entry;

  if (!(entry = (user_var_entry*) hash_search(hash, (unsigned char*) name.str,
					      name.length)) &&
      create_if_not_exists)
  {
    uint32_t size=ALIGN_SIZE(sizeof(user_var_entry))+name.length+1+extra_size;
    if (!hash_inited(hash))
      return 0;
    if (!(entry = (user_var_entry*) my_malloc(size,MYF(MY_WME | ME_FATALERROR))))
      return 0;
    entry->name.str=(char*) entry+ ALIGN_SIZE(sizeof(user_var_entry))+
      extra_size;
    entry->name.length=name.length;
    entry->value=0;
    entry->length=0;
    entry->update_query_id=0;
    entry->collation.set(NULL, DERIVATION_IMPLICIT, 0);
    entry->unsigned_flag= 0;
    /*
      If we are here, we were called from a SET or a query which sets a
      variable. Imagine it is this:
      INSERT INTO t SELECT @a:=10, @a:=@a+1.
      Then when we have a Item_func_get_user_var (because of the @a+1) so we
      think we have to write the value of @a to the binlog. But before that,
      we have a Item_func_set_user_var to create @a (@a:=10), in this we mark
      the variable as "already logged" (line below) so that it won't be logged
      by Item_func_get_user_var (because that's not necessary).
    */
    entry->used_query_id=current_thd->query_id;
    entry->type=STRING_RESULT;
    memcpy(entry->name.str, name.str, name.length+1);
    if (my_hash_insert(hash,(unsigned char*) entry))
    {
      free((char*) entry);
      return 0;
    }
  }
  return entry;
}

/*
  When a user variable is updated (in a SET command or a query like
  SELECT @a:= ).
*/

bool Item_func_set_user_var::fix_fields(THD *thd, Item **ref)
{
  assert(fixed == 0);
  /* fix_fields will call Item_func_set_user_var::fix_length_and_dec */
  if (Item_func::fix_fields(thd, ref) ||
      !(entry= get_variable(&thd->user_vars, name, 1)))
    return true;
  /* 
     Remember the last query which updated it, this way a query can later know
     if this variable is a constant item in the query (it is if update_query_id
     is different from query_id).
  */
  entry->update_query_id= thd->query_id;
  /*
    As it is wrong and confusing to associate any 
    character set with NULL, @a should be latin2
    after this query sequence:

      SET @a=_latin2'string';
      SET @a=NULL;

    I.e. the second query should not change the charset
    to the current default value, but should keep the 
    original value assigned during the first query.
    In order to do it, we don't copy charset
    from the argument if the argument is NULL
    and the variable has previously been initialized.
  */
  null_item= (args[0]->type() == NULL_ITEM);
  if (!entry->collation.collation || !null_item)
    entry->collation.set(args[0]->collation.collation, DERIVATION_IMPLICIT);
  collation.set(entry->collation.collation, DERIVATION_IMPLICIT);
  cached_result_type= args[0]->result_type();
  return false;
}


void
Item_func_set_user_var::fix_length_and_dec()
{
  maybe_null=args[0]->maybe_null;
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  collation.set(args[0]->collation.collation, DERIVATION_IMPLICIT);
}


/*
  Mark field in read_map

  NOTES
    This is used by filesort to register used fields in a a temporary
    column read set or to register used fields in a view
*/

bool Item_func_set_user_var::register_field_in_read_map(unsigned char *arg)
{
  if (result_field)
  {
    Table *table= (Table *) arg;
    if (result_field->table == table || !table)
      bitmap_set_bit(result_field->table->read_set, result_field->field_index);
  }
  return 0;
}


/**
  Set value to user variable.

  @param entry          pointer to structure representing variable
  @param set_null       should we set NULL value ?
  @param ptr            pointer to buffer with new value
  @param length         length of new value
  @param type           type of new value
  @param cs             charset info for new value
  @param dv             derivation for new value
  @param unsigned_arg   indiates if a value of type INT_RESULT is unsigned

  @note Sets error and fatal error if allocation fails.

  @retval
    false   success
  @retval
    true    failure
*/

static bool
update_hash(user_var_entry *entry, bool set_null, void *ptr, uint32_t length,
            Item_result type, const CHARSET_INFO * const cs, Derivation dv,
            bool unsigned_arg)
{
  if (set_null)
  {
    char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
    if (entry->value && entry->value != pos)
      free(entry->value);
    entry->value= 0;
    entry->length= 0;
  }
  else
  {
    if (type == STRING_RESULT)
      length++;					// Store strings with end \0
    if (length <= extra_size)
    {
      /* Save value in value struct */
      char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
      if (entry->value != pos)
      {
	if (entry->value)
	  free(entry->value);
	entry->value=pos;
      }
    }
    else
    {
      /* Allocate variable */
      if (entry->length != length)
      {
	char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
	if (entry->value == pos)
	  entry->value=0;
        entry->value= (char*) my_realloc(entry->value, length,
                                         MYF(MY_ALLOW_ZERO_PTR | MY_WME |
                                             ME_FATALERROR));
        if (!entry->value)
	  return 1;
      }
    }
    if (type == STRING_RESULT)
    {
      length--;					// Fix length change above
      entry->value[length]= 0;			// Store end \0
    }
    memcpy(entry->value,ptr,length);
    if (type == DECIMAL_RESULT)
      ((my_decimal*)entry->value)->fix_buffer_pointer();
    entry->length= length;
    entry->collation.set(cs, dv);
    entry->unsigned_flag= unsigned_arg;
  }
  entry->type=type;
  return 0;
}


bool
Item_func_set_user_var::update_hash(void *ptr, uint32_t length,
                                    Item_result res_type,
                                    const CHARSET_INFO * const cs, Derivation dv,
                                    bool unsigned_arg)
{
  /*
    If we set a variable explicitely to NULL then keep the old
    result type of the variable
  */
  if ((null_value= args[0]->null_value) && null_item)
    res_type= entry->type;                      // Don't change type of item
  if (::update_hash(entry, (null_value= args[0]->null_value),
                    ptr, length, res_type, cs, dv, unsigned_arg))
  {
    null_value= 1;
    return 1;
  }
  return 0;
}


/** Get the value of a variable as a double. */

double user_var_entry::val_real(bool *null_value)
{
  if ((*null_value= (value == 0)))
    return 0.0;

  switch (type) {
  case REAL_RESULT:
    return *(double*) value;
  case INT_RESULT:
    return (double) *(int64_t*) value;
  case DECIMAL_RESULT:
  {
    double result;
    my_decimal2double(E_DEC_FATAL_ERROR, (my_decimal *)value, &result);
    return result;
  }
  case STRING_RESULT:
    return my_atof(value);                      // This is null terminated
  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }
  return 0.0;					// Impossible
}


/** Get the value of a variable as an integer. */

int64_t user_var_entry::val_int(bool *null_value) const
{
  if ((*null_value= (value == 0)))
    return 0L;

  switch (type) {
  case REAL_RESULT:
    return (int64_t) *(double*) value;
  case INT_RESULT:
    return *(int64_t*) value;
  case DECIMAL_RESULT:
  {
    int64_t result;
    my_decimal2int(E_DEC_FATAL_ERROR, (my_decimal *)value, 0, &result);
    return result;
  }
  case STRING_RESULT:
  {
    int error;
    return my_strtoll10(value, (char**) 0, &error);// String is null terminated
  }
  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }
  return 0L;					// Impossible
}


/** Get the value of a variable as a string. */

String *user_var_entry::val_str(bool *null_value, String *str,
				uint32_t decimals)
{
  if ((*null_value= (value == 0)))
    return (String*) 0;

  switch (type) {
  case REAL_RESULT:
    str->set_real(*(double*) value, decimals, &my_charset_bin);
    break;
  case INT_RESULT:
    if (!unsigned_flag)
      str->set(*(int64_t*) value, &my_charset_bin);
    else
      str->set(*(uint64_t*) value, &my_charset_bin);
    break;
  case DECIMAL_RESULT:
    my_decimal2string(E_DEC_FATAL_ERROR, (my_decimal *)value, 0, 0, 0, str);
    break;
  case STRING_RESULT:
    if (str->copy(value, length, collation.collation))
      str= 0;					// EOM error
  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }
  return(str);
}

/** Get the value of a variable as a decimal. */

my_decimal *user_var_entry::val_decimal(bool *null_value, my_decimal *val)
{
  if ((*null_value= (value == 0)))
    return 0;

  switch (type) {
  case REAL_RESULT:
    double2my_decimal(E_DEC_FATAL_ERROR, *(double*) value, val);
    break;
  case INT_RESULT:
    int2my_decimal(E_DEC_FATAL_ERROR, *(int64_t*) value, 0, val);
    break;
  case DECIMAL_RESULT:
    val= (my_decimal *)value;
    break;
  case STRING_RESULT:
    str2my_decimal(E_DEC_FATAL_ERROR, value, length, collation.collation, val);
    break;
  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }
  return(val);
}

/**
  This functions is invoked on SET \@variable or
  \@variable:= expression.

  Evaluate (and check expression), store results.

  @note
    For now it always return OK. All problem with value evaluating
    will be caught by thd->is_error() check in sql_set_variables().

  @retval
    false OK.
*/

bool
Item_func_set_user_var::check(bool use_result_field)
{
  if (use_result_field && !result_field)
    use_result_field= false;

  switch (cached_result_type) {
  case REAL_RESULT:
  {
    save_result.vreal= use_result_field ? result_field->val_real() :
                        args[0]->val_real();
    break;
  }
  case INT_RESULT:
  {
    save_result.vint= use_result_field ? result_field->val_int() :
                       args[0]->val_int();
    unsigned_flag= use_result_field ? ((Field_num*)result_field)->unsigned_flag:
                    args[0]->unsigned_flag;
    break;
  }
  case STRING_RESULT:
  {
    save_result.vstr= use_result_field ? result_field->val_str(&value) :
                       args[0]->val_str(&value);
    break;
  }
  case DECIMAL_RESULT:
  {
    save_result.vdec= use_result_field ?
                       result_field->val_decimal(&decimal_buff) :
                       args[0]->val_decimal(&decimal_buff);
    break;
  }
  case ROW_RESULT:
  default:
    // This case should never be chosen
    assert(0);
    break;
  }
  return(false);
}


/**
  This functions is invoked on
  SET \@variable or \@variable:= expression.

  @note
    We have to store the expression as such in the variable, independent of
    the value method used by the user

  @retval
    0	OK
  @retval
    1	EOM Error

*/

bool
Item_func_set_user_var::update()
{
  bool res= false;

  switch (cached_result_type) {
  case REAL_RESULT:
  {
    res= update_hash((void*) &save_result.vreal,sizeof(save_result.vreal),
		     REAL_RESULT, &my_charset_bin, DERIVATION_IMPLICIT, 0);
    break;
  }
  case INT_RESULT:
  {
    res= update_hash((void*) &save_result.vint, sizeof(save_result.vint),
                     INT_RESULT, &my_charset_bin, DERIVATION_IMPLICIT,
                     unsigned_flag);
    break;
  }
  case STRING_RESULT:
  {
    if (!save_result.vstr)					// Null value
      res= update_hash((void*) 0, 0, STRING_RESULT, &my_charset_bin,
		       DERIVATION_IMPLICIT, 0);
    else
      res= update_hash((void*) save_result.vstr->ptr(),
		       save_result.vstr->length(), STRING_RESULT,
		       save_result.vstr->charset(),
		       DERIVATION_IMPLICIT, 0);
    break;
  }
  case DECIMAL_RESULT:
  {
    if (!save_result.vdec)					// Null value
      res= update_hash((void*) 0, 0, DECIMAL_RESULT, &my_charset_bin,
                       DERIVATION_IMPLICIT, 0);
    else
      res= update_hash((void*) save_result.vdec,
                       sizeof(my_decimal), DECIMAL_RESULT,
                       &my_charset_bin, DERIVATION_IMPLICIT, 0);
    break;
  }
  case ROW_RESULT:
  default:
    // This case should never be chosen
    assert(0);
    break;
  }
  return(res);
}


double Item_func_set_user_var::val_real()
{
  assert(fixed == 1);
  check(0);
  update();					// Store expression
  return entry->val_real(&null_value);
}

int64_t Item_func_set_user_var::val_int()
{
  assert(fixed == 1);
  check(0);
  update();					// Store expression
  return entry->val_int(&null_value);
}

String *Item_func_set_user_var::val_str(String *str)
{
  assert(fixed == 1);
  check(0);
  update();					// Store expression
  return entry->val_str(&null_value, str, decimals);
}


my_decimal *Item_func_set_user_var::val_decimal(my_decimal *val)
{
  assert(fixed == 1);
  check(0);
  update();					// Store expression
  return entry->val_decimal(&null_value, val);
}


double Item_func_set_user_var::val_result()
{
  assert(fixed == 1);
  check(true);
  update();					// Store expression
  return entry->val_real(&null_value);
}

int64_t Item_func_set_user_var::val_int_result()
{
  assert(fixed == 1);
  check(true);
  update();					// Store expression
  return entry->val_int(&null_value);
}

String *Item_func_set_user_var::str_result(String *str)
{
  assert(fixed == 1);
  check(true);
  update();					// Store expression
  return entry->val_str(&null_value, str, decimals);
}


my_decimal *Item_func_set_user_var::val_decimal_result(my_decimal *val)
{
  assert(fixed == 1);
  check(true);
  update();					// Store expression
  return entry->val_decimal(&null_value, val);
}


void Item_func_set_user_var::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("(@"));
  str->append(name.str, name.length);
  str->append(STRING_WITH_LEN(":="));
  args[0]->print(str, query_type);
  str->append(')');
}


void Item_func_set_user_var::print_as_stmt(String *str,
                                           enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("set @"));
  str->append(name.str, name.length);
  str->append(STRING_WITH_LEN(":="));
  args[0]->print(str, query_type);
  str->append(')');
}

bool Item_func_set_user_var::send(Protocol *protocol, String *str_arg)
{
  if (result_field)
  {
    check(1);
    update();
    return protocol->store(result_field);
  }
  return Item::send(protocol, str_arg);
}

void Item_func_set_user_var::make_field(Send_field *tmp_field)
{
  if (result_field)
  {
    result_field->make_field(tmp_field);
    assert(tmp_field->table_name != 0);
    if (Item::name)
      tmp_field->col_name=Item::name;               // Use user supplied name
  }
  else
    Item::make_field(tmp_field);
}


/*
  Save the value of a user variable into a field

  SYNOPSIS
    save_in_field()
      field           target field to save the value to
      no_conversion   flag indicating whether conversions are allowed

  DESCRIPTION
    Save the function value into a field and update the user variable
    accordingly. If a result field is defined and the target field doesn't
    coincide with it then the value from the result field will be used as
    the new value of the user variable.

    The reason to have this method rather than simply using the result
    field in the val_xxx() methods is that the value from the result field
    not always can be used when the result field is defined.
    Let's consider the following cases:
    1) when filling a tmp table the result field is defined but the value of it
    is undefined because it has to be produced yet. Thus we can't use it.
    2) on execution of an INSERT ... SELECT statement the save_in_field()
    function will be called to fill the data in the new record. If the SELECT
    part uses a tmp table then the result field is defined and should be
    used in order to get the correct result.

    The difference between the SET_USER_VAR function and regular functions
    like CONCAT is that the Item_func objects for the regular functions are
    replaced by Item_field objects after the values of these functions have
    been stored in a tmp table. Yet an object of the Item_field class cannot
    be used to update a user variable.
    Due to this we have to handle the result field in a special way here and
    in the Item_func_set_user_var::send() function.

  RETURN VALUES
    false       Ok
    true        Error
*/

int Item_func_set_user_var::save_in_field(Field *field, bool no_conversions,
                                          bool can_use_result_field)
{
  bool use_result_field= (!can_use_result_field ? 0 :
                          (result_field && result_field != field));
  int error;

  /* Update the value of the user variable */
  check(use_result_field);
  update();

  if (result_type() == STRING_RESULT ||
      (result_type() == REAL_RESULT && field->result_type() == STRING_RESULT))
  {
    String *result;
    const CHARSET_INFO * const cs= collation.collation;
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff, sizeof(buff), cs);
    result= entry->val_str(&null_value, &str_value, decimals);

    if (null_value)
    {
      str_value.set_quick(0, 0, cs);
      return set_field_to_null_with_conversions(field, no_conversions);
    }

    /* NOTE: If null_value == false, "result" must be not NULL.  */

    field->set_notnull();
    error=field->store(result->ptr(),result->length(),cs);
    str_value.set_quick(0, 0, cs);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr= entry->val_real(&null_value);
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store(nr);
  }
  else if (result_type() == DECIMAL_RESULT)
  {
    my_decimal decimal_value;
    my_decimal *val= entry->val_decimal(&null_value, &decimal_value);
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store_decimal(val);
  }
  else
  {
    int64_t nr= entry->val_int(&null_value);
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(nr, unsigned_flag);
  }
  return error;
}


String *
Item_func_get_user_var::val_str(String *str)
{
  assert(fixed == 1);
  if (!var_entry)
    return((String*) 0);			// No such variable
  return(var_entry->val_str(&null_value, str, decimals));
}


double Item_func_get_user_var::val_real()
{
  assert(fixed == 1);
  if (!var_entry)
    return 0.0;					// No such variable
  return (var_entry->val_real(&null_value));
}


my_decimal *Item_func_get_user_var::val_decimal(my_decimal *dec)
{
  assert(fixed == 1);
  if (!var_entry)
    return 0;
  return var_entry->val_decimal(&null_value, dec);
}


int64_t Item_func_get_user_var::val_int()
{
  assert(fixed == 1);
  if (!var_entry)
    return 0L;				// No such variable
  return (var_entry->val_int(&null_value));
}


/**
  Get variable by name and, if necessary, put the record of variable 
  use into the binary log.

  When a user variable is invoked from an update query (INSERT, UPDATE etc),
  stores this variable and its value in thd->user_var_events, so that it can be
  written to the binlog (will be written just before the query is written, see
  log.cc).

  @param      thd        Current thread
  @param      name       Variable name
  @param[out] out_entry  variable structure or NULL. The pointer is set
                         regardless of whether function succeeded or not.

  @retval
    0  OK
  @retval
    1  Failed to put appropriate record into binary log

*/

int get_var_with_binlog(THD *thd, enum_sql_command sql_command,
                        LEX_STRING &name, user_var_entry **out_entry)
{
  BINLOG_USER_VAR_EVENT *user_var_event;
  user_var_entry *var_entry;
  var_entry= get_variable(&thd->user_vars, name, 0);

  /*
    Any reference to user-defined variable which is done from stored
    function or trigger affects their execution and the execution of the
    calling statement. We must log all such variables even if they are 
    not involved in table-updating statements.
  */
  if (!(opt_bin_log && is_update_query(sql_command)))
  {
    *out_entry= var_entry;
    return 0;
  }

  if (!var_entry)
  {
    /*
      If the variable does not exist, it's NULL, but we want to create it so
      that it gets into the binlog (if it didn't, the slave could be
      influenced by a variable of the same name previously set by another
      thread).
      We create it like if it had been explicitly set with SET before.
      The 'new' mimics what sql_yacc.yy does when 'SET @a=10;'.
      sql_set_variables() is what is called from 'case SQLCOM_SET_OPTION'
      in dispatch_command()). Instead of building a one-element list to pass to
      sql_set_variables(), we could instead manually call check() and update();
      this would save memory and time; but calling sql_set_variables() makes
      one unique place to maintain (sql_set_variables()). 

      Manipulation with lex is necessary since free_underlaid_joins
      is going to release memory belonging to the main query.
    */

    List<set_var_base> tmp_var_list;
    LEX *sav_lex= thd->lex, lex_tmp;
    thd->lex= &lex_tmp;
    lex_start(thd);
    tmp_var_list.push_back(new set_var_user(new Item_func_set_user_var(name,
                                                                       new Item_null())));
    /* Create the variable */
    if (sql_set_variables(thd, &tmp_var_list))
    {
      thd->lex= sav_lex;
      goto err;
    }
    thd->lex= sav_lex;
    if (!(var_entry= get_variable(&thd->user_vars, name, 0)))
      goto err;
  }
  else if (var_entry->used_query_id == thd->query_id ||
           mysql_bin_log.is_query_in_union(thd, var_entry->used_query_id))
  {
    /* 
       If this variable was already stored in user_var_events by this query
       (because it's used in more than one place in the query), don't store
       it.
    */
    *out_entry= var_entry;
    return 0;
  }

  uint32_t size;
  /*
    First we need to store value of var_entry, when the next situation
    appears:
    > set @a:=1;
    > insert into t1 values (@a), (@a:=@a+1), (@a:=@a+1);
    We have to write to binlog value @a= 1.

    We allocate the user_var_event on user_var_events_alloc pool, not on
    the this-statement-execution pool because in SPs user_var_event objects 
    may need to be valid after current [SP] statement execution pool is
    destroyed.
  */
  size= ALIGN_SIZE(sizeof(BINLOG_USER_VAR_EVENT)) + var_entry->length;
  if (!(user_var_event= (BINLOG_USER_VAR_EVENT *)
        alloc_root(thd->user_var_events_alloc, size)))
    goto err;

  user_var_event->value= (char*) user_var_event +
    ALIGN_SIZE(sizeof(BINLOG_USER_VAR_EVENT));
  user_var_event->user_var_event= var_entry;
  user_var_event->type= var_entry->type;
  user_var_event->charset_number= var_entry->collation.collation->number;
  if (!var_entry->value)
  {
    /* NULL value*/
    user_var_event->length= 0;
    user_var_event->value= 0;
  }
  else
  {
    user_var_event->length= var_entry->length;
    memcpy(user_var_event->value, var_entry->value,
           var_entry->length);
  }
  /* Mark that this variable has been used by this query */
  var_entry->used_query_id= thd->query_id;
  if (insert_dynamic(&thd->user_var_events, (unsigned char*) &user_var_event))
    goto err;

  *out_entry= var_entry;
  return 0;

err:
  *out_entry= var_entry;
  return 1;
}

void Item_func_get_user_var::fix_length_and_dec()
{
  THD *thd=current_thd;
  int error;
  maybe_null=1;
  decimals=NOT_FIXED_DEC;
  max_length=MAX_BLOB_WIDTH;

  error= get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry);

  /*
    If the variable didn't exist it has been created as a STRING-type.
    'var_entry' is NULL only if there occured an error during the call to
    get_var_with_binlog.
  */
  if (var_entry)
  {
    m_cached_result_type= var_entry->type;
    unsigned_flag= var_entry->unsigned_flag;
    max_length= var_entry->length;

    collation.set(var_entry->collation);
    switch(m_cached_result_type) {
    case REAL_RESULT:
      max_length= DBL_DIG + 8;
      break;
    case INT_RESULT:
      max_length= MAX_BIGINT_WIDTH;
      decimals=0;
      break;
    case STRING_RESULT:
      max_length= MAX_BLOB_WIDTH;
      break;
    case DECIMAL_RESULT:
      max_length= DECIMAL_MAX_STR_LENGTH;
      decimals= DECIMAL_MAX_SCALE;
      break;
    case ROW_RESULT:                            // Keep compiler happy
    default:
      assert(0);
      break;
    }
  }
  else
  {
    collation.set(&my_charset_bin, DERIVATION_IMPLICIT);
    null_value= 1;
    m_cached_result_type= STRING_RESULT;
    max_length= MAX_BLOB_WIDTH;
  }
}


bool Item_func_get_user_var::const_item() const
{
  return (!var_entry || current_thd->query_id != var_entry->update_query_id);
}


enum Item_result Item_func_get_user_var::result_type() const
{
  return m_cached_result_type;
}


void Item_func_get_user_var::print(String *str,
                                   enum_query_type query_type __attribute__((unused)))
{
  str->append(STRING_WITH_LEN("(@"));
  str->append(name.str,name.length);
  str->append(')');
}


bool Item_func_get_user_var::eq(const Item *item,
                                bool binary_cmp __attribute__((unused))) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;					// Same item is same.
  /* Check if other type is also a get_user_var() object */
  if (item->type() != FUNC_ITEM ||
      ((Item_func*) item)->functype() != functype())
    return 0;
  Item_func_get_user_var *other=(Item_func_get_user_var*) item;
  return (name.length == other->name.length &&
	  !memcmp(name.str, other->name.str, name.length));
}


bool Item_user_var_as_out_param::fix_fields(THD *thd, Item **ref)
{
  assert(fixed == 0);
  if (Item::fix_fields(thd, ref) ||
      !(entry= get_variable(&thd->user_vars, name, 1)))
    return true;
  entry->type= STRING_RESULT;
  /*
    Let us set the same collation which is used for loading
    of fields in LOAD DATA INFILE.
    (Since Item_user_var_as_out_param is used only there).
  */
  entry->collation.set(thd->variables.collation_database);
  entry->update_query_id= thd->query_id;
  return false;
}


void Item_user_var_as_out_param::set_null_value(const CHARSET_INFO * const cs)
{
  ::update_hash(entry, true, 0, 0, STRING_RESULT, cs,
                DERIVATION_IMPLICIT, 0 /* unsigned_arg */);
}


void Item_user_var_as_out_param::set_value(const char *str, uint32_t length,
                                           const CHARSET_INFO * const cs)
{
  ::update_hash(entry, false, (void*)str, length, STRING_RESULT, cs,
                DERIVATION_IMPLICIT, 0 /* unsigned_arg */);
}


double Item_user_var_as_out_param::val_real()
{
  assert(0);
  return 0.0;
}


int64_t Item_user_var_as_out_param::val_int()
{
  assert(0);
  return 0;
}


String* Item_user_var_as_out_param::val_str(String *str __attribute__((unused)))
{
  assert(0);
  return 0;
}


my_decimal* Item_user_var_as_out_param::val_decimal(my_decimal *decimal_buffer __attribute__((unused)))
{
  assert(0);
  return 0;
}


void Item_user_var_as_out_param::print(String *str,
                                       enum_query_type query_type __attribute__((unused)))
{
  str->append('@');
  str->append(name.str,name.length);
}


Item_func_get_system_var::
Item_func_get_system_var(sys_var *var_arg, enum_var_type var_type_arg,
                       LEX_STRING *component_arg, const char *name_arg,
                       size_t name_len_arg)
  :var(var_arg), var_type(var_type_arg), component(*component_arg)
{
  /* set_name() will allocate the name */
  set_name(name_arg, name_len_arg, system_charset_info);
}


bool
Item_func_get_system_var::fix_fields(THD *thd, Item **ref)
{
  Item *item;

  /*
    Evaluate the system variable and substitute the result (a basic constant)
    instead of this item. If the variable can not be evaluated,
    the error is reported in sys_var::item().
  */
  if (!(item= var->item(thd, var_type, &component)))
    return(1);                             // Impossible
  item->set_name(name, 0, system_charset_info); // don't allocate a new name
  thd->change_item_tree(ref, item);

  return(0);
}


bool Item_func_get_system_var::is_written_to_binlog()
{
  return var->is_written_to_binlog(var_type);
}

int64_t Item_func_bit_xor::val_int()
{
  assert(fixed == 1);
  uint64_t arg1= (uint64_t) args[0]->val_int();
  uint64_t arg2= (uint64_t) args[1]->val_int();
  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return 0;
  return (int64_t) (arg1 ^ arg2);
}


/***************************************************************************
  System variables
****************************************************************************/

/**
  Return value of an system variable base[.name] as a constant item.

  @param thd			Thread handler
  @param var_type		global / session
  @param name		        Name of base or system variable
  @param component		Component.

  @note
    If component.str = 0 then the variable name is in 'name'

  @return
    - 0  : error
    - #  : constant item
*/


Item *get_system_var(THD *thd, enum_var_type var_type, LEX_STRING name,
		     LEX_STRING component)
{
  sys_var *var;
  LEX_STRING *base_name, *component_name;

  if (component.str)
  {
    base_name= &component;
    component_name= &name;
  }
  else
  {
    base_name= &name;
    component_name= &component;			// Empty string
  }

  if (!(var= find_sys_var(thd, base_name->str, base_name->length)))
    return 0;
  if (component.str)
  {
    if (!var->is_struct())
    {
      my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), base_name->str);
      return 0;
    }
  }

  set_if_smaller(component_name->length, MAX_SYS_VAR_LENGTH);

  return new Item_func_get_system_var(var, var_type, component_name,
                                      NULL, 0);
}


/**
  Check a user level lock.

  Sets null_value=true on error.

  @retval
    1		Available
  @retval
    0		Already taken, or error
*/

int64_t Item_func_is_free_lock::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  User_level_lock *ull;

  null_value=0;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }
  
  pthread_mutex_lock(&LOCK_user_locks);
  ull= (User_level_lock *) hash_search(&hash_user_locks, (unsigned char*) res->ptr(),
                                       (size_t) res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 1;
  return 0;
}

int64_t Item_func_is_used_lock::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  User_level_lock *ull;

  null_value=1;
  if (!res || !res->length())
    return 0;
  
  pthread_mutex_lock(&LOCK_user_locks);
  ull= (User_level_lock *) hash_search(&hash_user_locks, (unsigned char*) res->ptr(),
                                       (size_t) res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 0;

  null_value=0;
  return ull->thread_id;
}


int64_t Item_func_row_count::val_int()
{
  assert(fixed == 1);
  THD *thd= current_thd;

  return thd->row_count_func;
}

int64_t Item_func_found_rows::val_int()
{
  assert(fixed == 1);
  THD *thd= current_thd;

  return thd->found_rows();
}
