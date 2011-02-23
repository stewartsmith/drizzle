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

#include <drizzled/function/time/last_day.h>
#include <drizzled/error.h>
#include <drizzled/calendar.h>
#include <drizzled/temporal.h>

#include <sstream>
#include <string>

namespace drizzled
{

/**
 * Interpret the first argument as a DateTime string and then populate
 * our supplied temporal object with a Date representing the last day of 
 * the corresponding month and year.
 */
bool Item_func_last_day::get_temporal(Date &to)
{
  assert(fixed);

  /* We return NULL from LAST_DAY() only when supplied a NULL argument */
  if (args[0]->null_value)
  {
    null_value= true;
    return false;
  }

  /* We use a DateTime to match as many temporal formats as possible. */
  DateTime temporal;
  Item_result arg0_result_type= args[0]->result_type();
  
  switch (arg0_result_type)
  {
    case REAL_RESULT:
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

        if (! res)
        {
          /* 
           * Likely a nested function issue where the nested
           * function had bad input.  We rely on the nested
           * function my_error() and simply return false here.
           */
          return false;
        }

        if (res != &tmp)
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
          return false;
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

        if (! res)
        {
          /* 
           * Likely a nested function issue where the nested
           * function had bad input.  We rely on the nested
           * function my_error() and simply return false here.
           */
          return false;
        }

        if (res != &tmp)
        {
          tmp.copy(*res);
        }

        my_error(ER_INVALID_DATETIME_VALUE, MYF(0), tmp.c_ptr());
        return false;
      }
  }
  null_value= false;

  /* Now strip to the last day of the month... */
  temporal.set_days(days_in_gregorian_year_month(temporal.years(), temporal.months()));
  to= temporal; /* Operator overload in effect for assign DateTime to Date. */

  return true;
}

} /* namespace drizzled */
