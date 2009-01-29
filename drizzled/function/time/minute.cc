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
#include "drizzled/function/time/minute.h"

int64_t Item_func_minute::val_int()
{
  assert(fixed);

  if (args[0]->is_null())
  {
    /* For NULL argument, we return a NULL result */
    null_value= true;
    return 0;
  }

  /* 
   * Because of the ridiculous way in which MySQL handles
   * TIME values (it does implicit integer -> string conversions
   * but only for DATETIME, not TIME values) we must first 
   * try a conversion into a TIME from a string.  If this
   * fails, we fall back on a DATETIME conversion.  This is
   * necessary because of the fact that DateTime::from_string()
   * looks first for DATETIME, then DATE regex matches.  6 consecutive
   * numbers, say 231130, will match the DATE regex YYMMDD
   * with no TIME part, but MySQL actually implicitly treats
   * parameters to SECOND(), HOUR(), and MINUTE() as TIME-only
   * values and matches 231130 as HHmmSS!
   *
   * Oh, and Brian Aker MADE me do this. :) --JRP
   */
  drizzled::Time temporal_time;
  
  char time_buff[DRIZZLE_MAX_LENGTH_DATETIME_AS_STRING];
  String tmp_time(time_buff,sizeof(time_buff), &my_charset_utf8_bin);
  String *time_res= args[0]->val_str(&tmp_time);
  if (! temporal_time.from_string(time_res->c_ptr(), time_res->length()))
  {
    /* 
     * OK, we failed to match the first argument as a string
     * representing a time value, so we grab the first argument 
     * as a DateTime object and try that for a match...
     */
    drizzled::DateTime temporal_datetime;
    Item_result arg0_result_type= args[0]->result_type();
    
    switch (arg0_result_type)
    {
      case STRING_RESULT:
        {
          char buff[DRIZZLE_MAX_LENGTH_DATETIME_AS_STRING];
          String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
          String *res= args[0]->val_str(&tmp);
          if (! temporal_datetime.from_string(res->c_ptr(), res->length()))
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
        if (temporal_datetime.from_int64_t(args[0]->val_int()))
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

          my_error(ER_INVALID_DATETIME_VALUE, MYF(0), res->c_ptr());
          return 0;
        }
    }
    return (int64_t) temporal_datetime.minutes();
  }
  return (int64_t) temporal_time.minutes();
}

