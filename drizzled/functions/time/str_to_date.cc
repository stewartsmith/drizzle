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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/functions/time/str_to_date.h>

void Item_func_str_to_date::fix_length_and_dec()
{
  maybe_null= 1;
  decimals=0;
  cached_field_type= DRIZZLE_TYPE_DATETIME;
  max_length= MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  cached_timestamp_type= DRIZZLE_TIMESTAMP_NONE;
  if ((const_item= args[1]->const_item()))
  {
    char format_buff[64];
    String format_str(format_buff, sizeof(format_buff), &my_charset_bin);
    String *format= args[1]->val_str(&format_str);
    if (!args[1]->null_value)
    {
      cached_format_type= get_date_time_result_type(format->ptr(),
                                                    format->length());
      switch (cached_format_type) {
      case DATE_ONLY:
        cached_timestamp_type= DRIZZLE_TIMESTAMP_DATE;
        cached_field_type= DRIZZLE_TYPE_DATE; 
        max_length= MAX_DATE_WIDTH * MY_CHARSET_BIN_MB_MAXLEN;
        break;
      case TIME_ONLY:
      case TIME_MICROSECOND:
        cached_timestamp_type= DRIZZLE_TIMESTAMP_TIME;
        cached_field_type= DRIZZLE_TYPE_TIME; 
        max_length= MAX_TIME_WIDTH * MY_CHARSET_BIN_MB_MAXLEN;
        break;
      default:
        cached_timestamp_type= DRIZZLE_TIMESTAMP_DATETIME;
        cached_field_type= DRIZZLE_TYPE_DATETIME; 
        break;
      }
    }
  }
}


bool Item_func_str_to_date::get_date(DRIZZLE_TIME *ltime, uint32_t fuzzy_date)
{
  DATE_TIME_FORMAT date_time_format;
  char val_buff[64], format_buff[64];
  String val_string(val_buff, sizeof(val_buff), &my_charset_bin), *val;
  String format_str(format_buff, sizeof(format_buff), &my_charset_bin), *format;

  val=    args[0]->val_str(&val_string);
  format= args[1]->val_str(&format_str);
  if (args[0]->null_value || args[1]->null_value)
    goto null_date;

  null_value= 0;
  memset(ltime, 0, sizeof(*ltime));
  date_time_format.format.str=    (char*) format->ptr();
  date_time_format.format.length= format->length();
  if (extract_date_time(&date_time_format, val->ptr(), val->length(),
			ltime, cached_timestamp_type, 0, "datetime") ||
      ((fuzzy_date & TIME_NO_ZERO_DATE) &&
       (ltime->year == 0 || ltime->month == 0 || ltime->day == 0)))
    goto null_date;
  if (cached_timestamp_type == DRIZZLE_TIMESTAMP_TIME && ltime->day)
  {
    /*
      Day part for time type can be nonzero value and so 
      we should add hours from day part to hour part to
      keep valid time value.
    */
    ltime->hour+= ltime->day*24;
    ltime->day= 0;
  }
  return 0;

null_date:
  return (null_value=1);
}


String *Item_func_str_to_date::val_str(String *str)
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;

  if (Item_func_str_to_date::get_date(&ltime, TIME_FUZZY_DATE))
    return 0;

  if (!make_datetime((const_item ? cached_format_type :
		     (ltime.second_part ? DATE_TIME_MICROSECOND : DATE_TIME)),
		     &ltime, str))
    return str;
  return 0;
}
