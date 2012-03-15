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

#include <drizzled/function/min_max.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/session.h>

namespace drizzled
{

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
      compare_as_dates= true;
    }
  }
  else if ((cmp_type == DECIMAL_RESULT) || (cmp_type == INT_RESULT))
    max_length= class_decimal_precision_to_length(max_int_part+decimals, decimals,
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
   0	If one of arguments is NULL or there was a execution error
   #	index of the least/greatest argument
*/

uint32_t Item_func_min_max::cmp_datetimes(uint64_t *value)
{
  uint64_t min_max= 0;
  uint32_t min_max_idx= 0;

  for (uint32_t i=0; i < arg_count ; i++)
  {
    Item **arg= args + i;
    bool is_null_unused;
    uint64_t res= get_datetime_value(&getSession(), &arg, 0, datetime_item,
                                     &is_null_unused);

    /* Check if we need to stop (because of error or KILL)  and stop the loop */
    if (getSession().is_error())
    {
      null_value= 1;
      return 0;
    }

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
    if (datetime_item->field_type() == DRIZZLE_TYPE_DATE)
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
    if (args[min_max_idx]->null_value)
    {
      // check if the call to val_str() above returns a NULL value
      null_value= 1;
      return NULL;
    }
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
      type::Decimal dec_buf, *dec_val= val_decimal(&dec_buf);
      if (null_value)
        return 0;
      class_decimal2string(dec_val, 0, str);
      return str;
    }

  case REAL_RESULT:
    {
      double nr= val_real();
      if (null_value)
        return 0;
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


type::Decimal *Item_func_min_max::val_decimal(type::Decimal *dec)
{
  assert(fixed == 1);
  type::Decimal tmp_buf, *tmp, *res= NULL;

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
      if (tmp && (class_decimal_cmp(tmp, res) * cmp_sign) < 0)
      {
        if (tmp == &tmp_buf)
        {
          /* Move value out of tmp_buf as this will be reused on next loop */
          class_decimal2decimal(tmp, dec);
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

} /* namespace drizzled */
