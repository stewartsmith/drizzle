/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>

#include <drizzled/function/time/curdate.h>
#include <drizzled/temporal.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/current_session.h>

namespace drizzled
{

void Item_func_curdate::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  decimals=0;
  max_length=Date::MAX_STRING_LENGTH*MY_CHARSET_BIN_MB_MAXLEN;

  store_now_in_TIME(&ltime);

  /* We don't need to set second_part and neg because they already 0 */
  ltime.hour= ltime.minute= ltime.second= 0;
  ltime.time_type= type::DRIZZLE_TIMESTAMP_DATE;

  /** 
   * @TODO Remove ltime completely when timezones are reworked.  Using this
   * technique now to avoid a large patch...
   */
  cached_temporal.set_years(ltime.year);
  cached_temporal.set_months(ltime.month);
  cached_temporal.set_days(ltime.day);
}

/**
    Converts current time in time_t to type::Time represenatation for local
    time zone. Defines time zone (local) used for whole CURDATE function.
*/
void Item_func_curdate_local::store_now_in_TIME(type::Time *now_time)
{
  (void) cached_temporal.from_time_t(current_session->times.getCurrentTimestampEpoch());

  now_time->year= cached_temporal.years();
  now_time->month= cached_temporal.months();
  now_time->day= cached_temporal.days();
  now_time->hour= 0;
  now_time->minute= 0;
  now_time->second= 0;
  now_time->second_part= 0;
}

/**
    Converts current time in time_t to type::Time represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_DATE function.
*/
void Item_func_curdate_utc::store_now_in_TIME(type::Time *now_time)
{
  (void) cached_temporal.from_time_t(current_session->times.getCurrentTimestampEpoch());

  now_time->year= cached_temporal.years();
  now_time->month= cached_temporal.months();
  now_time->day= cached_temporal.days();
  now_time->hour= 0;
  now_time->minute= 0;
  now_time->second= 0;
  now_time->second_part= 0;
}

bool Item_func_curdate::get_temporal(Date &to)
{
  to= cached_temporal;
  return true;
}

} /* namespace drizzled */
