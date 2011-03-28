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

#include <drizzled/internal/m_string.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/current_session.h>
#include <drizzled/function/time/date.h>
#include <drizzled/temporal_interval.h>
#include <drizzled/time_functions.h>

namespace drizzled
{

bool TemporalInterval::initFromItem(Item *args,
                                    interval_type int_type,
                                    String *str_value)
{
  uint64_t array[MAX_STRING_ELEMENTS];
  int64_t value= 0;
  const char *str= NULL;
  size_t length= 0;
  const charset_info_st * const cs= str_value->charset();


  // Types <= microsecond can be converted as an integer
  if (static_cast<int>(int_type) <= INTERVAL_MICROSECOND)
  {
    value= args->val_int();
    if (args->null_value)
      return true;
    if (value < 0)
    {
      neg= true;
      value= -value;
    }
  }
  else
  {
    // Otherwise we must convert to a string and extract the multiple parts
    String *res;
    if (!(res= args->val_str(str_value)))
      return true;

    // record negative intervalls in interval->neg 
    str= res->ptr();
    const char *end= str+res->length();
    // Skip the whitespace
    while (str != end && my_isspace(cs,*str))
      str++;
    if (str != end && *str == '-')
    {
      neg= true;
      // skip the -
      str++;
    }
    length= static_cast<size_t>(end-str);		// Set up pointers to new str
  }

  switch (int_type)
  {
  case INTERVAL_YEAR:
    year= static_cast<uint32_t>(value);
    break;
  case INTERVAL_QUARTER:
    month= static_cast<uint32_t>(value*3);
    break;
  case INTERVAL_MONTH:
    month= static_cast<uint32_t>(value);
    break;
  case INTERVAL_WEEK:
    day= static_cast<uint32_t>(value*7);
    break;
  case INTERVAL_DAY:
    day= static_cast<uint32_t>(value);
    break;
  case INTERVAL_HOUR:
    hour= static_cast<uint32_t>(value);
    break;
  case INTERVAL_MICROSECOND:
    second_part= value;
    break;
  case INTERVAL_MINUTE:
    minute= value;
    break;
  case INTERVAL_SECOND:
    second= value;
    break;
  case INTERVAL_YEAR_MONTH:			// Allow YEAR-MONTH YYYYYMM
    if (getIntervalFromString(str,length,cs,NUM_YEAR_MONTH_STRING_ELEMENTS,array,false))
      return true;
    year=  static_cast<uint32_t>(array[0]);
    month= static_cast<uint32_t>(array[1]);
    break;
  case INTERVAL_DAY_HOUR:
    if (getIntervalFromString(str,length,cs,NUM_DAY_HOUR_STRING_ELEMENTS,array,false))
      return true;
    day=  static_cast<uint32_t>(array[0]);
    hour= static_cast<uint32_t>(array[1]);
    break;
  case INTERVAL_DAY_MICROSECOND:
    if (getIntervalFromString(str,length,cs,NUM_DAY_MICROSECOND_STRING_ELEMENTS,array,true))
      return true;
    day=    static_cast<uint32_t>(array[0]);
    hour=   static_cast<uint32_t>(array[1]);
    minute= array[2];
    second= array[3];
    second_part= array[4];
    break;
  case INTERVAL_DAY_MINUTE:
    if (getIntervalFromString(str,length,cs,NUM_DAY_MINUTE_STRING_ELEMENTS,array,false))
      return true;
    day=    static_cast<uint32_t>(array[0]);
    hour=   static_cast<uint32_t>(array[1]);
    minute= array[2];
    break;
  case INTERVAL_DAY_SECOND:
    if (getIntervalFromString(str,length,cs,NUM_DAY_SECOND_STRING_ELEMENTS,array,false))
      return true;
    day=    static_cast<uint32_t>(array[0]);
    hour=   static_cast<uint32_t>(array[1]);
    minute= array[2];
    second= array[3];
    break;
  case INTERVAL_HOUR_MICROSECOND:
    if (getIntervalFromString(str,length,cs,NUM_HOUR_MICROSECOND_STRING_ELEMENTS,array,true))
      return true;
    hour=   static_cast<uint32_t>(array[0]);
    minute= array[1];
    second= array[2];
    second_part= array[3];
    break;
  case INTERVAL_HOUR_MINUTE:
    if (getIntervalFromString(str,length,cs,NUM_HOUR_MINUTE_STRING_ELEMENTS,array,false))
      return true;
    hour=   static_cast<uint32_t>(array[0]);
    minute= array[1];
    break;
  case INTERVAL_HOUR_SECOND:
    if (getIntervalFromString(str,length,cs,NUM_HOUR_SECOND_STRING_ELEMENTS,array,false))
      return true;
    hour=   static_cast<uint32_t>(array[0]);
    minute= array[1];
    second= array[2];
    break;
  case INTERVAL_MINUTE_MICROSECOND:
    if (getIntervalFromString(str,length,cs,NUM_MINUTE_MICROSECOND_STRING_ELEMENTS,array,true))
      return true;
    minute= array[0];
    second= array[1];
    second_part= array[2];
    break;
  case INTERVAL_MINUTE_SECOND:
    if (getIntervalFromString(str,length,cs,NUM_MINUTE_SECOND_STRING_ELEMENTS,array,false))
      return true;
    minute= array[0];
    second= array[1];
    break;
  case INTERVAL_SECOND_MICROSECOND:
    if (getIntervalFromString(str,length,cs,NUM_SECOND_MICROSECOND_STRING_ELEMENTS,array,true))
      return true;
    second= array[0];
    second_part= array[1];
    break;
  case INTERVAL_LAST:
    assert(0);
    break;
  }
  return false;
}

bool TemporalInterval::addDate(type::Time *ltime, interval_type int_type)
{
  long period, sign;

  ltime->neg= 0;

  sign= (neg ? -1 : 1);

  switch (int_type)
  {
  case INTERVAL_SECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
  case INTERVAL_MINUTE:
  case INTERVAL_HOUR:
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_MINUTE_SECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_HOUR_SECOND:
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_DAY_SECOND:
  case INTERVAL_DAY_MINUTE:
  case INTERVAL_DAY_HOUR:
    int64_t sec, days, daynr, microseconds, extra_sec;
    ltime->time_type= type::DRIZZLE_TIMESTAMP_DATETIME; // Return full date
    microseconds= ltime->second_part + sign*second_part;
    extra_sec= microseconds/1000000L;
    microseconds= microseconds%1000000L;

    sec= ((ltime->day-1)*3600*24L+ltime->hour*3600+ltime->minute*60+
        ltime->second +
        sign* (int64_t) (day*3600*24L +
          hour*3600L+minute*60L+
          second))+ extra_sec;
    if (microseconds < 0)
    {
      microseconds+= 1000000L;
      sec--;
    }
    days= sec/(3600*24L);
    sec-= days*3600*24L;
    if (sec < 0)
    {
      days--;
      sec+= 3600*24L;
    }
    ltime->second_part= (uint32_t) microseconds;
    ltime->second= (uint32_t) (sec % 60);
    ltime->minute= (uint32_t) (sec/60 % 60);
    ltime->hour=   (uint32_t) (sec/3600);
    daynr= calc_daynr(ltime->year,ltime->month,1) + days;
    /* Day number from year 0 to 9999-12-31 */
    if ((uint64_t) daynr > MAX_DAY_NUMBER)
      goto invalid_date;
    get_date_from_daynr((long) daynr, &ltime->year, &ltime->month, &ltime->day);
    break;
  case INTERVAL_DAY:
  case INTERVAL_WEEK:
    period= (calc_daynr(ltime->year,ltime->month,ltime->day) +
        sign * (long) day);
    /* Daynumber from year 0 to 9999-12-31 */
    if (period > MAX_DAY_NUMBER)
      goto invalid_date;
    get_date_from_daynr((long) period,&ltime->year,&ltime->month,&ltime->day);
    break;
  case INTERVAL_YEAR:
    ltime->year+= sign * (long) year;
    if (ltime->year >= 10000L)
      goto invalid_date;
    if (ltime->month == 2 && ltime->day == 29 &&
        calc_days_in_year(ltime->year) != 366)
      ltime->day= 28;				// Was leap-year
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_QUARTER:
  case INTERVAL_MONTH:
    period= (ltime->year*12 + sign * (long) year*12 +
        ltime->month-1 + sign * (long) month);
    if (period >= 120000L)
      goto invalid_date;
    ltime->year= (uint32_t) (period / 12);
    ltime->month= (uint32_t) (period % 12L)+1;
    /* Adjust day if the new month doesn't have enough days */
    if (ltime->day > days_in_month[ltime->month-1])
    {
      ltime->day= days_in_month[ltime->month-1];
      if (ltime->month == 2 && calc_days_in_year(ltime->year) == 366)
        ltime->day++;				// Leap-year
    }
    break;
  default:
    goto null_date;
  }

  return 0;					// Ok

invalid_date:
  push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                      ER_DATETIME_FUNCTION_OVERFLOW,
                      ER(ER_DATETIME_FUNCTION_OVERFLOW),
                      "datetime");
null_date:
  return 1;
}

bool TemporalInterval::getIntervalFromString(const char *str,
                                             uint32_t length,
                                             const charset_info_st * const cs,
                                             uint32_t count, uint64_t *values,
                                             bool transform_msec)
{
  const char *end= str+length;
  uint32_t x;

  while (str != end && !my_isdigit(cs,*str))
    str++;

  for (x= 0 ; x < count ; x++)
  {
    int64_t value;
    const char *start= str;
    for (value= 0 ; str != end && my_isdigit(cs,*str) ; str++)
      value= value * 10L + (int64_t) (*str - '0');
    if (transform_msec && (x == count - 1 || str == end)) // microseconds always last
    {
      long msec_length= 6 - (str - start);
      if (msec_length > 0)
        value*= (long) log_10_int[msec_length];
    }
    values[x]= value;
    while (str != end && !my_isdigit(cs,*str))
      str++;
    if (str == end && x != count-1)
    {
      x++;
      /* Change values[0...x-1] -> values[count-x...count-1] */
      internal::bmove_upp((unsigned char*) (values+count),
                          (unsigned char*) (values+x),
                          sizeof(*values)*x);
      memset(values, 0, sizeof(*values)*(count-x));
      break;
    }
  }
  return (str != end);
}

} /* namespace drizzled */
