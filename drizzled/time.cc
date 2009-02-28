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


/* Functions to handle date and time */

#include <drizzled/server_includes.h>
#include <drizzled/error.h>
#include <drizzled/util/test.h>
#include <drizzled/tztime.h>
#include <drizzled/session.h>

/* Some functions to calculate dates */

#ifndef TESTTIME

/*
  Name description of interval names used in statements.

  'interval_type_to_name' is ordered and sorted on interval size and
  interval complexity.
  Order of elements in 'interval_type_to_name' should correspond to
  the order of elements in 'interval_type' enum

  See also interval_type, interval_names
*/

LEX_STRING interval_type_to_name[INTERVAL_LAST] = {
  { C_STRING_WITH_LEN("YEAR")},
  { C_STRING_WITH_LEN("QUARTER")},
  { C_STRING_WITH_LEN("MONTH")},
  { C_STRING_WITH_LEN("WEEK")},
  { C_STRING_WITH_LEN("DAY")},
  { C_STRING_WITH_LEN("HOUR")},
  { C_STRING_WITH_LEN("MINUTE")},
  { C_STRING_WITH_LEN("SECOND")},
  { C_STRING_WITH_LEN("MICROSECOND")},
  { C_STRING_WITH_LEN("YEAR_MONTH")},
  { C_STRING_WITH_LEN("DAY_HOUR")},
  { C_STRING_WITH_LEN("DAY_MINUTE")},
  { C_STRING_WITH_LEN("DAY_SECOND")},
  { C_STRING_WITH_LEN("HOUR_MINUTE")},
  { C_STRING_WITH_LEN("HOUR_SECOND")},
  { C_STRING_WITH_LEN("MINUTE_SECOND")},
  { C_STRING_WITH_LEN("DAY_MICROSECOND")},
  { C_STRING_WITH_LEN("HOUR_MICROSECOND")},
  { C_STRING_WITH_LEN("MINUTE_MICROSECOND")},
  { C_STRING_WITH_LEN("SECOND_MICROSECOND")}
};

	/* Calc weekday from daynr */
	/* Returns 0 for monday, 1 for tuesday .... */

int calc_weekday(long daynr,bool sunday_first_day_of_week)
{
  return ((int) ((daynr + 5L + (sunday_first_day_of_week ? 1L : 0L)) % 7));
}

/*
  The bits in week_format has the following meaning:
   WEEK_MONDAY_FIRST (0)  If not set	Sunday is first day of week
      		   	  If set	Monday is first day of week
   WEEK_YEAR (1)	  If not set	Week is in range 0-53

   	Week 0 is returned for the the last week of the previous year (for
	a date at start of january) In this case one can get 53 for the
	first week of next year.  This flag ensures that the week is
	relevant for the given year. Note that this flag is only
	releveant if WEEK_JANUARY is not set.

			  If set	 Week is in range 1-53.

	In this case one may get week 53 for a date in January (when
	the week is that last week of previous year) and week 1 for a
	date in December.

  WEEK_FIRST_WEEKDAY (2)  If not set	Weeks are numbered according
			   		to ISO 8601:1988
			  If set	The week that contains the first
					'first-day-of-week' is week 1.

	ISO 8601:1988 means that if the week containing January 1 has
	four or more days in the new year, then it is week 1;
	Otherwise it is the last week of the previous year, and the
	next week is week 1.
*/

uint32_t calc_week(DRIZZLE_TIME *l_time, uint32_t week_behaviour, uint32_t *year)
{
  uint32_t days;
  uint32_t daynr= calc_daynr(l_time->year,l_time->month,l_time->day);
  uint32_t first_daynr= calc_daynr(l_time->year,1,1);
  bool monday_first= test(week_behaviour & WEEK_MONDAY_FIRST);
  bool week_year= test(week_behaviour & WEEK_YEAR);
  bool first_weekday= test(week_behaviour & WEEK_FIRST_WEEKDAY);

  uint32_t weekday=calc_weekday(first_daynr, !monday_first);
  *year=l_time->year;

  if (l_time->month == 1 && l_time->day <= 7-weekday)
  {
    if ((!week_year) && ((first_weekday && weekday != 0) || (!first_weekday && weekday >= 4)))
      return 0;
    week_year= 1;
    (*year)--;
    first_daynr-= (days=calc_days_in_year(*year));
    weekday= (weekday + 53*7- days) % 7;
  }

  if ((first_weekday && weekday != 0) ||
      (!first_weekday && weekday >= 4))
    days= daynr - (first_daynr+ (7-weekday));
  else
    days= daynr - (first_daynr - weekday);

  if (week_year && days >= 52*7)
  {
    weekday= (weekday + calc_days_in_year(*year)) % 7;
    if ((!first_weekday && weekday < 4) || (first_weekday && weekday == 0))
    {
      (*year)++;
      return 1;
    }
  }
  return days/7+1;
}

	/* Change a daynr to year, month and day */
	/* Daynr 0 is returned as date 00.00.00 */

void get_date_from_daynr(long daynr,uint32_t *ret_year,uint32_t *ret_month,
			 uint32_t *ret_day)
{
  uint32_t year,temp,leap_day,day_of_year,days_in_year;
  unsigned char *month_pos;

  if (daynr <= 365L || daynr >= 3652500)
  {						/* Fix if wrong daynr */
    *ret_year= *ret_month = *ret_day =0;
  }
  else
  {
    year= (uint32_t) (daynr*100 / 36525L);
    temp=(((year-1)/100+1)*3)/4;
    day_of_year=(uint32_t) (daynr - (long) year * 365L) - (year-1)/4 +temp;
    while (day_of_year > (days_in_year= calc_days_in_year(year)))
    {
      day_of_year-=days_in_year;
      (year)++;
    }
    leap_day=0;
    if (days_in_year == 366)
    {
      if (day_of_year > 31+28)
      {
	day_of_year--;
	if (day_of_year == 31+28)
	  leap_day=1;		/* Handle leapyears leapday */
      }
    }
    *ret_month=1;
    for (month_pos= days_in_month ;
	 day_of_year > (uint32_t) *month_pos ;
	 day_of_year-= *(month_pos++), (*ret_month)++)
      ;
    *ret_year=year;
    *ret_day=day_of_year+leap_day;
  }
  return;
}

/*
  Convert a timestamp string to a DRIZZLE_TIME value and produce a warning
  if string was truncated during conversion.

  NOTE
    See description of str_to_datetime() for more information.
*/

enum enum_drizzle_timestamp_type
str_to_datetime_with_warn(const char *str, uint32_t length, DRIZZLE_TIME *l_time,
                          uint32_t flags)
{
  int was_cut;
  Session *session= current_session;
  enum enum_drizzle_timestamp_type ts_type;

  ts_type= str_to_datetime(str, length, l_time,
                           (flags | (session->variables.sql_mode &
                                     (MODE_INVALID_DATES |
                                      MODE_NO_ZERO_DATE))),
                           &was_cut);
  if (was_cut || ts_type <= DRIZZLE_TIMESTAMP_ERROR)
    make_truncated_value_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 str, length, ts_type,  NULL);
  return ts_type;
}


/*
  Convert a datetime from broken-down DRIZZLE_TIME representation to corresponding
  TIMESTAMP value.

  SYNOPSIS
    TIME_to_timestamp()
      session             - current thread
      t               - datetime in broken-down representation,
      in_dst_time_gap - pointer to bool which is set to true if t represents
                        value which doesn't exists (falls into the spring
                        time-gap) or to false otherwise.

  RETURN
     Number seconds in UTC since start of Unix Epoch corresponding to t.
     0 - t contains datetime value which is out of TIMESTAMP range.

*/
time_t TIME_to_timestamp(Session *session, const DRIZZLE_TIME *t,
                            bool *in_dst_time_gap)
{
  time_t timestamp;

  *in_dst_time_gap= 0;

  timestamp= session->variables.time_zone->TIME_to_gmt_sec(t, in_dst_time_gap);
  if (timestamp)
  {
    return timestamp;
  }

  /* If we are here we have range error. */
  return(0);
}


/*
  Convert a time string to a DRIZZLE_TIME struct and produce a warning
  if string was cut during conversion.

  NOTE
    See str_to_time() for more info.
*/
bool
str_to_time_with_warn(const char *str, uint32_t length, DRIZZLE_TIME *l_time)
{
  int warning;
  bool ret_val= str_to_time(str, length, l_time, &warning);
  if (ret_val || warning)
    make_truncated_value_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 str, length, DRIZZLE_TIMESTAMP_TIME, NULL);
  return ret_val;
}


/*
  Convert a system time structure to TIME
*/

void localtime_to_TIME(DRIZZLE_TIME *to, struct tm *from)
{
  to->neg=0;
  to->second_part=0;
  to->year=	(int) ((from->tm_year+1900) % 10000);
  to->month=	(int) from->tm_mon+1;
  to->day=	(int) from->tm_mday;
  to->hour=	(int) from->tm_hour;
  to->minute=	(int) from->tm_min;
  to->second=   (int) from->tm_sec;
}

void calc_time_from_sec(DRIZZLE_TIME *to, long seconds, long microseconds)
{
  long t_seconds;
  // to->neg is not cleared, it may already be set to a useful value
  to->time_type= DRIZZLE_TIMESTAMP_TIME;
  to->year= 0;
  to->month= 0;
  to->day= 0;
  to->hour= seconds/3600L;
  t_seconds= seconds%3600L;
  to->minute= t_seconds/60L;
  to->second= t_seconds%60L;
  to->second_part= microseconds;
}

void make_time(const DRIZZLE_TIME *l_time, String *str)
{
  uint32_t length= (uint32_t) my_time_to_str(l_time, (char*) str->c_ptr());
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_date(const DRIZZLE_TIME *l_time, String *str)
{
  uint32_t length= (uint32_t) my_date_to_str(l_time, (char*) str->c_ptr());
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_datetime(const DRIZZLE_TIME *l_time, String *str)
{
  uint32_t length= (uint32_t) my_datetime_to_str(l_time, (char*) str->c_ptr());
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_truncated_value_warning(Session *session, DRIZZLE_ERROR::enum_warning_level level,
                                  const char *str_val,
				  uint32_t str_length,
                                  enum enum_drizzle_timestamp_type time_type,
                                  const char *field_name)
{
  char warn_buff[DRIZZLE_ERRMSG_SIZE];
  const char *type_str;
  CHARSET_INFO *cs= &my_charset_utf8_general_ci;
  char buff[128];
  String str(buff,(uint32_t) sizeof(buff), system_charset_info);
  str.copy(str_val, str_length, system_charset_info);
  str[str_length]= 0;               // Ensure we have end 0 for snprintf

  switch (time_type) {
    case DRIZZLE_TIMESTAMP_DATE:
      type_str= "date";
      break;
    case DRIZZLE_TIMESTAMP_TIME:
      type_str= "time";
      break;
    case DRIZZLE_TIMESTAMP_DATETIME:  // FALLTHROUGH
    default:
      type_str= "datetime";
      break;
  }
  if (field_name)
    cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                       ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                       type_str, str.c_ptr(), field_name,
                       (uint32_t) session->row_count);
  else
  {
    if (time_type > DRIZZLE_TIMESTAMP_ERROR)
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_TRUNCATED_WRONG_VALUE),
                         type_str, str.c_ptr());
    else
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_WRONG_VALUE), type_str, str.c_ptr());
  }
  push_warning(session, level,
               ER_TRUNCATED_WRONG_VALUE, warn_buff);
}

bool date_add_interval(DRIZZLE_TIME *ltime, interval_type int_type, INTERVAL interval)
{
  long period, sign;

  ltime->neg= 0;

  sign= (interval.neg ? -1 : 1);

  switch (int_type) {
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
  {
    int64_t sec, days, daynr, microseconds, extra_sec;
    ltime->time_type= DRIZZLE_TIMESTAMP_DATETIME; // Return full date
    microseconds= ltime->second_part + sign*interval.second_part;
    extra_sec= microseconds/1000000L;
    microseconds= microseconds%1000000L;

    sec=((ltime->day-1)*3600*24L+ltime->hour*3600+ltime->minute*60+
	 ltime->second +
	 sign* (int64_t) (interval.day*3600*24L +
                           interval.hour*3600L+interval.minute*60L+
                           interval.second))+ extra_sec;
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
    get_date_from_daynr((long) daynr, &ltime->year, &ltime->month,
                        &ltime->day);
    break;
  }
  case INTERVAL_DAY:
  case INTERVAL_WEEK:
    period= (calc_daynr(ltime->year,ltime->month,ltime->day) +
             sign * (long) interval.day);
    /* Daynumber from year 0 to 9999-12-31 */
    if (period > MAX_DAY_NUMBER)
      goto invalid_date;
    get_date_from_daynr((long) period,&ltime->year,&ltime->month,&ltime->day);
    break;
  case INTERVAL_YEAR:
    ltime->year+= sign * (long) interval.year;
    if (ltime->year >= 10000L)
      goto invalid_date;
    if (ltime->month == 2 && ltime->day == 29 &&
	calc_days_in_year(ltime->year) != 366)
      ltime->day=28;				// Was leap-year
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_QUARTER:
  case INTERVAL_MONTH:
    period= (ltime->year*12 + sign * (long) interval.year*12 +
	     ltime->month-1 + sign * (long) interval.month);
    if (period >= 120000L)
      goto invalid_date;
    ltime->year= (uint32_t) (period / 12);
    ltime->month= (uint32_t) (period % 12L)+1;
    /* Adjust day if the new month doesn't have enough days */
    if (ltime->day > days_in_month[ltime->month-1])
    {
      ltime->day = days_in_month[ltime->month-1];
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


/*
  Calculate difference between two datetime values as seconds + microseconds.

  SYNOPSIS
    calc_time_diff()
      l_time1         - TIME/DATE/DATETIME value
      l_time2         - TIME/DATE/DATETIME value
      l_sign          - 1 absolute values are substracted,
                        -1 absolute values are added.
      seconds_out     - Out parameter where difference between
                        l_time1 and l_time2 in seconds is stored.
      microseconds_out- Out parameter where microsecond part of difference
                        between l_time1 and l_time2 is stored.

  NOTE
    This function calculates difference between l_time1 and l_time2 absolute
    values. So one should set l_sign and correct result if he want to take
    signs into account (i.e. for DRIZZLE_TIME values).

  RETURN VALUES
    Returns sign of difference.
    1 means negative result
    0 means positive result

*/

bool
calc_time_diff(DRIZZLE_TIME *l_time1, DRIZZLE_TIME *l_time2, int l_sign, int64_t *seconds_out,
               long *microseconds_out)
{
  long days;
  bool neg;
  int64_t microseconds;

  /*
    We suppose that if first argument is DRIZZLE_TIMESTAMP_TIME
    the second argument should be TIMESTAMP_TIME also.
    We should check it before calc_time_diff call.
  */
  if (l_time1->time_type == DRIZZLE_TIMESTAMP_TIME)  // Time value
    days= (long)l_time1->day - l_sign * (long)l_time2->day;
  else
  {
    days= calc_daynr((uint32_t) l_time1->year,
		     (uint32_t) l_time1->month,
		     (uint32_t) l_time1->day);
    if (l_time2->time_type == DRIZZLE_TIMESTAMP_TIME)
      days-= l_sign * (long)l_time2->day;
    else
      days-= l_sign*calc_daynr((uint32_t) l_time2->year,
			       (uint32_t) l_time2->month,
			       (uint32_t) l_time2->day);
  }

  microseconds= ((int64_t)days*86400L +
                 (int64_t)(l_time1->hour*3600L +
                            l_time1->minute*60L +
                            l_time1->second) -
                 l_sign*(int64_t)(l_time2->hour*3600L +
                                   l_time2->minute*60L +
                                   l_time2->second)) * 1000000L +
                (int64_t)l_time1->second_part -
                l_sign*(int64_t)l_time2->second_part;

  neg= 0;
  if (microseconds < 0)
  {
    microseconds= -microseconds;
    neg= 1;
  }
  *seconds_out= microseconds/1000000L;
  *microseconds_out= (long) (microseconds%1000000L);
  return neg;
}


/*
  Compares 2 DRIZZLE_TIME structures

  SYNOPSIS
    my_time_compare()

      a - first time
      b - second time

  RETURN VALUE
   -1   - a < b
    0   - a == b
    1   - a > b

  NOTES
    TIME.second_part is not considered during comparison
*/

int
my_time_compare(DRIZZLE_TIME *a, DRIZZLE_TIME *b)
{
  uint64_t a_t= TIME_to_uint64_t_datetime(a);
  uint64_t b_t= TIME_to_uint64_t_datetime(b);

  if (a_t > b_t)
    return 1;
  else if (a_t < b_t)
    return -1;

  return 0;
}

#endif
