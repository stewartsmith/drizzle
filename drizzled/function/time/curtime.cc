/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Matthew Rheaume
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#include <drizzled/function/time/curtime.h>
#include <drizzled/temporal.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/current_session.h>

namespace drizzled
{

void Item_func_curtime::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  decimals=0;
  max_length=Time::MAX_STRING_LENGTH*MY_CHARSET_BIN_MB_MAXLEN;

  store_now_in_TIME(&ltime);

  ltime.year= ltime.month= ltime.day= 0;
  ltime.time_type = type::DRIZZLE_TIMESTAMP_TIME;

  cached_temporal.set_hours(ltime.hour);
  cached_temporal.set_minutes(ltime.minute);
  cached_temporal.set_seconds(ltime.second);
}

void Item_func_curtime_local::store_now_in_TIME(type::Time *now_time)
{
  (void) cached_temporal.from_time_t(current_session->times.getCurrentTimestampEpoch());

  now_time->year= 0;
  now_time->month= 0;
  now_time->day= 0;
  now_time->hour= cached_temporal.hours();
  now_time->minute= cached_temporal.minutes();
  now_time->second= cached_temporal.seconds();
}

void Item_func_curtime_utc::store_now_in_TIME(type::Time *now_time)
{
  (void) cached_temporal.from_time_t(current_session->times.getCurrentTimestampEpoch());

  now_time->year= 0;
  now_time->month= 0;
  now_time->day= 0;
  now_time->hour= cached_temporal.hours();
  now_time->minute= cached_temporal.minutes();
  now_time->second= cached_temporal.seconds();
}

bool Item_func_curtime::get_temporal(Time &to)
{
  to= cached_temporal;
  return true;
}

bool Item_func_curtime::get_time(type::Time &res)
{
  res= ltime;
  return 0;
}

} /* namespace drizzled */
