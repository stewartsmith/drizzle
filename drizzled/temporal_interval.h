/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  Authors: 
 *
 *  Clint Byrum <clint@fewbar.com>
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

#pragma once

/* @TODO Replace this include with some forward decls */
#include <drizzled/item.h>
#include <drizzled/type/time.h>

namespace drizzled 
{

/**
 * @brief
 *  Stores time interval for date/time manipulation
 */
class TemporalInterval
{
public:

  TemporalInterval(uint32_t in_year,
                   uint32_t in_month,
                   uint32_t in_day,
                   uint32_t in_hour,
                   uint64_t in_minute,
                   uint64_t in_second,
                   uint64_t in_second_part,
                   bool in_neg) :
    year(in_year),
    month(in_month),
    day(in_day),
    hour(in_hour),
    minute(in_minute),
    second(in_second),
    second_part(in_second_part),
    neg(in_neg)
  {}

  TemporalInterval() :
    year(0),
    month(0),
    day(0),
    hour(0),
    minute(0),
    second(0),
    second_part(0),
    neg(false)
  {}

  /**
   * Sets whether or not this object specifies a negative interval
   * @param[in] in_neg true if this is a negative interval, false if not
   */
  inline void setNegative(bool in_neg= true)
  {
    neg= in_neg;
  }

  /**
   * reverse boolean value of the negative flag
   */
  inline void toggleNegative()
  {
    neg= !neg;
  }

  /**
   * @retval true this is a negative temporal interval
   * @retval false this is a positive temporal interval
   */
  inline bool getNegative() const
  {
    return neg;
  }

  inline uint32_t  get_year() { return year; }
  inline void set_year(uint32_t new_year) { year = new_year; }

  inline uint32_t  get_month(){ return month; }
  inline void set_month(uint32_t new_month) { month = new_month; }

  inline uint32_t  get_day(){ return day; }
  inline void set_day(uint32_t new_day) { day = new_day; }

  inline uint32_t  get_hour(){ return hour; }
  inline void set_hour(uint32_t new_hour) { hour = new_hour; }

  inline uint64_t  get_minute(){ return minute; }
  inline void set_minute(uint32_t new_minute) { minute = new_minute; }

  inline uint64_t  get_second(){ return second; }
  inline void set_second(uint32_t new_second) { second = new_second; }

  inline uint64_t  get_second_part(){ return second_part; }
  inline void set_second_part(uint32_t new_second_part) { second_part = new_second_part; }

  /**
   * Populate this TemporalInterval from a string value
   *
   * To make code easy, allow interval objects without separators.
   *
   * @param args argument Item structure
   * @param int_type type of interval to create
   * @param str_value String pointer to the input value
   * @return true if the string would result in a null interval
   * 
   */
  bool initFromItem(Item *args, interval_type int_type, String *str_value);

  /**
   * Adds this interval to a DRIZZLE_LTIME structure
   *
   * @param[in,out] ltime the interval will be added to ltime directly in the ltime structure
   * @param[in] int_type the type of interval requested
   * @retval true date was added and value stored properly
   * @retval false result of addition is a null value
   */
  bool addDate(type::Time *ltime, interval_type int_type);

private:

  /**
   * The maximum number of text elements to extract into a temporal interval
   */
  static const uint32_t MAX_STRING_ELEMENTS = 5;

  /**
   * Each of these corresponds to an 'interval_type'
   */
  static const uint32_t NUM_YEAR_MONTH_STRING_ELEMENTS         = 2;
  static const uint32_t NUM_DAY_HOUR_STRING_ELEMENTS           = 2; 
  static const uint32_t NUM_DAY_MICROSECOND_STRING_ELEMENTS    = 5;
  static const uint32_t NUM_DAY_MINUTE_STRING_ELEMENTS         = 3;
  static const uint32_t NUM_DAY_SECOND_STRING_ELEMENTS         = 4;
  static const uint32_t NUM_HOUR_MICROSECOND_STRING_ELEMENTS   = 4;
  static const uint32_t NUM_HOUR_MINUTE_STRING_ELEMENTS        = 2;
  static const uint32_t NUM_HOUR_SECOND_STRING_ELEMENTS        = 3;
  static const uint32_t NUM_MINUTE_MICROSECOND_STRING_ELEMENTS = 3;
  static const uint32_t NUM_MINUTE_SECOND_STRING_ELEMENTS      = 2;
  static const uint32_t NUM_SECOND_MICROSECOND_STRING_ELEMENTS = 2;

  /**
   *  @details
   *  Get a array of positive numbers from a string object.
   *  Each number is separated by 1 non digit character
   *  Return error if there is too many numbers.
   *  If there is too few numbers, assume that the numbers are left out
   *  from the high end. This allows one to give:
   *  DAY_TO_SECOND as "D MM:HH:SS", "MM:HH:SS" "HH:SS" or as seconds.
   *
   *  @param[in] length:         length of str
   *  @param[in] cs:             charset of str
   *  @param[out] values:         array of results
   *  @param[out] count:          count of elements in result array
   *  @param transform_msec: if value is true we suppose
   *  that the last part of string value is microseconds
   *  and we should transform value to six digit value.
   *  For example, '1.1' -> '1.100000'
   */
  bool getIntervalFromString(const char *str,
                             uint32_t length, 
                             const charset_info_st * const cs,
                             uint32_t count, 
                             uint64_t *values,
                             bool transform_msec);

  uint32_t  year;
  uint32_t  month;
  uint32_t  day;
  uint32_t  hour;
  uint64_t  minute;
  uint64_t  second;
  uint64_t  second_part;
  bool      neg;

};

} /* namespace drizzled */

