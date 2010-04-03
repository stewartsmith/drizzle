/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_CALENDAR_H
#define DRIZZLED_CALENDAR_H

namespace drizzled
{

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Different calendars supported by the temporal library
 */
enum calendar
{
  GREGORIAN= 1
, JULIAN= 2
, HEBREW= 3
, ISLAM= 4
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
 * Returns the number of days in a month, given
 * a year and a month in the Gregorian calendar.
 *
 * @param Year in Gregorian Proleptic calendar
 * @param Month in date
 */
uint32_t days_in_gregorian_year_month(uint32_t year, uint32_t month);

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
 * @param Pointer to a uint32_t to hold the resulting year, which 
 *        may be incremented or decremented depending on flags
 */
uint32_t iso_week_number_from_gregorian_date(uint32_t year
                                           , uint32_t month
                                           , uint32_t day
                                           , uint32_t *year_out);
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

#ifdef __cplusplus
}
#endif

} /* namespace drizzled */

#endif /* DRIZZLED_CALENDAR_H */
