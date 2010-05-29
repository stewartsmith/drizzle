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

#include "config.h"

#include <drizzled/temporal.h>

#include "generator.h"

using namespace drizzled;

void Generator::DateGen::make_date(Date *date,
                                          uint32_t years, uint32_t months, uint32_t days)
{
  date->set_years(years);
  date->set_months(months);
  date->set_days(days);
}

void Generator::DateGen::make_valid_date(drizzled::Date *date)
{
  date->set_years(2005);
  date->set_months(5);
  date->set_days(26);
}

void Generator::TemporalGen::leap_day_in_leap_year(drizzled::Temporal *temporal)
{
  temporal->set_years(2008);
  temporal->set_months(2);
  temporal->set_days(29);
}

void Generator::TemporalGen::leap_day_in_non_leap_year(drizzled::Temporal *temporal)
{
  temporal->set_years(2010);
  temporal->set_months(2);
  temporal->set_days(29);
}

void Generator::TimeGen::make_time(drizzled::Time *time,
                                   uint32_t hours, uint32_t minutes, uint32_t seconds)
{
  time->set_hours(hours);
  time->set_minutes(minutes);
  time->set_seconds(seconds);
}

void Generator::TemporalGen::make_min_time(drizzled::Temporal *temporal)
{
  temporal->set_hours(0);
  temporal->set_minutes(0);
  temporal->set_seconds(0);
}

void Generator::TemporalGen::make_max_time(drizzled::Temporal *temporal)
{
  temporal->set_hours(23);
  temporal->set_minutes(59);
  temporal->set_seconds(59);
}

void Generator::DateTimeGen::make_datetime(drizzled::DateTime *datetime,
                   uint32_t years, uint32_t months, uint32_t days, uint32_t hours,
                   uint32_t minutes, uint32_t seconds)
{
  datetime->set_years(years);
  datetime->set_months(months);
  datetime->set_days(days);
  datetime->set_hours(hours);
  datetime->set_minutes(minutes);
  datetime->set_seconds(seconds);
}                     

void Generator::DateTimeGen::make_valid_datetime(drizzled::DateTime *datetime)
{
  datetime->set_years(1999);
  datetime->set_months(8);
  datetime->set_days(15);
  datetime->set_hours(13);
  datetime->set_minutes(34);
  datetime->set_seconds(6);
}

void Generator::TimestampGen::make_timestamp(drizzled::Timestamp *timestamp,
                           uint32_t years, uint32_t months, uint32_t days)
{
  timestamp->set_years(years);
  timestamp->set_months(months);
  timestamp->set_days(days);
}