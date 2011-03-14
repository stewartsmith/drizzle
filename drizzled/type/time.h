/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#pragma once

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <drizzled/sql_string.h>

namespace drizzled
{

extern uint64_t log_10_int[20];
extern unsigned char days_in_month[];

/* Time handling defaults */
#define TIMESTAMP_MIN_YEAR (1900 + YY_PART_YEAR - 1)
#define TIMESTAMP_MAX_VALUE INT32_MAX
#define TIMESTAMP_MIN_VALUE 1

/* two-digit years < this are 20..; >= this are 19.. */
#define YY_PART_YEAR	   70

/* Flags to str_to_datetime */
#define TIME_FUZZY_DATE		1
#define TIME_DATETIME_ONLY	2

/* Must be same as MODE_NO_ZERO_IN_DATE */
#define TIME_NO_ZERO_IN_DATE    (65536L*2*2*2*2*2*2*2)

/* Must be same as MODE_NO_ZERO_DATE */
#define TIME_NO_ZERO_DATE	(TIME_NO_ZERO_IN_DATE*2)
#define TIME_INVALID_DATES	(TIME_NO_ZERO_DATE*2)

#define DRIZZLE_TIME_WARN_TRUNCATED    1
#define DRIZZLE_TIME_WARN_OUT_OF_RANGE 2

/* Limits for the TIME data type */
#define TIME_MAX_HOUR 838
#define TIME_MAX_MINUTE 59
#define TIME_MAX_SECOND 59
#define TIME_MAX_VALUE (TIME_MAX_HOUR*10000 + TIME_MAX_MINUTE*100 + \
                        TIME_MAX_SECOND)

/*
  Structure which is used to represent datetime values inside Drizzle.

  We assume that values in this structure are normalized, i.e. year <= 9999,
  month <= 12, day <= 31, hour <= 23, hour <= 59, hour <= 59. Many functions
  in server such as my_system_gmt_sec() or make_time() family of functions
  rely on this (actually now usage of make_*() family relies on a bit weaker
  restriction). Also functions that produce type::Time as result ensure this.
  There is one exception to this rule though if this structure holds time
  value (time_type == DRIZZLE_TIMESTAMP_TIME) days and hour member can hold
  bigger values.
*/
namespace type {

enum timestamp_t
{
  DRIZZLE_TIMESTAMP_NONE= -2, DRIZZLE_TIMESTAMP_ERROR= -1,
  DRIZZLE_TIMESTAMP_DATE= 0, DRIZZLE_TIMESTAMP_DATETIME= 1, DRIZZLE_TIMESTAMP_TIME= 2
};

enum cut_t
{
  VALID= 0,
  CUT= 1,
  INVALID= 2
};

/*
  datatime_t while being stored in an integer is actually a formatted value.
*/
typedef int64_t datetime_t;
typedef int64_t date_t;

inline bool is_valid(const datetime_t &value)
{
  if (value == -1L)
    return false;

  return true;
}

class Time
{
public:
  typedef uint32_t usec_t;
  typedef int64_t epoch_t;

  Time()
  {
    reset();
  }

  Time(uint32_t year_arg,
       uint32_t month_arg,
       uint32_t day_arg,
       uint32_t hour_arg,
       uint32_t minute_arg,
       uint32_t second_arg,
       usec_t second_part_arg,
       timestamp_t type_arg) :
    year(year_arg),
    month(month_arg),
    day(day_arg),
    hour(hour_arg),
    minute(minute_arg),
    second(second_arg),
    second_part(second_part_arg),
    neg(false),
    time_type(type_arg),
    _is_local_time(false)
  {
  }

  Time(uint32_t hour_arg,
       uint32_t minute_arg,
       uint32_t second_arg,
       usec_t second_part_arg,
       bool neg_arg) :
    year(0),
    month(0),
    day(0),
    hour(hour_arg),
    minute(minute_arg),
    second(second_arg),
    second_part(second_part_arg),
    neg(neg_arg),
    time_type(DRIZZLE_TIMESTAMP_TIME),
    _is_local_time(false)
  {
  }

  uint32_t year, month, day, hour, minute, second;
  usec_t second_part;
  bool neg;
  timestamp_t time_type;
  bool _is_local_time;

  void reset()
  {
    year= month= day= hour= minute= second= second_part= 0;
    neg= false;
    time_type= DRIZZLE_TIMESTAMP_DATE;
    _is_local_time= false;
  }

  timestamp_t type() const
  {
    return time_type;
  }

  void convert(drizzled::String &str, timestamp_t arg= type::DRIZZLE_TIMESTAMP_DATETIME);
  void convert(char *str, size_t &to_length, timestamp_t arg= type::DRIZZLE_TIMESTAMP_DATETIME);
  void convert(datetime_t &datetime, timestamp_t arg= type::DRIZZLE_TIMESTAMP_DATETIME);
  void convert(datetime_t &ret, int64_t nr, uint32_t flags);
  void convert(datetime_t &ret, int64_t nr, uint32_t flags, type::cut_t &was_cut);
  void convert(type::Time::epoch_t &epoch, long *my_timezone,
               bool *in_dst_time_gap, bool skip_timezone= false) const;

  void truncate(const timestamp_t arg);

  bool store(const char *str,uint32_t length, int &warning, type::timestamp_t arg= DRIZZLE_TIMESTAMP_TIME);
  type::timestamp_t store(const char *str, uint32_t length, uint32_t flags, type::cut_t &was_cut);
  type::timestamp_t store(const char *str, uint32_t length, uint32_t flags);
  void store(const type::Time::epoch_t &from, bool use_localtime= false);
  void store(const type::Time::epoch_t &from, const usec_t &from_fractional_seconds, bool use_localtime= false);
  void store(const struct tm &from);
  void store(const struct timeval &from);


  static const uint32_t FRACTIONAL_DIGITS= 1000000;
  static const size_t MAX_STRING_LENGTH= 32;   // +32 to make my_snprintf_{8bit|ucs2} happy

  bool check(bool not_zero_date, uint32_t flags, type::cut_t &was_cut) const;

  inline bool isValidEpoch() const
  {
    if ((year < TIMESTAMP_MIN_YEAR) or (year == TIMESTAMP_MIN_YEAR && (month < 12 || day < 31)))
    {
      return false;
    }

    return true;
  }
};

}

long calc_daynr(uint32_t year,uint32_t month,uint32_t day);
uint32_t calc_days_in_year(uint32_t year);
uint32_t year_2000_handling(uint32_t year);

void init_time(void);

/*
  Available interval types used in any statement.

  'interval_type' must be sorted so that simple intervals comes first,
  ie year, quarter, month, week, day, hour, etc. The order based on
  interval size is also important and the intervals should be kept in a
  large to smaller order. (get_interval_value() depends on this)

  Note: If you change the order of elements in this enum you should fix
  order of elements in 'interval_type_to_name' and 'interval_names'
  arrays

  See also interval_type_to_name, get_interval_value, interval_names
*/

enum interval_type
{
  INTERVAL_YEAR, INTERVAL_QUARTER, INTERVAL_MONTH, INTERVAL_WEEK, INTERVAL_DAY,
  INTERVAL_HOUR, INTERVAL_MINUTE, INTERVAL_SECOND, INTERVAL_MICROSECOND,
  INTERVAL_YEAR_MONTH, INTERVAL_DAY_HOUR, INTERVAL_DAY_MINUTE,
  INTERVAL_DAY_SECOND, INTERVAL_HOUR_MINUTE, INTERVAL_HOUR_SECOND,
  INTERVAL_MINUTE_SECOND, INTERVAL_DAY_MICROSECOND, INTERVAL_HOUR_MICROSECOND,
  INTERVAL_MINUTE_MICROSECOND, INTERVAL_SECOND_MICROSECOND, INTERVAL_LAST
};

} /* namespace drizzled */

