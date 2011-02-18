/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Pawel Blokus
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

#ifndef UNITTESTS_GENERATOR_H
#define UNITTESTS_GENERATOR_H

#include <config.h>

namespace drizzled
{
  class Date;
  class TemporalFormat;
  class Temporal;
  class Time;
  class DateTime;
  class Timestamp;
  class MicroTimestamp;
  class NanoTimestamp;
}

class TemporalGenerator 
{
public:

  class TemporalGen
  {
  public:
    static void leap_day_in_leap_year(drizzled::Temporal *temporal);
    static void leap_day_in_non_leap_year(drizzled::Temporal *temporal);
    static void make_min_time(drizzled::Temporal *temporal);
    static void make_max_time(drizzled::Temporal *temporal);
  };
  
  class DateGen
  {
  public:
    static void make_date(drizzled::Date *date, uint32_t years, uint32_t months, uint32_t days);
    static void make_valid_date(drizzled::Date *date);  
  };
  
  class TimeGen
  {
  public:
    static void make_time(drizzled::Time *time, uint32_t hours, uint32_t minutes, uint32_t seconds,
                          uint32_t useconds = 0);
  };
  
  class DateTimeGen
  {
  public:
    static void make_datetime(drizzled::DateTime *datetime,
                              uint32_t years, uint32_t months, uint32_t days, uint32_t hours,
                              uint32_t minutes, uint32_t seconds, uint32_t useconds = 0);
    static void make_valid_datetime(drizzled::DateTime *datetime);
  };
  
  class TimestampGen
  {
  public:
    static void make_timestamp(drizzled::Timestamp *timestamp,
                               uint32_t years, uint32_t months, uint32_t days, uint32_t hours,
                               uint32_t minutes, uint32_t seconds);
    static void make_micro_timestamp(drizzled::MicroTimestamp *timestamp,
                              uint32_t years, uint32_t months, uint32_t days, uint32_t hours,
                              uint32_t minutes, uint32_t seconds, uint32_t microseconds);
    static void make_nano_timestamp(drizzled::NanoTimestamp *timestamp,
                                uint32_t years, uint32_t months, uint32_t days, uint32_t hours,
                                uint32_t minutes, uint32_t seconds, uint32_t nanoseconds);
  };

  class TemporalFormatGen
  {
  public:
    static drizzled::TemporalFormat *make_temporal_format(const char *regexp,
                                     int32_t year_part_index,
                                     int32_t month_part_index,
                                     int32_t day_part_index,
                                     int32_t hour_part_index,
                                     int32_t minute_part_index,
                                     int32_t second_part_index,
                                     int32_t usecond_part_index,
                                     int32_t nsecond_part_index);
  };

  class TemporalIntervalGen
  {
  public:
    static drizzled::TemporalInterval *make_temporal_interval(
                                                              uint32_t  year,
                                                              uint32_t  month,
                                                              uint32_t  day,
                                                              uint32_t  hour,
                                                              uint64_t  minute,
                                                              uint64_t  second,
                                                              uint64_t  second_part,
                                                              bool neg);
  };
};

#endif /*UNITTESTS_GENERATOR_H*/
