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
 * Implementation of the server's date and time string matching utility.
 */

#include <config.h>

#include <boost/foreach.hpp>
#include <drizzled/temporal_format.h>
#include <drizzled/temporal.h>

#include <string.h>
#include PCRE_HEADER

#include <string>
#include <vector>

using namespace std;

namespace drizzled {

TemporalFormat::TemporalFormat(const char *pattern) :
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
, _nsecond_part_index(0)
{
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

  int32_t match_vector[OUT_VECTOR_SIZE]; /**< Stores match substring indexes */
  
  /* Make sure we've got no junk in the match_vector. */
  memset(match_vector, 0, sizeof(match_vector));

  /* Simply check the subject against the compiled regular expression */
  int32_t result= pcre_exec(_re
                            , NULL /* No extra data */
                            , data
                            , data_len
                            , 0 /* Start at offset 0 of subject...*/
                            , 0 /* Default options */
                            , match_vector
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
                              + (_usecond_part_index > 1 ? 1 : 0)
                              + (_nsecond_part_index > 1 ? 1 : 0)
                              + 1; /* Add one for the entire match... */
  if (result != expected_match_count)
    return false;

  /* C++ string class easy to use substr() method is very useful here */
  string copy_data(data, data_len);
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
    size_t year_start= match_vector[_year_part_index];
    size_t year_len= match_vector[_year_part_index + 1] - match_vector[_year_part_index];
    to->_years= atoi(copy_data.substr(year_start, year_len).c_str());
    if (year_len == 2)
      to->_years+= (to->_years >= DRIZZLE_YY_PART_YEAR ? 1900 : 2000);
  }
  if (_month_part_index > 1)
  {
    size_t month_start= match_vector[_month_part_index];
    size_t month_len= match_vector[_month_part_index + 1] - match_vector[_month_part_index];
    to->_months= atoi(copy_data.substr(month_start, month_len).c_str());
  }
  if (_day_part_index > 1)
  {
    size_t day_start= match_vector[_day_part_index];
    size_t day_len= match_vector[_day_part_index + 1] - match_vector[_day_part_index];
    to->_days= atoi(copy_data.substr(day_start, day_len).c_str());
  }
  if (_hour_part_index > 1)
  {
    size_t hour_start= match_vector[_hour_part_index];
    size_t hour_len= match_vector[_hour_part_index + 1] - match_vector[_hour_part_index];
    to->_hours= atoi(copy_data.substr(hour_start, hour_len).c_str());
  }
  if (_minute_part_index > 1)
  {
    size_t minute_start= match_vector[_minute_part_index];
    size_t minute_len= match_vector[_minute_part_index + 1] - match_vector[_minute_part_index];
    to->_minutes= atoi(copy_data.substr(minute_start, minute_len).c_str());
  }
  if (_second_part_index > 1)
  {
    size_t second_start= match_vector[_second_part_index];
    size_t second_len= match_vector[_second_part_index + 1] - match_vector[_second_part_index];
    to->_seconds= atoi(copy_data.substr(second_start, second_len).c_str());
  }
  if (_usecond_part_index > 1)
  {
    size_t usecond_start= match_vector[_usecond_part_index];
    size_t usecond_len= match_vector[_usecond_part_index + 1] - match_vector[_usecond_part_index];
    /* 
     * For microseconds, which are millionth of 1 second, 
     * we must ensure that we produce a correct result, 
     * even if < 6 places were specified.  For instance, if we get .1, 
     * we must produce 100000. .11 should produce 110000, etc.
     */
    uint32_t multiplier= 1;
    int32_t x= usecond_len;
    while (x < 6)
    {
      multiplier*= 10;
      ++x;
    }
    to->_useconds= atoi(copy_data.substr(usecond_start, usecond_len).c_str()) * multiplier;
  }
  if (_nsecond_part_index > 1)
  {
    size_t nsecond_start= match_vector[_nsecond_part_index];
    size_t nsecond_len= match_vector[_nsecond_part_index + 1] - match_vector[_nsecond_part_index];
    /* 
     * For nanoseconds, which are 1 billionth of a second, 
     * we must ensure that we produce a correct result, 
     * even if < 9 places were specified.  For instance, if we get .1, 
     * we must produce 100000000. .11 should produce 110000000, etc.
     */
    uint32_t multiplier= 1;
    int32_t x= nsecond_len;
    while (x < 9)
    {
      multiplier*= 10;
      ++x;
    }
    to->_nseconds= atoi(copy_data.substr(nsecond_start, nsecond_len).c_str()) * multiplier;
  }
  return true;
}


#define COUNT_KNOWN_FORMATS 19

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
  int32_t nsecond_part_index;
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
  {"^(\\d{4})(\\d{2})(\\d{2})(\\d{2})(\\d{2})(\\d{2})\\.(\\d{1,6})$", 1, 2, 3, 4, 5, 6, 7, 0} /* YYYYMMDDHHmmSS.uuuuuu */
, {"^(\\d{4})(\\d{2})(\\d{2})(\\d{2})(\\d{2})(\\d{2})$", 1, 2, 3, 4, 5, 6, 0, 0} /* YYYYMMDDHHmmSS */
, {"^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})[T|\\s+](\\d{2}):(\\d{2}):(\\d{2})\\.(\\d{1,6})$", 1, 2, 3, 4, 5, 6, 7, 0} /* YYYY[/-.]MM[/-.]DD[T]HH:mm:SS.uuuuuu */
, {"^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})[T|\\s+](\\d{2}):(\\d{2}):(\\d{2})$", 1, 2, 3, 4, 5, 6, 0, 0} /* YYYY[/-.][M]M[/-.][D]D[T]HH:mm:SS */
, {"^(\\d{2})[-/.](\\d{1,2})[-/.](\\d{1,2})[\\s+](\\d{2}):(\\d{2}):(\\d{2})$", 1, 2, 3, 4, 5, 6, 0, 0} /* YY[/-.][M]M[/-.][D]D HH:mm:SS */
, {"^(\\d{2})[-/.](\\d{1,2})[-/.](\\d{1,2})[\\s+](\\d{2}):(\\d{2})$", 1, 2, 3, 4, 5, 0, 0, 0} /* YY[/-.][M]M[/-.][D]D HH:mm */
, {"^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})[\\s+](\\d{2}):(\\d{2})$", 1, 2, 3, 4, 5, 0, 0, 0} /* YYYY[/-.][M]M[/-.][D]D HH:mm */
, {"^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})$", 1, 2, 3, 0, 0, 0, 0, 0} /* YYYY-[M]M-[D]D, YYYY.[M]M.[D]D, YYYY/[M]M/[D]D */ 
, {"^(\\d{4})(\\d{2})(\\d{2})$", 1, 2, 3, 0, 0, 0, 0, 0} /* YYYYMMDD */
, {"^(\\d{2})[-/.]*(\\d{2})[-/.]*(\\d{4})$", 3, 1, 2, 0, 0, 0, 0, 0} /* MM[-/.]DD[-/.]YYYY (US common format)*/
, {"^(\\d{2})[-/.]*(\\d{2})[-/.]*(\\d{2})$", 1, 2, 3, 0, 0, 0, 0, 0} /* YY[-/.]MM[-/.]DD */
, {"^(\\d{2})[-/.]*(\\d{1,2})[-/.]*(\\d{1,2})$", 1, 2, 3, 0, 0, 0, 0, 0} /* YY[-/.][M]M[-/.][D]D */
, {"^(\\d{4})[-/.]*(\\d{1,2})[-/.]*(\\d{1,2})$", 1, 2, 3, 0, 0, 0, 0, 0} /* YYYY[-/.][M]M[-/.][D]D */
, {"^(\\d{2}):*(\\d{2}):*(\\d{2})\\.(\\d{1,6})$", 0, 0, 0, 1, 2, 3, 4, 0} /* HHmmSS.uuuuuu, HH:mm:SS.uuuuuu */
, {"^(\\d{1,2}):*(\\d{2}):*(\\d{2})$", 0, 0, 0, 1, 2, 3, 0, 0} /* [H]HmmSS, [H]H:mm:SS */
, {"^(\\d{1,2}):(\\d{1,2}):(\\d{1,2})$", 0, 0, 0, 1, 2, 3, 0, 0} /* [H]H:[m]m:[S]S */
, {"^(\\d{1,2}):*(\\d{2})$", 0, 0, 0, 0, 1, 2, 0, 0} /* [m]mSS, [m]m:SS */
, {"^(\\d{1,2})$", 0, 0, 0, 0, 0, 1, 0, 0} /* SS, S */
, {"^(\\d{1,2})\\.(\\d{1,6})$", 0, 0, 0, 0, 0, 1, 2, 0} /* [S]S.uuuuuu */
};

vector<TemporalFormat *> known_datetime_formats;
vector<TemporalFormat *> known_date_formats;
vector<TemporalFormat *> known_time_formats;
vector<TemporalFormat *> all_temporal_formats;

/**
 * We allocate and initialize all known date/time formats.
 *
 * @TODO Cut down calls to new. Allocate as a block...
 */
bool init_temporal_formats()
{
  /* Compile all the regular expressions for the datetime formats */
  TemporalFormat *tmp;
  struct temporal_format_args current_format_args;
  
  for (int32_t x= 0; x < COUNT_KNOWN_FORMATS; ++x)
  {
    current_format_args= __format_args[x];
    tmp= new TemporalFormat(current_format_args.pattern);
    tmp->set_year_part_index(current_format_args.year_part_index);
    tmp->set_month_part_index(current_format_args.month_part_index);
    tmp->set_day_part_index(current_format_args.day_part_index);
    tmp->set_hour_part_index(current_format_args.hour_part_index);
    tmp->set_minute_part_index(current_format_args.minute_part_index);
    tmp->set_second_part_index(current_format_args.second_part_index);
    tmp->set_usecond_part_index(current_format_args.usecond_part_index);
    tmp->set_nsecond_part_index(current_format_args.nsecond_part_index);
    
    /* 
     * We store the pointer in all_temporal_formats because we 
     * delete pointers from that vector and only that vector
     */
    all_temporal_formats.push_back(tmp); 

    if (current_format_args.year_part_index > 0) /* A date must have a year */
    {
      known_datetime_formats.push_back(tmp);
      if (current_format_args.second_part_index == 0) /* A time must have seconds. */
        known_date_formats.push_back(tmp);
    }

    if (current_format_args.second_part_index > 0) /* A time must have seconds, but may not have minutes or hours */
      known_time_formats.push_back(tmp);
  }
  return true;
}

/** Free all allocated temporal formats */
void deinit_temporal_formats()
{
  BOOST_FOREACH(TemporalFormat* it, all_temporal_formats)
    delete it;
  known_date_formats.clear();
  known_datetime_formats.clear();
  known_time_formats.clear();
  all_temporal_formats.clear();
}

} /* end namespace drizzled */
