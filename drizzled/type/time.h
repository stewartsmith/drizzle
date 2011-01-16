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

#ifndef DRIZZLED_TYPE_TIME_H
#define DRIZZLED_TYPE_TIME_H

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
#define TIME_MAX_VALUE_SECONDS (TIME_MAX_HOUR * 3600L + \
                                TIME_MAX_MINUTE * 60L + TIME_MAX_SECOND)

enum enum_drizzle_timestamp_type
{
  DRIZZLE_TIMESTAMP_NONE= -2, DRIZZLE_TIMESTAMP_ERROR= -1,
  DRIZZLE_TIMESTAMP_DATE= 0, DRIZZLE_TIMESTAMP_DATETIME= 1, DRIZZLE_TIMESTAMP_TIME= 2
};


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

class Time
{
public:
  typedef uint32_t usec_t;
  typedef int64_t epoch_t;

  Time()
  {
    reset();
  }

  Time(unsigned int year_arg,
       unsigned int month_arg,
       unsigned int day_arg,
       unsigned int hour_arg,
       unsigned int minute_arg,
       unsigned int second_arg,
       usec_t second_part_arg,
       enum_drizzle_timestamp_type type_arg) :
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

  Time(unsigned int hour_arg,
       unsigned int minute_arg,
       unsigned int second_arg,
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

  unsigned int year, month, day, hour, minute, second;
  usec_t second_part;
  bool neg;
  enum enum_drizzle_timestamp_type time_type;
  bool _is_local_time;

  void reset()
  {
    year= month= day= hour= minute= second= second_part= 0;
    neg= false;
    time_type= DRIZZLE_TIMESTAMP_DATE;
    _is_local_time= false;
  }

  void convert(drizzled::String &str, enum_drizzle_timestamp_type arg= DRIZZLE_TIMESTAMP_DATETIME);
  void store(const time_t &from, bool use_localtime= false);
  void store(const time_t &from, const usec_t &from_fractional_seconds, bool use_localtime= false);

  static const uint32_t FRACTIONAL_DIGITS= 1000000;
};

}

bool check_date(const type::Time *ltime, bool not_zero_date,
                   uint32_t flags, int *was_cut);
enum enum_drizzle_timestamp_type
str_to_datetime(const char *str, uint32_t length, type::Time *l_time,
                uint32_t flags, int *was_cut);
int64_t number_to_datetime(int64_t nr, type::Time *time_res,
                            uint32_t flags, int *was_cut);
uint64_t TIME_to_uint64_t_datetime(const type::Time *);
uint64_t TIME_to_uint64_t(const type::Time *);


bool str_to_time(const char *str,uint32_t length, type::Time *l_time, int *warning);

long calc_daynr(uint32_t year,uint32_t month,uint32_t day);
uint32_t calc_days_in_year(uint32_t year);
uint32_t year_2000_handling(uint32_t year);

void init_time(void);


/*
  Function to check sanity of a TIMESTAMP value

  DESCRIPTION
    Check if a given type::Time value fits in TIMESTAMP range.
    This function doesn't make precise check, but rather a rough
    estimate.

  RETURN VALUES
    false   The value seems sane
    true    The type::Time value is definitely out of range
*/

static inline bool validate_timestamp_range(const type::Time *t)
{
  if ((t->year < TIMESTAMP_MIN_YEAR) or (t->year == TIMESTAMP_MIN_YEAR && (t->month < 12 || t->day < 31)))
  {
    return false;
  }

  return true;
}

time_t
my_system_gmt_sec(const type::Time *t, long *my_timezone,
                  bool *in_dst_time_gap, bool skip_timezone= false);

/*
  Required buffer length for my_time_to_str, my_date_to_str,
  my_datetime_to_str and TIME_to_string functions. Note, that the
  caller is still responsible to check that given TIME structure
  has values in valid ranges, otherwise size of the buffer could
  be not enough. We also rely on the fact that even wrong values
  sent using binary protocol fit in this buffer.
*/
#define MAX_DATE_STRING_REP_LENGTH 30

int my_TIME_to_str(const type::Time *l_time, char *to);

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

#endif /* DRIZZLED_TYPE_TIME_H */
