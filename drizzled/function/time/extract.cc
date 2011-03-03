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
#include <drizzled/session.h>
#include <drizzled/calendar.h>
#include <drizzled/function/time/extract.h>

namespace drizzled
{

/*
   'interval_names' reflects the order of the enumeration interval_type.
   See item/time.h
 */

extern const char *interval_names[];

void Item_extract::print(String *str)
{
  str->append(STRING_WITH_LEN("extract("));
  str->append(interval_names[int_type]);
  str->append(STRING_WITH_LEN(" from "));
  args[0]->print(str);
  str->append(')');
}

void Item_extract::fix_length_and_dec()
{
  value.alloc(DRIZZLE_MAX_LENGTH_DATETIME_AS_STRING);				

  maybe_null= 1;					/* If NULL supplied only */
  switch (int_type) {
  case INTERVAL_YEAR:		max_length=4; date_value=1; break;
  case INTERVAL_YEAR_MONTH:	max_length=6; date_value=1; break;
  case INTERVAL_QUARTER:        max_length=2; date_value=1; break;
  case INTERVAL_MONTH:		max_length=2; date_value=1; break;
  case INTERVAL_WEEK:		max_length=2; date_value=1; break;
  case INTERVAL_DAY:		max_length=2; date_value=1; break;
  case INTERVAL_DAY_HOUR:	max_length=9; date_value=0; break;
  case INTERVAL_DAY_MINUTE:	max_length=11; date_value=0; break;
  case INTERVAL_DAY_SECOND:	max_length=13; date_value=0; break;
  case INTERVAL_HOUR:		max_length=2; date_value=0; break;
  case INTERVAL_HOUR_MINUTE:	max_length=4; date_value=0; break;
  case INTERVAL_HOUR_SECOND:	max_length=6; date_value=0; break;
  case INTERVAL_MINUTE:		max_length=2; date_value=0; break;
  case INTERVAL_MINUTE_SECOND:	max_length=4; date_value=0; break;
  case INTERVAL_SECOND:		max_length=2; date_value=0; break;
  case INTERVAL_MICROSECOND:	max_length=2; date_value=0; break;
  case INTERVAL_DAY_MICROSECOND: max_length=20; date_value=0; break;
  case INTERVAL_HOUR_MICROSECOND: max_length=13; date_value=0; break;
  case INTERVAL_MINUTE_MICROSECOND: max_length=11; date_value=0; break;
  case INTERVAL_SECOND_MICROSECOND: max_length=9; date_value=0; break;
  case INTERVAL_LAST: assert(0); break;
  }
}

int64_t Item_extract::val_int()
{
  assert(fixed);

  if (args[0]->is_null())
  {
    /* For NULL argument, we return a NULL result */
    null_value= true;
    return 0;
  }

  /* We could have either a datetime or a time.. */
  DateTime datetime_temporal;
  Time time_temporal;

  /* Abstract pointer type we'll use in the final switch */
  Temporal *temporal;

  if (date_value)
  {
    /* Grab the first argument as a DateTime object */
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

          if (! datetime_temporal.from_string(tmp.c_ptr(), tmp.length()))
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
        if (datetime_temporal.from_int64_t(args[0]->val_int()))
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
    /* 
     * If we got here, we have a successfully converted DateTime temporal. 
     * Point our working temporal to this.
     */
    temporal= &datetime_temporal;
  }
  else
  {
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
    
    char time_buff[DRIZZLE_MAX_LENGTH_DATETIME_AS_STRING];
    String tmp_time(time_buff,sizeof(time_buff), &my_charset_utf8_bin);
    String *time_res= args[0]->val_str(&tmp_time);

    if (time_res && (time_res != &tmp_time))
    {
      tmp_time.copy(*time_res);
    }

    if (! time_temporal.from_string(tmp_time.c_ptr(), tmp_time.length()))
    {
      /* 
       * OK, we failed to match the first argument as a string
       * representing a time value, so we grab the first argument 
       * as a DateTime object and try that for a match...
       */
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

            if (! datetime_temporal.from_string(tmp.c_ptr(), tmp.length()))
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
          if (datetime_temporal.from_int64_t(args[0]->val_int()))
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
      /* If we're here, our time failed, but our datetime succeeded... */
      temporal= &datetime_temporal;
    }
    else
    {
      /* If we're here, our time succeeded... */
      temporal= &time_temporal;
    }
  }

  /* Return the requested datetime component */
  switch (int_type) {
    case INTERVAL_YEAR:		
      return (int64_t) temporal->years();
    case INTERVAL_YEAR_MONTH:	
      return (int64_t) ((temporal->years() * 100L) + temporal->months());
    case INTERVAL_QUARTER:
      return (int64_t) (temporal->months() + 2) / 3;
    case INTERVAL_MONTH:
      return (int64_t) temporal->months();
    case INTERVAL_WEEK:
      return iso_week_number_from_gregorian_date(temporal->years()
                                               , temporal->months()
                                               , temporal->days());
    case INTERVAL_DAY:
      return (int64_t) temporal->days();
    case INTERVAL_DAY_HOUR:	
      return (int64_t) ((temporal->days() * 100L) + temporal->hours());
    case INTERVAL_DAY_MINUTE:	
      return (int64_t) ((temporal->days() * 10000L)
            + (temporal->hours() * 100L) 
            + temporal->minutes());
    case INTERVAL_DAY_SECOND:	 
      return (int64_t) (
              (int64_t) (temporal->days() * 1000000L) 
            + (int64_t) (temporal->hours() * 10000L)
            + (temporal->minutes() * 100L) 
            + temporal->seconds());
    case INTERVAL_HOUR:		
      return (int64_t) temporal->hours();
    case INTERVAL_HOUR_MINUTE:	
      return (int64_t) (temporal->hours() * 100L) 
            + temporal->minutes();
    case INTERVAL_HOUR_SECOND:	
      return (int64_t) (temporal->hours() * 10000L) 
            + (temporal->minutes() * 100L) 
            + temporal->seconds();
    case INTERVAL_MINUTE:
      return (int64_t) temporal->minutes();
    case INTERVAL_MINUTE_SECOND:	
      return (int64_t) (temporal->minutes() * 100L) + temporal->seconds();
    case INTERVAL_SECOND:
      return (int64_t) temporal->seconds();
    case INTERVAL_MICROSECOND:	
      return (int64_t) temporal->useconds();
    case INTERVAL_DAY_MICROSECOND: 
      return (int64_t) 
              (
              (
              (int64_t) (temporal->days() * 1000000L) 
            + (int64_t) (temporal->hours() * 10000L) 
            + (temporal->minutes() * 100L) 
            + temporal->seconds()
              ) 
              * 1000000L
              ) 
            + temporal->useconds();
    case INTERVAL_HOUR_MICROSECOND:
        return (int64_t)
              (
              (
              (int64_t) (temporal->hours() * 10000L) 
            + (temporal->minutes() * 100L) 
            + temporal->seconds()
              ) 
              * 1000000L
              ) 
            + temporal->useconds();
    case INTERVAL_MINUTE_MICROSECOND:
        return (int64_t)
              (
              (
              (temporal->minutes() * 100L) 
            + temporal->seconds()
              ) 
              * 1000000L
              ) 
            + temporal->useconds();
    case INTERVAL_SECOND_MICROSECOND: 
        return (int64_t) (temporal->seconds() * 1000000L)
            + temporal->useconds();
    case INTERVAL_LAST: 
    default:
        assert(0); 
        return 0;
  }
}

bool Item_extract::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      functype() != ((Item_func*)item)->functype())
    return 0;

  Item_extract* ie= (Item_extract*)item;
  if (ie->int_type != int_type)
    return 0;

  if (!args[0]->eq(ie->args[0], binary_cmp))
      return 0;
  return 1;
}

} /* namespace drizzled */
