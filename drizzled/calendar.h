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
 * Structures and functions for:
 *
 * Calculating day number in Gregorian and Julian proleptic calendars.
 * Converting between day numbers and dates in the calendars.
 * Converting between different calendars.
 * Calculating differences between dates.
 *
 * Works used in research:
 *
 * @cite "Calendrical Calculations", Dershowitz and Reingold
 * @cite ISO 8601 http://en.wikipedia.org/wiki/ISO_8601
 * @cite http://www.ddj.com/hpc-high-performance-computing/197006254 
 * @cite http://en.wikipedia.org/wiki/Julian_day#Calculation
 */

#pragma once

#define JULIAN_DAY_NUMBER_AT_ABSOLUTE_DAY_ONE INT64_C(1721425)

#define DAYS_IN_NORMAL_YEAR INT32_C(365)
#define DAYS_IN_LEAP_YEAR INT32_C(366)

#define UNIX_EPOCH_MIN_YEARS 1970
#define UNIX_EPOCH_MAX_YEARS 2038

#define CALENDAR_YY_PART_YEAR 70

/**
 * The following constants define the system of calculating the number
 * of days in various periods of time in the Gregorian calendar.
 *
 * Leap years (years containing 366 days) occur:
 *
 * - When the year is evenly divisible by 4
 * - If the year is evenly divisible by 100, it must also
 *   be evenly divisible by 400.
 */
#define GREGORIAN_DAYS_IN_400_YEARS UINT32_C(146097)
#define GREGORIAN_DAYS_IN_100_YEARS UINT32_C(36524)
#define GREGORIAN_DAYS_IN_4_YEARS   UINT32_C(1461)

namespace drizzled
{

/**
 * Different calendars supported by the temporal library
 */
enum calendar
{
  GREGORIAN= 1, 
  JULIAN= 2, 
  HEBREW= 3, 
  ISLAM= 4
};


/**
 * Calculates the Julian Day Number from the year, month 
 * and day supplied for a Gregorian Proleptic calendar date.
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
int64_t julian_day_number_from_gregorian_date(uint32_t year, uint32_t month, uint32_t day);

/**
 * Translates an absolute day number to a 
 * Julian day number.
 *
 * @param The absolute day number
 */
int64_t absolute_day_number_to_julian_day_number(int64_t absolute_day);

/**
 * Translates a Julian day number to an 
 * absolute day number.  
 *
 * @param The Julian day number
 */
int64_t julian_day_number_to_absolute_day_number(int64_t julian_day);

/**
 * Given a supplied Julian Day Number, populates a year, month, and day
 * with the date in the Gregorian Proleptic calendar which corresponds to
 * the given Julian Day Number.
 *
 * @param Julian Day Number
 * @param Pointer to year to populate
 * @param Pointer to month to populate
 * @param Pointer to the day to populate
 */
void gregorian_date_from_julian_day_number(int64_t julian_day
                                         , uint32_t *year_out
                                         , uint32_t *month_out
                                         , uint32_t *day_out);

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
                                           , uint32_t *day_out);

/**
 * Returns the number of days in a particular year.
 *
 * @param year to evaluate
 * @param calendar to use
 */
uint32_t days_in_year(uint32_t year, enum calendar calendar);

/**
 * Returns the number of days in a particular Gregorian Proleptic calendar year.
 *
 * @param year to evaluate
 */
uint32_t days_in_year_gregorian(uint32_t year);

/**
 * Returns the number of days in a particular Julian Proleptic calendar year.
 *
 * @param year to evaluate
 */
uint32_t days_in_year_julian(uint32_t year);

/**
 * Returns the number of leap years that have
 * occurred in the Julian Proleptic calendar
 * up to the supplied year.
 *
 * @param year to evaluate (1 - 9999)
 */
int32_t number_of_leap_years_julian(uint32_t year);

/**
 * Returns the number of leap years that have
 * occurred in the Gregorian Proleptic calendar
 * up to the supplied year.
 *
 * @param year to evaluate (1 - 9999)
 */
int32_t number_of_leap_years_gregorian(uint32_t year);

/**
 * Returns the number of days in a month, given
 * a year and a month in the Gregorian calendar.
 *
 * @param Year in Gregorian Proleptic calendar
 * @param Month in date
 */
uint32_t days_in_gregorian_year_month(uint32_t year, uint32_t month);

inline static bool num_leap_years(uint32_t y, enum calendar c) 
{
  return (c == GREGORIAN                
          ? number_of_leap_years_gregorian(y) 
          : number_of_leap_years_julian(y));
}

/**
 * Returns the number of the day in a week.
 *
 * @see temporal_to_number_days()
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
 * @param Number of days since start of Gregorian calendar.
 * @param Consider Sunday the first day of the week?
 */
uint32_t day_of_week(int64_t day_number, bool sunday_is_first_day_of_week);

/**
 * Given a year, month, and day, returns whether the date is 
 * valid for the Gregorian proleptic calendar.
 *
 * @param The year
 * @param The month
 * @param The day
 */
bool is_valid_gregorian_date(uint32_t year, uint32_t month, uint32_t day);

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
bool in_unix_epoch_range(uint32_t year
                       , uint32_t month
                       , uint32_t day
                       , uint32_t hour
                       , uint32_t minute
                       , uint32_t second);

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
                                       , bool sunday_is_first_day_of_week);

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
 */
uint32_t iso_week_number_from_gregorian_date(uint32_t year
                                           , uint32_t month
                                           , uint32_t day);
/**
 * Takes a number in the form [YY]YYMM and converts it into
 * a number of months.
 *
 * @param Period in the form [YY]YYMM
 */
uint32_t year_month_to_months(uint32_t year_month);

/**
 * Takes a number of months and converts it to
 * a period in the form YYYYMM.
 *
 * @param Number of months
 */
uint32_t months_to_year_month(uint32_t months);

/**
 * Simple function returning whether the supplied year
 * is a leap year in the supplied calendar.
 *
 * @param Year to evaluate
 * @param Calendar to use
 */
inline static bool is_leap_year(uint32_t y, enum calendar c)
{
  return (days_in_year(y, c) == 366);
}

/**
 * Simple function returning whether the supplied year
 * is a leap year in the Gregorian proleptic calendar.
 */
inline static bool is_gregorian_leap_year(uint32_t y)
{
  return (days_in_year_gregorian(y) == 366);
}

/**
 * Simple function returning whether the supplied year
 * is a leap year in the Julian proleptic calendar.
 */
inline static bool is_julian_leap_year(uint32_t y) 
{
  return (days_in_year_julian(y) == 366);
}

} /* namespace drizzled */

