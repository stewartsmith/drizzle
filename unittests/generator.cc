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

void make_datetime(drizzled::DateTime *datetime,
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
}                     

void make_timestamp(drizzled::Timestamp *timestamp,
                           uint32_t years, uint32_t months, uint32_t days)
{
  timestamp->set_years(years);
  timestamp->set_months(months);
  timestamp->set_days(days);
}