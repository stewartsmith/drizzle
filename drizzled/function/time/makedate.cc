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

#include <drizzled/function/time/makedate.h>
#include <drizzled/time_functions.h>

namespace drizzled
{

/**
  MAKEDATE(a,b) is a date function that creates a date value
  from a year and day value.

  NOTES:
    As arguments are integers, we can't know if the year is a 2 digit or 4 digit year.
    In this case we treat all years < 100 as 2 digit years. Ie, this is not safe
    for dates between 0000-01-01 and 0099-12-31
*/

String *Item_func_makedate::val_str(String *str)
{
  assert(fixed == 1);
  type::Time l_time;
  long daynr=  (long) args[1]->val_int();
  long year= (long) args[0]->val_int();
  long days;

  if (args[0]->null_value || args[1]->null_value ||
      year < 0 || daynr <= 0)
    goto err;

  if (year < 100)
    year= year_2000_handling(year);

  days= calc_daynr(year,1,1) + daynr - 1;
  /* Day number from year 0 to 9999-12-31 */
  if (days >= 0 && days <= MAX_DAY_NUMBER)
  {
    null_value=0;
    get_date_from_daynr(days,&l_time.year,&l_time.month,&l_time.day);
    str->alloc(type::Time::MAX_STRING_LENGTH);

    l_time.convert(*str, type::DRIZZLE_TIMESTAMP_DATE);

    return str;
  }

err:
  null_value=1;
  return 0;
}


/*
  MAKEDATE(a,b) is a date function that creates a date value
  from a year and day value.

  NOTES:
    As arguments are integers, we can't know if the year is a 2 digit or 4 digit year.
    In this case we treat all years < 100 as 2 digit years. Ie, this is not safe
    for dates between 0000-01-01 and 0099-12-31
*/

int64_t Item_func_makedate::val_int()
{
  assert(fixed == 1);
  type::Time l_time;
  long daynr=  (long) args[1]->val_int();
  long year= (long) args[0]->val_int();
  long days;

  if (args[0]->null_value || args[1]->null_value ||
      year < 0 || daynr <= 0)
    goto err;

  if (year < 100)
    year= year_2000_handling(year);

  days= calc_daynr(year,1,1) + daynr - 1;
  /* Day number from year 0 to 9999-12-31 */
  if (days >= 0 && days < MAX_DAY_NUMBER)
  {
    null_value=0;
    get_date_from_daynr(days,&l_time.year,&l_time.month,&l_time.day);
    return (int64_t) (l_time.year * 10000L + l_time.month * 100 + l_time.day);
  }

err:
  null_value= 1;
  return 0;
}

} /* namespace drizzled */
