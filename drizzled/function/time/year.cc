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

#include <drizzled/temporal.h>
#include <drizzled/error.h>
#include <drizzled/function/time/year.h>

namespace drizzled
{

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
  DateTime temporal;
  Item_result arg0_result_type= args[0]->result_type();
  
  switch (arg0_result_type)
  {
    case DECIMAL_RESULT: 
      /* 
       * For doubles supplied, interpret the arg as a string, 
       * so intentionally fall-through here...
       * This allows us to accept double parameters like 
       * 19971231235959.01 and interpret it the way MySQL does:
       * as a TIMESTAMP-like thing with a microsecond component.
       * Ugh, but need to keep backwards-compat.
       */
    case STRING_RESULT:
      {
        char buff[DRIZZLE_MAX_LENGTH_DATETIME_AS_STRING];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res= args[0]->val_str(&tmp);

        if (res && (res != &tmp))
        {
          tmp.copy(*res);
        }

        if (! temporal.from_string(tmp.c_ptr(), tmp.length()))
        {
          /* 
          * Could not interpret the function argument as a temporal value, 
          * so throw an error and return 0
          */
          my_error(ER_INVALID_DATETIME_VALUE, MYF(0), tmp.c_ptr());
          return 0;
        }
      }
      break;
    case INT_RESULT:
      if (temporal.from_int64_t(args[0]->val_int()))
        break;
      /* Intentionally fall-through on invalid conversion from integer */
    default:
      {
        /* 
        * Could not interpret the function argument as a temporal value, 
        * so throw an error and return 0
        */
        null_value= true;
        char buff[DRIZZLE_MAX_LENGTH_DATETIME_AS_STRING];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res;

        res= args[0]->val_str(&tmp);

        if (res && (res != &tmp))
        {
          tmp.copy(*res);
        }

        my_error(ER_INVALID_DATETIME_VALUE, MYF(0), tmp.c_ptr());
        return 0;
      }
  }
  return (int64_t) temporal.years();
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
  DateTime temporal;
  
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

} /* namespace drizzled */
