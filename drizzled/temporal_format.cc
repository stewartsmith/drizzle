/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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
 * Implementation of the server's date and time string matching utility.
 */

#include <string> /** C++ string class used */
#include <string.h>
#include <vector>
#include <pcre.h>

#include "drizzled/global.h"
#include "drizzled/temporal_format.h"
#include "drizzled/temporal.h"

namespace drizzled
{

  TemporalFormat::TemporalFormat(const char *pattern)
  :
  _pattern(pattern)
, _error_offset(0)
, _error(NULL)
, _year_part_index(0)
, _month_part_index(0)
, _day_part_index(0)
, _hour_part_index(0)
, _minute_part_index(0)
, _second_part_index(0)
, _usecond_part_index(0)
{
  /* Make sure we've got no junk in the match_vector. */
  memset(_match_vector, 0, sizeof(_match_vector));

  /* Compile our regular expression */
  _re= pcre_compile(pattern
                    , 0 /* Default options */
                    , &_error
                    , &_error_offset
                    , NULL /* Use default character table */
                    );
}

bool TemporalFormat::matches(const char *data, size_t data_len, Temporal *to)
{
  if (! is_valid()) 
    return false;
  
  /* Simply check the subject against the compiled regular expression */
  int32_t result= pcre_exec(_re
                            , NULL /* No extra data */
                            , data
                            , data_len
                            , 0 /* Start at offset 0 of subject...*/
                            , 0 /* Default options */
                            , _match_vector
                            , OUT_VECTOR_SIZE
                            );
  if (result < 0)
  {
    switch (result)
    {
      case PCRE_ERROR_NOMATCH:
        return false; /* No match, just return false */
      default:
        return false;
    }
    return false;
  }

  int32_t expected_match_count= (_year_part_index > 1 ? 1 : 0)
                              + (_month_part_index > 1 ? 1 : 0)
                              + (_day_part_index > 1 ? 1 : 0)
                              + (_hour_part_index > 1 ? 1 : 0)
                              + (_minute_part_index > 1 ? 1 : 0)
                              + (_second_part_index > 1 ? 1 : 0)
                              + 1; /* Add one for the entire match... */
  if (result != expected_match_count)
    return false;

  /* C++ string class easy to use substr() method is very useful here */
  std::string copy_data(data, data_len);
  /* 
   * OK, we have the expected substring matches, so grab
   * the various temporal parts from the subject string
   *
   * @note 
   *
   * TemporalFormatMatch is a friend class to Temporal, so
   * we can access the temporal instance's protected data.
   */
  if (_year_part_index > 1)
  {
    size_t year_start= _match_vector[_year_part_index];
    size_t year_len= _match_vector[_year_part_index + 1] - _match_vector[_year_part_index];
    to->_years= atoi(copy_data.substr(year_start, year_len).c_str());
    if (year_len == 2)
      to->_years+= (to->_years >= DRIZZLE_YY_PART_YEAR ? 1900 : 2000);
  }
  if (_month_part_index > 1)
  {
    size_t month_start= _match_vector[_month_part_index];
    size_t month_len= _match_vector[_month_part_index + 1] - _match_vector[_month_part_index];
    to->_months= atoi(copy_data.substr(month_start, month_len).c_str());
  }
  if (_day_part_index > 1)
  {
    size_t day_start= _match_vector[_day_part_index];
    size_t day_len= _match_vector[_day_part_index + 1] - _match_vector[_day_part_index];
    to->_days= atoi(copy_data.substr(day_start, day_len).c_str());
  }
  if (_hour_part_index > 1)
  {
    size_t hour_start= _match_vector[_hour_part_index];
    size_t hour_len= _match_vector[_hour_part_index + 1] - _match_vector[_hour_part_index];
    to->_hours= atoi(copy_data.substr(hour_start, hour_len).c_str());
  }
  if (_minute_part_index > 1)
  {
    size_t minute_start= _match_vector[_minute_part_index];
    size_t minute_len= _match_vector[_minute_part_index + 1] - _match_vector[_minute_part_index];
    to->_minutes= atoi(copy_data.substr(minute_start, minute_len).c_str());
  }
  if (_second_part_index > 1)
  {
    size_t second_start= _match_vector[_second_part_index];
    size_t second_len= _match_vector[_second_part_index + 1] - _match_vector[_second_part_index];
    to->_seconds= atoi(copy_data.substr(second_start, second_len).c_str());
  }
  if (_usecond_part_index > 1)
  {
    size_t usecond_start= _match_vector[_usecond_part_index];
    size_t usecond_len= _match_vector[_usecond_part_index + 1] - _match_vector[_usecond_part_index];
    to->_useconds= atoi(copy_data.substr(usecond_start, usecond_len).c_str());
  }
  return true;
}

} /* end namespace drizzled */

#define COUNT_KNOWN_FORMATS 12

struct temporal_format_args
{
  const char *pattern;
  int32_t year_part_index;
  int32_t month_part_index;
  int32_t day_part_index;
  int32_t hour_part_index;
  int32_t minute_part_index;
  int32_t second_part_index;
  int32_t usecond_part_index;
};

/**
 * A collection of all known format strings.
 *
 * @note
 *
 * IMPORTANT: Make sure TIMESTAMP and DATETIME formats precede DATE formats and TIME formats, 
 * as the matching functionality matches on the first hit.
 *
 * @note 
 *
 * Remember to increment COUNT_KNOWN_FORMATS when you add a known format!
 */
static struct temporal_format_args __format_args[COUNT_KNOWN_FORMATS]= 
{
  {"(\\d{4})(\\d{2})(\\d{2})(\\d{2})(\\d{2})(\\d{2})", 1, 2, 3, 4, 5, 6, 0} /* YYYYMMDDHHmmSS */
, {"(\\d{4})(\\d{2})(\\d{2})(\\d{2})(\\d{2})(\\d{2})\\.(\\d{6})", 1, 2, 3, 4, 5, 6, 7} /* YYYYMMDDHHmmSS.uuuuuu */
, {"(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})[T|\\s+](\\d{2}):(\\d{2}):(\\d{2})", 1, 2, 3, 4, 5, 6, 0} /* YYYY-MM-DD[T]HH:mm:SS, YYYY.MM.DD[T]HH:mm:SS, YYYY/MM/DD[T]HH:mm:SS*/
, {"(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})", 1, 2, 3, 0, 0, 0, 0} /* YYYY-MM-DD, YYYY.MM.DD, YYYY/MM/DD */
, {"(\\d{4})(\\d{2})(\\d{2})", 1, 2, 3, 0, 0, 0, 0} /* YYYYMMDD */
, {"(\\d{2})[-/.]*(\\d{2})[-/.]*(\\d{4})", 3, 1, 2, 0, 0, 0, 0} /* MM[-/.]DD[-/.]YYYY (US common format)*/
, {"(\\d{2})[-/.]*(\\d{2})[-/.]*(\\d{2})", 1, 2, 3, 0, 0, 0, 0} /* YY[-/.]MM[-/.]DD */
, {"(\\d{2})[-/.]*(\\d{1,2})[-/.]*(\\d{1,2})", 1, 2, 3, 0, 0, 0, 0} /* YY[-/.][M]M[-/.][D]D */
, {"(\\d{2}):*(\\d{2}):*(\\d{2})\\.(\\d{6})", 0, 0, 0, 1, 2, 3, 4} /* HHmmSS.uuuuuu, HH:mm:SS.uuuuuu */
, {"(\\d{1,2}):*(\\d{2}):*(\\d{2})", 0, 0, 0, 1, 2, 3, 0} /* [H]HmmSS, [H]H:mm:SS */
, {"(\\d{1,2}):*(\\d{2})", 0, 0, 0, 0, 1, 2, 0} /* [m]mSS, [m]m:SS */
, {"(\\d{1,2})", 0, 0, 0, 0, 0, 1, 0} /* SS, S */
};

std::vector<drizzled::TemporalFormat*> known_datetime_formats;
std::vector<drizzled::TemporalFormat*> known_date_formats;
std::vector<drizzled::TemporalFormat*> known_time_formats;

/**
 * We allocate and initialize all known date/time formats.
 *
 * @TODO Cut down calls to new. Allocate as a block...
 */
bool init_temporal_formats()
{
  /* Compile all the regular expressions for the datetime formats */
  drizzled::TemporalFormat *tmp;
  struct temporal_format_args current_format_args;
  int32_t x;
  
  for (x= 0; x<COUNT_KNOWN_FORMATS; ++x)
  {
    current_format_args= __format_args[x];
    tmp= new drizzled::TemporalFormat(current_format_args.pattern);
    tmp->set_year_part_index(current_format_args.year_part_index);
    tmp->set_month_part_index(current_format_args.month_part_index);
    tmp->set_day_part_index(current_format_args.day_part_index);
    tmp->set_hour_part_index(current_format_args.hour_part_index);
    tmp->set_minute_part_index(current_format_args.minute_part_index);
    tmp->set_second_part_index(current_format_args.second_part_index);
    tmp->set_usecond_part_index(current_format_args.usecond_part_index);

    if (current_format_args.year_part_index > 0)
    {
      known_datetime_formats.push_back(tmp);
      if (current_format_args.hour_part_index == 0)
        known_date_formats.push_back(tmp);
    }
    if (current_format_args.hour_part_index > 0)
      if (current_format_args.year_part_index == 0)
        known_time_formats.push_back(tmp);
  }
  return true;
}
