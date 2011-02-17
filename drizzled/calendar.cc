/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

/**
 * @file 
 *
 * Common functions for dealing with calendrical calculations
 */

#include <config.h>

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
#include <cstdlib>

#include <drizzled/calendar.h>

namespace drizzled
{

/** Static arrays for number of days in a month and their "day ends" */
static const uint32_t __leap_days_in_month[12]=       {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const uint32_t __normal_days_in_month[12]=     {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const uint32_t __leap_days_to_end_month[13]=   {0, 31, 60, 91, 121, 151, 182, 213, 244, 274, 305, 335, 366};
static const uint32_t __normal_days_to_end_month[13]= {0, 31, 59, 90, 120, 150, 181, 212, 243, 273, 304, 334, 365};

/** 
 * Private utility macro for enabling a switch between
 * Gregorian and Julian leap year date arrays.
 */
inline static const uint32_t* days_in_month(uint32_t y, enum calendar c) 
{
  if (is_leap_year(y, c))
    return __leap_days_in_month;
  else
    return __normal_days_in_month;
}

inline static const uint32_t* days_to_end_month(uint32_t y, enum calendar c) 
{
  if (is_leap_year(y, c))
    return __leap_days_to_end_month;
  else
    return __normal_days_to_end_month;
}


/**
 * Calculates the Julian Day Number from the year, month 
 * and day supplied.  The calendar used by the supplied
 * year, month, and day is assumed to be Gregorian Proleptic.
 *
 * The months January to December are 1 to 12. 
 * Astronomical year numbering is used, thus 1 BC is 0, 2 BC is −1, 
 * and 4713 BC is −4712. In all divisions (except for JD) the floor 
 * function is applied to the quotient (for dates since 
 * March 1, −4800 all quotients are non-negative, so we can also 
 * apply truncation).
 *
 * a = (14 - month) / 12
 * y = year + 4800 - a
 * m = month + 12a - 3
 * JDN = day + ((153m + 2) / 5) + 365y + (y / 4) - (y / 100) + (y / 400) - 32045
 *
 * @cite http://en.wikipedia.org/wiki/Julian_day#Calculation
 *
 * @note
 *
 * Year month and day values are assumed to be valid.  This 
 * method does no bounds checking or validation.
 *
 * @param Year of date
 * @param Month of date
 * @param Day of date
 */
int64_t julian_day_number_from_gregorian_date(uint32_t year, uint32_t month, uint32_t day)
{
  int64_t day_number;
  int64_t a= (14 - month) / 12;
  int64_t y= year + 4800 - a;
  int64_t m= month + (12 * a) - 3;

  day_number= day + (((153 * m) + 2) / 5) + (365 * y) + (y / 4) - (y / 100) + (y / 400) - 32045;
  return day_number;
}

/**
 * Translates an absolute day number to a 
 * Julian day number.  Note that a Julian day number
 * is not the same as a date in the Julian proleptic calendar.
 *
 * @param The absolute day number
 */
int64_t absolute_day_number_to_julian_day_number(int64_t absolute_day)
{
  return absolute_day + JULIAN_DAY_NUMBER_AT_ABSOLUTE_DAY_ONE;
}

/**
 * Translates a Julian day number to an 
 * absolute day number.  Note that a Julian day number
 * is not the same as a date in the Julian proleptic calendar.
 *
 * @param The Julian day number
 */
int64_t julian_day_number_to_absolute_day_number(int64_t julian_day)
{
  return julian_day - JULIAN_DAY_NUMBER_AT_ABSOLUTE_DAY_ONE;
}

/**
 * Given a supplied Julian Day Number, populates a year, month, and day
 * with the date in the Gregorian Proleptic calendar which corresponds to
 * the given Julian Day Number.
 *
 * @cite Algorithm from http://en.wikipedia.org/wiki/Julian_day
 *
 * @param Julian Day Number
 * @param Pointer to year to populate
 * @param Pointer to month to populate
 * @param Pointer to the day to populate
 */
void gregorian_date_from_julian_day_number(int64_t julian_day
                                         , uint32_t *year_out
                                         , uint32_t *month_out
                                         , uint32_t *day_out)
{
  int64_t j = julian_day + 32044;
  int64_t g = j / 146097;
  int64_t dg = j % 146097;
  int64_t c = (dg / 36524 + 1) * 3 / 4;
  int64_t dc = dg - c * 36524;
  int64_t b = dc / 1461;
  int64_t db = dc % 1461;
  int64_t a = (db / 365 + 1) * 3 / 4;
  int64_t da = db - a * 365;
  int64_t y = g * 400 + c * 100 + b * 4 + a;
  int64_t m = (da * 5 + 308) / 153 - 2;
  int64_t d = da - (m + 4) * 153 / 5 + 122;
  int64_t Y = y - 4800 + (m + 2) / 12;
  int64_t M = (m + 2) % 12 + 1;
  int64_t D = (int64_t)((double)d + 1.5);

  /* Push out parameters */
  *year_out= (uint32_t) Y;
  *month_out= (uint32_t) M;
  *day_out= (uint32_t) D;
}

/**
 * Given a supplied Absolute Day Number, populates a year, month, and day
 * with the date in the Gregorian Proleptic calendar which corresponds to
 * the given Absolute Day Number.
 *
 * @param Absolute Day Number
 * @param Pointer to year to populate
 * @param Pointer to month to populate
 * @param Pointer to the day to populate
 */
void gregorian_date_from_absolute_day_number(int64_t absolute_day
                                           , uint32_t *year_out
                                           , uint32_t *month_out
                                           , uint32_t *day_out)
{
  gregorian_date_from_julian_day_number(
      absolute_day_number_to_julian_day_number(absolute_day)
    , year_out
    , month_out
    , day_out);
}

/**
 * Functions to calculate the number of days in a 
 * particular year.  The number of days in a year 
 * depends on the calendar used for the date.
 *
 * For the Julian proleptic calendar, a leap year 
 * is one which is evenly divisible by 4.
 *
 * For the Gregorian proleptic calendar, a leap year
 * is one which is evenly divisible by 4, and if
 * the year is evenly divisible by 100, it must also be evenly
 * divisible by 400.
 */

/**
 * Returns the number of days in a particular year
 * depending on the supplied calendar.
 *
 * @param year to evaluate
 * @param calendar to use
 */
inline uint32_t days_in_year(const uint32_t year, enum calendar calendar)
{
  if (calendar == GREGORIAN)
    return days_in_year_gregorian(year);
  return days_in_year_julian(year);
}

/**
 * Returns the number of days in a particular Julian calendar year.
 *
 * @param year to evaluate
 */
inline uint32_t days_in_year_julian(const uint32_t year)
{
  /* Short-circuit. No odd years can be leap years... */
  return (year & 3) == 0;
}

/**
 * Returns the number of days in a particular Gregorian year.
 *
 * @param year to evaluate
 */
inline uint32_t days_in_year_gregorian(const uint32_t year)
{
  /* Short-circuit. No odd years can be leap years... */
  if ((year & 1) == 1)
    return 365;
  return (            
            (year & 3) == 0 
            && (year % 100 || ((year % 400 == 0) && year)) 
            ? 366 
            : 365
         );
}

/**
 * Returns the number of the day in a week.
 *
 * Return values:
 *
 * Day            Day Number  Sunday first day?
 * -------------- ----------- -----------------
 * Sunday         0           true
 * Monday         1           true
 * Tuesday        2           true
 * Wednesday      3           true
 * Thursday       4           true
 * Friday         5           true
 * Saturday       6           true
 * Sunday         6           false
 * Monday         0           false
 * Tuesday        1           false
 * Wednesday      2           false
 * Thursday       3           false
 * Friday         4           false
 * Saturday       5           false
 *
 * @param Julian Day Number
 * @param Consider Sunday the first day of the week?
 */
uint32_t day_of_week(int64_t day_number
                   , bool sunday_is_first_day_of_week)
{
  uint32_t tmp= (uint32_t) (day_number % 7);
  /* 0 returned from above modulo is a Monday */
  if (sunday_is_first_day_of_week)
    tmp= (tmp == 6 ? 0 : tmp + 1);
  return tmp;
}

/**
 * Given a year, month, and day, returns whether the date is 
 * valid for the Gregorian proleptic calendar.
 *
 * @param The year
 * @param The month
 * @param The day
 */
bool is_valid_gregorian_date(uint32_t year, uint32_t month, uint32_t day)
{
  if (year < 1)
    return false;
  if (month != 2)
    return (day <= __normal_days_in_month[month - 1]);
  else
  {
    const uint32_t *p_months= days_in_month(year, (enum calendar) GREGORIAN);
    return (day <= p_months[1]);
  }
}

/**
 * Returns the number of days in a month, given
 * a year and a month in the Gregorian calendar.
 *
 * @param Year in Gregorian Proleptic calendar
 * @param Month in date
 */
uint32_t days_in_gregorian_year_month(uint32_t year, uint32_t month)
{
  const uint32_t *p_months= days_in_month(year, GREGORIAN);
  return p_months[month - 1];
}

/**
 * Returns whether the supplied date components are within the 
 * range of the UNIX epoch.
 *
 * Times in the range of 1970-01-01T00:00:00 to 2038-01-19T03:14:07
 *
 * @param Year
 * @param Month
 * @param Day
 * @param Hour
 * @param Minute
 * @param Second
 */
bool in_unix_epoch_range(uint32_t year,
                         uint32_t month,
                         uint32_t day,
                         uint32_t hour,
                         uint32_t minute,
                         uint32_t second)
{
  if (month == 0 || day == 0)
    return false;

  if (year < UNIX_EPOCH_MAX_YEARS
      && year >= UNIX_EPOCH_MIN_YEARS)
    return true;

  if (year < UNIX_EPOCH_MIN_YEARS)
    return false;

  if (year == UNIX_EPOCH_MAX_YEARS)
  {
    if (month > 1)
    {
      return false;
    }
    if (day > 19)
    {
      return false;
    }
    else if (day < 19)
    {
      return true;
    }
    else
    {
      /* We are on the final day of UNIX Epoch */
      uint32_t seconds= (hour * 60 * 60)
                      + (minute * 60)
                      + (second);
      if (seconds <= ((3 * 60 * 60) + (14 * 60) + 7))
        return true;
      return false;
    }
  }
  return false;
}

/**
 * Returns the number of the week from a supplied year, month, and
 * date in the Gregorian proleptic calendar.  We use strftime() and
 * the %U, %W, and %V format specifiers depending on the value
 * of the sunday_is_first_day_of_week parameter.
 *
 * @param Subject year
 * @param Subject month
 * @param Subject day
 * @param Is sunday the first day of the week?
 * @param Pointer to a uint32_t to hold the resulting year, which 
 *        may be incremented or decremented depending on flags
 */
uint32_t week_number_from_gregorian_date(uint32_t year
                                       , uint32_t month
                                       , uint32_t day
                                       , bool sunday_is_first_day_of_week)
{
  struct tm broken_time;

  broken_time.tm_year= year;
  broken_time.tm_mon= month - 1; /* struct tm has non-ordinal months */
  broken_time.tm_mday= day;

  /* fill out the rest of our tm fields. */
  (void) mktime(&broken_time);

  char result[3]; /* 3 is enough space for a max 2-digit week number */
  size_t result_len= strftime(result
                            , sizeof(result)
                            , (sunday_is_first_day_of_week ? "%U" : "%W")
                            , &broken_time);

  if (result_len != 0)
    return (uint32_t) atoi(result);
  return 0;
}

/**
 * Returns the ISO week number of a supplied year, month, and
 * date in the Gregorian proleptic calendar.  We use strftime() and
 * the %V format specifier to do the calculation, which yields a
 * correct ISO 8601:1988 week number.
 *
 * The final year_out parameter is a pointer to an integer which will
 * be set to the year in which the week belongs, according to ISO8601:1988, 
 * which may be different from the Gregorian calendar year.
 *
 * @see http://en.wikipedia.org/wiki/ISO_8601
 *
 * @param Subject year
 * @param Subject month
 * @param Subject day
 * @param Pointer to a uint32_t to hold the resulting year, which 
 *        may be incremented or decremented depending on flags
 */
uint32_t iso_week_number_from_gregorian_date(uint32_t year
                                           , uint32_t month
                                           , uint32_t day)
{
  struct tm broken_time;

  broken_time.tm_year= year;
  broken_time.tm_mon= month - 1; /* struct tm has non-ordinal months */
  broken_time.tm_mday= day;

  /* fill out the rest of our tm fields. */
  (void) mktime(&broken_time);

  char result[3]; /* 3 is enough space for a max 2-digit week number */
  size_t result_len= strftime(result
                            , sizeof(result)
                            , "%V"
                            , &broken_time);


  if (result_len == 0)
    return 0; /* Not valid for ISO8601:1988 */

  uint32_t week_number= (uint32_t) atoi(result);

  return week_number;
}

/**
 * Takes a number in the form [YY]YYMM and converts it into
 * a number of months.
 *
 * @param Period in the form [YY]YYMM
 */
uint32_t year_month_to_months(uint32_t year_month)
{
  if (year_month == 0)
    return 0L;

  uint32_t years= year_month / 100;
  if (years < CALENDAR_YY_PART_YEAR)
    years+= 2000;
  else if (years < 100)
    years+= 1900;

  uint32_t months= year_month % 100;
  return (years * 12) + (months - 1);
}

/**
 * Takes a number of months and converts it to
 * a period in the form YYYYMM.
 *
 * @param Number of months
 */
uint32_t months_to_year_month(uint32_t months)
{
  if (months == 0L)
    return 0L;

  uint32_t years= (months / 12);

  if (years < 100)
    years+= (years < CALENDAR_YY_PART_YEAR) ? 2000 : 1900;

  return (years * 100) + (months % 12) + 1;
}

} /* namespace drizzled */
