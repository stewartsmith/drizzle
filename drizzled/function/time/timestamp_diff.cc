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

#include <drizzled/function/time/timestamp_diff.h>
#include <drizzled/time_functions.h>

namespace drizzled
{

int64_t Item_func_timestamp_diff::val_int()
{
  type::Time ltime1, ltime2;
  int64_t seconds;
  long microseconds;
  long months= 0;
  int neg= 1;

  null_value= 0;
  if (args[0]->get_date(ltime1, TIME_NO_ZERO_DATE) ||
      args[1]->get_date(ltime2, TIME_NO_ZERO_DATE))
    goto null_date;

  if (calc_time_diff(&ltime2,&ltime1, 1,
		     &seconds, &microseconds))
    neg= -1;

  if (int_type == INTERVAL_YEAR ||
      int_type == INTERVAL_QUARTER ||
      int_type == INTERVAL_MONTH)
  {
    uint32_t year_beg, year_end, month_beg, month_end, day_beg, day_end;
    uint32_t years= 0;
    uint32_t second_beg, second_end, microsecond_beg, microsecond_end;

    if (neg == -1)
    {
      year_beg= ltime2.year;
      year_end= ltime1.year;
      month_beg= ltime2.month;
      month_end= ltime1.month;
      day_beg= ltime2.day;
      day_end= ltime1.day;
      second_beg= ltime2.hour * 3600 + ltime2.minute * 60 + ltime2.second;
      second_end= ltime1.hour * 3600 + ltime1.minute * 60 + ltime1.second;
      microsecond_beg= ltime2.second_part;
      microsecond_end= ltime1.second_part;
    }
    else
    {
      year_beg= ltime1.year;
      year_end= ltime2.year;
      month_beg= ltime1.month;
      month_end= ltime2.month;
      day_beg= ltime1.day;
      day_end= ltime2.day;
      second_beg= ltime1.hour * 3600 + ltime1.minute * 60 + ltime1.second;
      second_end= ltime2.hour * 3600 + ltime2.minute * 60 + ltime2.second;
      microsecond_beg= ltime1.second_part;
      microsecond_end= ltime2.second_part;
    }

    /* calc years */
    years= year_end - year_beg;
    if (month_end < month_beg || (month_end == month_beg && day_end < day_beg))
      years-= 1;

    /* calc months */
    months= 12*years;
    if (month_end < month_beg || (month_end == month_beg && day_end < day_beg))
      months+= 12 - (month_beg - month_end);
    else
      months+= (month_end - month_beg);

    if (day_end < day_beg)
      months-= 1;
    else if ((day_end == day_beg) &&
	     ((second_end < second_beg) ||
	      (second_end == second_beg && microsecond_end < microsecond_beg)))
      months-= 1;
  }

  switch (int_type) {
  case INTERVAL_YEAR:
    return months/12*neg;
  case INTERVAL_QUARTER:
    return months/3*neg;
  case INTERVAL_MONTH:
    return months*neg;
  case INTERVAL_WEEK:
    return seconds/86400L/7L*neg;
  case INTERVAL_DAY:
    return seconds/86400L*neg;
  case INTERVAL_HOUR:
    return seconds/3600L*neg;
  case INTERVAL_MINUTE:
    return seconds/60L*neg;
  case INTERVAL_SECOND:
    return seconds*neg;
  case INTERVAL_MICROSECOND:
    /*
      In MySQL difference between any two valid datetime values
      in microseconds fits into int64_t.
    */
    return (seconds*1000000L+microseconds)*neg;
  default:
    break;
  }

null_date:
  null_value=1;
  return 0;
}


void Item_func_timestamp_diff::print(String *str)
{
  str->append(func_name());
  str->append('(');

  switch (int_type) {
  case INTERVAL_YEAR:
    str->append(STRING_WITH_LEN("YEAR"));
    break;
  case INTERVAL_QUARTER:
    str->append(STRING_WITH_LEN("QUARTER"));
    break;
  case INTERVAL_MONTH:
    str->append(STRING_WITH_LEN("MONTH"));
    break;
  case INTERVAL_WEEK:
    str->append(STRING_WITH_LEN("WEEK"));
    break;
  case INTERVAL_DAY:
    str->append(STRING_WITH_LEN("DAY"));
    break;
  case INTERVAL_HOUR:
    str->append(STRING_WITH_LEN("HOUR"));
    break;
  case INTERVAL_MINUTE:
    str->append(STRING_WITH_LEN("MINUTE"));
    break;
  case INTERVAL_SECOND:
    str->append(STRING_WITH_LEN("SECOND"));
    break;
  case INTERVAL_MICROSECOND:
    str->append(STRING_WITH_LEN("SECOND_FRAC"));
    break;
  default:
    break;
  }

  for (uint32_t i=0 ; i < 2 ; i++)
  {
    str->append(',');
    args[i]->print(str);
  }
  str->append(')');
}

} /* namespace drizzled */
