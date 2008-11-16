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
#include <mysys/my_bit.h>
#include <drizzled/slave.h>
#include <drizzled/error.h>

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
  Session *session= current_session;
  push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR, ER_DIVISION_BY_ZERO, ER(ER_DIVISION_BY_ZERO));
  null_value= 1;
}


Item *Item_func::get_tmp_table_item(Session *session)
{
  if (!with_sum_func && !const_item() && functype() != SUSERVAR_FUNC)
    return new Item_field(result_field);
  return copy_or_same(session);
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
