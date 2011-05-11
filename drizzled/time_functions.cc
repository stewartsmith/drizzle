/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/error.h>
#include <drizzled/util/test.h>
#include <drizzled/session.h>
#include <drizzled/time_functions.h>
#include <drizzled/charset.h>
#include <drizzled/system_variables.h>

namespace drizzled {

/* Some functions to calculate dates */


int calc_weekday(long daynr,bool sunday_first_day_of_week)
{
  return ((int) ((daynr + 5L + (sunday_first_day_of_week ? 1L : 0L)) % 7));
}


uint32_t calc_week(type::Time *l_time, uint32_t week_behaviour, uint32_t *year)
{
  uint32_t days;
  uint32_t daynr= calc_daynr(l_time->year,l_time->month,l_time->day);
  uint32_t first_daynr= calc_daynr(l_time->year,1,1);
  bool monday_first= test(week_behaviour & WEEK_MONDAY_FIRST);
  bool week_year= test(week_behaviour & WEEK_YEAR);
  bool first_weekday= test(week_behaviour & WEEK_FIRST_WEEKDAY);

  uint32_t weekday= calc_weekday(first_daynr, !monday_first);
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


void get_date_from_daynr(long daynr,
                         uint32_t *ret_year,
                         uint32_t *ret_month,
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


type::timestamp_t str_to_datetime_with_warn(Session *session,
                                            const char *str, 
                                            uint32_t length, 
                                            type::Time *l_time,
                                            uint32_t flags)
{
  type::cut_t was_cut= type::VALID;
  type::timestamp_t ts_type;

  ts_type= l_time->store(str, length,
                         (flags | (session->variables.sql_mode &
                                   (MODE_INVALID_DATES |
                                    MODE_NO_ZERO_DATE))),
                         was_cut);
  if (was_cut || ts_type <= type::DRIZZLE_TIMESTAMP_ERROR)
    make_truncated_value_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 str, length, ts_type,  NULL);

  return ts_type;
}


bool
str_to_time_with_warn(Session *session, const char *str, uint32_t length, type::Time *l_time)
{
  int warning;
  bool ret_val= l_time->store(str, length, warning, type::DRIZZLE_TIMESTAMP_TIME);
  if (ret_val || warning)
    make_truncated_value_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 str, length, type::DRIZZLE_TIMESTAMP_TIME, NULL);
  return ret_val;
}


void make_truncated_value_warning(Session *session, 
                                  DRIZZLE_ERROR::enum_warning_level level,
                                  const char *str_val,
				                          uint32_t str_length,
                                  type::timestamp_t time_type,
                                  const char *field_name)
{
  char warn_buff[DRIZZLE_ERRMSG_SIZE];
  const char *type_str;
  charset_info_st *cs= &my_charset_utf8_general_ci;
  char buff[128];
  String str(buff,(uint32_t) sizeof(buff), system_charset_info);
  str.copy(str_val, str_length, system_charset_info);
  str[str_length]= 0;               // Ensure we have end 0 for snprintf

  switch (time_type) {
  case type::DRIZZLE_TIMESTAMP_DATE:
    type_str= "date";
    break;

  case type::DRIZZLE_TIMESTAMP_TIME:
    type_str= "time";
    break;

  case type::DRIZZLE_TIMESTAMP_DATETIME:  // FALLTHROUGH
  default:
    type_str= "datetime";
    break;
  }

  if (field_name)
  {
    cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                       ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                       type_str, str.c_ptr(), field_name,
                       (uint32_t) session->row_count);
  }
  else
  {
    if (time_type > type::DRIZZLE_TIMESTAMP_ERROR)
    {
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_TRUNCATED_WRONG_VALUE),
                         type_str, str.c_ptr());
    }
    else
    {
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_WRONG_VALUE), type_str, str.c_ptr());
    }
  }
  push_warning(session, level,
               ER_TRUNCATED_WRONG_VALUE, warn_buff);
}


bool
calc_time_diff(type::Time *l_time1, type::Time *l_time2, int l_sign, int64_t *seconds_out,
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
  if (l_time1->time_type == type::DRIZZLE_TIMESTAMP_TIME)  // Time value
    days= (long)l_time1->day - l_sign * (long)l_time2->day;
  else
  {
    days= calc_daynr((uint32_t) l_time1->year,
                     (uint32_t) l_time1->month,
                     (uint32_t) l_time1->day);
    if (l_time2->time_type == type::DRIZZLE_TIMESTAMP_TIME)
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

} /* namespace drizzled */
