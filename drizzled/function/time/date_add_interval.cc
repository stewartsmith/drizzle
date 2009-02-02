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
#include <drizzled/function/time/date_add_interval.h>
#include <drizzled/function/time/get_interval_value.h>
#include <drizzled/function/time/make_datetime.h>

/*
   'interval_names' reflects the order of the enumeration interval_type.
   See item/time.h
 */
const char *interval_names[]=
{
  "year", "quarter", "month", "week", "day",
  "hour", "minute", "second", "microsecond",
  "year_month", "day_hour", "day_minute",
  "day_second", "hour_minute", "hour_second",
  "minute_second", "day_microsecond",
  "hour_microsecond", "minute_microsecond",
  "second_microsecond"
};


void Item_date_add_interval::fix_length_and_dec()
{
  enum_field_types arg0_field_type;

  collation.set(&my_charset_bin);
  maybe_null=1;
  max_length=MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  value.alloc(max_length);

  /*
    The field type for the result of an Item_date function is defined as
    follows:

    - If first arg is a DRIZZLE_TYPE_DATETIME result is DRIZZLE_TYPE_DATETIME
    - If first arg is a DRIZZLE_TYPE_DATE and the interval type uses hours,
      minutes or seconds then type is DRIZZLE_TYPE_DATETIME.
    - Otherwise the result is DRIZZLE_TYPE_VARCHAR
      (This is because you can't know if the string contains a DATE, DRIZZLE_TIME or
      DATETIME argument)
  */
  cached_field_type= DRIZZLE_TYPE_VARCHAR;
  arg0_field_type= args[0]->field_type();
  if (arg0_field_type == DRIZZLE_TYPE_DATETIME ||
      arg0_field_type == DRIZZLE_TYPE_TIMESTAMP)
    cached_field_type= DRIZZLE_TYPE_DATETIME;
  else if (arg0_field_type == DRIZZLE_TYPE_DATE)
  {
    if (int_type <= INTERVAL_DAY || int_type == INTERVAL_YEAR_MONTH)
      cached_field_type= arg0_field_type;
    else
      cached_field_type= DRIZZLE_TYPE_DATETIME;
  }
}


/* Here arg[1] is a Item_interval object */

bool Item_date_add_interval::get_date(DRIZZLE_TIME *ltime, uint32_t )
{
  INTERVAL interval;

  if (args[0]->get_date(ltime, TIME_NO_ZERO_DATE) ||
      get_interval_value(args[1], int_type, &value, &interval))
    return (null_value=1);

  if (date_sub_interval)
    interval.neg = !interval.neg;

  if ((null_value= date_add_interval(ltime, int_type, interval)))
    return 1;
  return 0;
}


String *Item_date_add_interval::val_str(String *str)
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  enum date_time_format_types format;

  if (Item_date_add_interval::get_date(&ltime, TIME_NO_ZERO_DATE))
    return 0;

  if (ltime.time_type == DRIZZLE_TIMESTAMP_DATE)
    format= DATE_ONLY;
  else if (ltime.second_part)
    format= DATE_TIME_MICROSECOND;
  else
    format= DATE_TIME;

  if (!make_datetime(format, &ltime, str))
    return str;

  null_value=1;
  return 0;
}


int64_t Item_date_add_interval::val_int()
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  int64_t date;
  if (Item_date_add_interval::get_date(&ltime, TIME_NO_ZERO_DATE))
    return (int64_t) 0;
  date = (ltime.year*100L + ltime.month)*100L + ltime.day;
  return ltime.time_type == DRIZZLE_TIMESTAMP_DATE ? date :
    ((date*100L + ltime.hour)*100L+ ltime.minute)*100L + ltime.second;
}



bool Item_date_add_interval::eq(const Item *item, bool binary_cmp) const
{
  Item_date_add_interval *other= (Item_date_add_interval*) item;
  if (!Item_func::eq(item, binary_cmp))
    return 0;
  return ((int_type == other->int_type) &&
          (date_sub_interval == other->date_sub_interval));
}

void Item_date_add_interval::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  str->append(date_sub_interval?" - interval ":" + interval ");
  args[1]->print(str, query_type);
  str->append(' ');
  str->append(interval_names[int_type]);
  str->append(')');
}
