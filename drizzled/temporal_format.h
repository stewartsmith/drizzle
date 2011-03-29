/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  Authors:
 *
 *  Jay Pipes <jay.pipes@sun.com>
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
 * Defines the API for matching datetime formats.
 */

#pragma once

#include <drizzled/common_fwd.h>

#include PCRE_HEADER

/* Output vector size for pcre matching.  Should be multiple of 3. */
#define OUT_VECTOR_SIZE 30

namespace drizzled {

class TemporalFormat
{
protected:
  const char *_pattern; /**< The regular expression string to match */
  pcre *_re; /**< The compiled regular expression struct */
  int32_t _error_offset; /**< Any error encountered during compilation or matching */
  const char *_error;
  /* Index of the pattern which is a specific temporal part */
  uint32_t _year_part_index;
  uint32_t _month_part_index;
  uint32_t _day_part_index;
  uint32_t _hour_part_index;
  uint32_t _minute_part_index;
  uint32_t _second_part_index;
  uint32_t _usecond_part_index;
  uint32_t _nsecond_part_index;
public:
  /**
   * Constructor which takes a regex string as
   * it's only parameter.
   *
   * @param Pattern to use in matching
   */
  TemporalFormat(const char *pattern);
  /**
   * Returns whether the instance is compiled
   * and contains a valid regular expression.
   */
  inline bool is_valid() const {return _re && (_error == NULL);}
  /**
   * Sets the index for the year part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_year_part_index(int32_t index) {_year_part_index= ((index - 1) * 2) + 2;}
  /**
   * Sets the index for the month part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_month_part_index(int32_t index) {_month_part_index= ((index - 1) * 2) + 2;}
  /**
   * Sets the index for the day part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_day_part_index(int32_t index) {_day_part_index= ((index - 1) * 2) + 2;}
  /**
   * Sets the index for the hour part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_hour_part_index(int32_t index) {_hour_part_index= ((index - 1) * 2) + 2;}
  /**
   * Sets the index for the minute part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_minute_part_index(int32_t index) {_minute_part_index= ((index - 1) * 2) + 2;}
  /**
   * Sets the index for the second part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_second_part_index(int32_t index) {_second_part_index= ((index - 1) * 2) + 2;}
  /**
   * Sets the index for the microsecond part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_usecond_part_index(int32_t index) {_usecond_part_index= ((index - 1) * 2) + 2;}
  /**
   * Sets the index for the nanosecond part of the pattern.
   *
   * @param index of the temporal part
   */
  inline void set_nsecond_part_index(int32_t index) {_nsecond_part_index= ((index - 1) * 2) + 2;}
  /**
   * Returns true or false whether a supplied
   * string matches the internal pattern for this
   * temporal format string.
   *
   * @param Subject to match
   * @param Length of subject string
   */
  bool matches(const char *data, size_t data_len, Temporal *to);
};


/**
 * Initializes the regular expressions used by the datetime
 * string matching conversion functions.
 *
 * Returns whether initialization was successful.
 *
 * @note
 *
 * This function is not thread-safe.  Call before threading
 * is initialized on server init.
 */
bool init_temporal_formats();
/** 
 * Frees all memory allocated for temporal format objects
 */
void deinit_temporal_formats();

} /* end namespace drizzled */

