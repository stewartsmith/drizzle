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

#include <config.h>

#include <drizzled/temporal.h>
#include <drizzled/temporal_format.h>
#include <drizzled/temporal_interval.h>

#include "temporal_generator.h"

using namespace drizzled;

void TemporalGenerator::DateGen::make_date(Date *date,
                                          uint32_t years, uint32_t months, uint32_t days)
{
  date->set_years(years);
  date->set_months(months);
  date->set_days(days);
  date->set_epoch_seconds();
}

void TemporalGenerator::DateGen::make_valid_date(drizzled::Date *date)
{
  date->set_years(2005);
  date->set_months(5);
  date->set_days(26);
  date->set_epoch_seconds();
}

void TemporalGenerator::TemporalGen::leap_day_in_leap_year(drizzled::Temporal *temporal)
{
  temporal->set_years(2008);
  temporal->set_months(2);
  temporal->set_days(29);
  temporal->set_epoch_seconds();
}

void TemporalGenerator::TemporalGen::leap_day_in_non_leap_year(drizzled::Temporal *temporal)
{
  temporal->set_years(2010);
  temporal->set_months(2);
  temporal->set_days(29);
  temporal->set_epoch_seconds();
}

void TemporalGenerator::TimeGen::make_time(drizzled::Time *time,
                                   uint32_t hours, uint32_t minutes, uint32_t seconds,
                                   uint32_t useconds)
{
  time->set_hours(hours);
  time->set_minutes(minutes);
  time->set_seconds(seconds);
  time->set_useconds(useconds);
  time->set_epoch_seconds();
}

void TemporalGenerator::TemporalGen::make_min_time(drizzled::Temporal *temporal)
{
  temporal->set_hours(0);
  temporal->set_minutes(0);
  temporal->set_seconds(0);
  temporal->set_epoch_seconds();
}

void TemporalGenerator::TemporalGen::make_max_time(drizzled::Temporal *temporal)
{
  temporal->set_hours(23);
  temporal->set_minutes(59);
  temporal->set_seconds(59);
  temporal->set_epoch_seconds();
}

void TemporalGenerator::DateTimeGen::make_datetime(drizzled::DateTime *datetime,
                   uint32_t years, uint32_t months, uint32_t days, uint32_t hours,
                   uint32_t minutes, uint32_t seconds, uint32_t useconds)
{
  datetime->set_years(years);
  datetime->set_months(months);
  datetime->set_days(days);
  datetime->set_hours(hours);
  datetime->set_minutes(minutes);
  datetime->set_seconds(seconds);
  datetime->set_useconds(useconds);
  datetime->set_epoch_seconds();
}                     

void TemporalGenerator::DateTimeGen::make_valid_datetime(drizzled::DateTime *datetime)
{
  datetime->set_years(1999);
  datetime->set_months(8);
  datetime->set_days(15);
  datetime->set_hours(13);
  datetime->set_minutes(34);
  datetime->set_seconds(6);
  datetime->set_epoch_seconds();
}

void TemporalGenerator::TimestampGen::make_timestamp(drizzled::Timestamp *timestamp,
                                             uint32_t years, uint32_t months, uint32_t days,
                                             uint32_t hours, uint32_t minutes, uint32_t seconds)
{
  timestamp->set_years(years);
  timestamp->set_months(months);
  timestamp->set_days(days);
  timestamp->set_hours(hours);
  timestamp->set_minutes(minutes);
  timestamp->set_seconds(seconds);
  timestamp->set_epoch_seconds();
}

void TemporalGenerator::TimestampGen::make_micro_timestamp(drizzled::MicroTimestamp *timestamp,
                                                   uint32_t years, uint32_t months, uint32_t days,
                                                   uint32_t hours, uint32_t minutes,
                                                   uint32_t seconds, uint32_t microseconds)
{
  make_timestamp(timestamp, years, months, days, hours, minutes, seconds);
  timestamp->set_useconds(microseconds);
  timestamp->set_epoch_seconds();
}
                                 
void TemporalGenerator::TimestampGen::make_nano_timestamp(drizzled::NanoTimestamp *timestamp,
                                                  uint32_t years, uint32_t months, uint32_t days,
                                                  uint32_t hours, uint32_t minutes,
                                                  uint32_t seconds, uint32_t nanoseconds)
{
  make_timestamp(timestamp, years, months, days, hours, minutes, seconds);
  timestamp->set_nseconds(nanoseconds);
  timestamp->set_epoch_seconds();
}

drizzled::TemporalFormat *TemporalGenerator::TemporalFormatGen::make_temporal_format(const char *regexp,
                                                                  int32_t year_part_index,
                                                                  int32_t month_part_index,
                                                                  int32_t day_part_index,
                                                                  int32_t hour_part_index,
                                                                  int32_t minute_part_index,
                                                                  int32_t second_part_index,
                                                                  int32_t usecond_part_index,
                                                                  int32_t nsecond_part_index)
{
  drizzled::TemporalFormat *temporal_format= new drizzled::TemporalFormat(regexp);

  temporal_format->set_year_part_index(year_part_index);
  temporal_format->set_month_part_index(month_part_index);
  temporal_format->set_day_part_index(day_part_index);
  temporal_format->set_hour_part_index(hour_part_index);
  temporal_format->set_minute_part_index(minute_part_index);
  temporal_format->set_second_part_index(second_part_index);
  temporal_format->set_usecond_part_index(usecond_part_index);
  temporal_format->set_nsecond_part_index(nsecond_part_index);

  return temporal_format;
}

drizzled::TemporalInterval *TemporalGenerator::TemporalIntervalGen::make_temporal_interval(
                                                                                           uint32_t  year,
                                                                                           uint32_t  month,
                                                                                           uint32_t  day,
                                                                                           uint32_t  hour,
                                                                                           uint64_t  minute,
                                                                                           uint64_t  second,
                                                                                           uint64_t  second_part,
                                                                                           bool neg)
{
  drizzled::TemporalInterval *interval= new drizzled::TemporalInterval();

  interval->set_year(year);
  interval->set_month(month);
  interval->set_day(day);
  interval->set_hour(hour);
  interval->set_minute(minute);
  interval->set_second(second);
  interval->set_second_part(second_part);
  interval->setNegative(neg);

  return interval;
}
