/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include "drizzled/server_includes.h"
#include CSTDINT_H
#include "drizzled/temporal.h"
#include "drizzled/error.h"
#include "drizzled/function/time/year.h"

int64_t Item_func_year::val_int()
{
  assert(fixed);

  if (args[0]->is_null())
  {
    /* For NULL argument, we return a NULL result */
    null_value= true;
    return 0;
  }

  /* Grab the first argument as a DateTime object */
  drizzled::DateTime temporal;
  Item_result arg0_result_type= args[0]->result_type();
  
  switch (arg0_result_type)
  {
    case STRING_RESULT:
      {
        char buff[40];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res= args[0]->val_str(&tmp);
        if (! temporal.from_string(res->c_ptr(), res->length()))
        {
          /* 
          * Could not interpret the function argument as a temporal value, 
          * so throw an error and return 0
          */
          my_error(ER_INVALID_DATETIME_VALUE, MYF(0), res->c_ptr());
          return 0;
        }
      }
      break;
    case INT_RESULT:
      if (! temporal.from_int64_t(args[0]->val_int()))
      {
        /* 
        * Could not interpret the function argument as a temporal value, 
        * so throw an error and return 0
        */
        null_value= true;
        char buff[40];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res;

        res= args[0]->val_str(&tmp);

        my_error(ER_INVALID_DATETIME_VALUE, MYF(0), res->c_ptr());
        return 0;
      }
      break;
    default:
      {
        /* 
        * Could not interpret the function argument as a temporal value, 
        * so throw an error and return 0
        */
        null_value= true;
        char buff[40];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res;

        res= args[0]->val_str(&tmp);

        my_error(ER_INVALID_DATETIME_VALUE, MYF(0), res->c_ptr());
        return 0;
      }
  }
  return (int64_t) temporal.years();
}

/* information about this Item tree monotonicity

  SYNOPSIS
    Item_func_year::get_monotonicity_info()

  DESCRIPTION
  Get information about monotonicity of the function represented by this item
  tree.

  RETURN
    See enum_monotonicity_info.
*/

enum_monotonicity_info Item_func_year::get_monotonicity_info() const
{
  if (args[0]->type() == Item::FIELD_ITEM &&
      (args[0]->field_type() == DRIZZLE_TYPE_DATE ||
       args[0]->field_type() == DRIZZLE_TYPE_DATETIME))
    return MONOTONIC_INCREASING;
  return NON_MONOTONIC;
}

int64_t Item_func_year::val_int_endpoint(bool left_endp, bool *incl_endp)
{
  assert(fixed == 1);

  if (args[0]->is_null())
  {
    /* got NULL, leave the incl_endp intact */
    return INT64_MIN;
  }

  /* Grab the first argument as a DateTime object */
  drizzled::DateTime temporal;
  
  if (! temporal.from_int64_t(args[0]->val_int()))
  {
    /* got bad DateTime, leave the incl_endp intact */
    return INT64_MIN;
  }

  /*
    Handle the special but practically useful case of datetime values that
    point to year bound ("strictly less" comparison stays intact) :

      col < '2007-01-01 00:00:00'  -> YEAR(col) <  2007

    which is different from the general case ("strictly less" changes to
    "less or equal"):

      col < '2007-09-15 23:00:00'  -> YEAR(col) <= 2007
  */
  if (!left_endp && temporal.days() == 1 && temporal.months() == 1 &&
      ! (temporal.hours() || temporal.minutes() || temporal.seconds() || temporal.useconds()))
    ; /* do nothing */
  else
    *incl_endp= true;
  return (int64_t) temporal.years();
}

