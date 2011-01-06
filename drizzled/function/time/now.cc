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

#include "config.h"

#include <drizzled/function/time/now.h>
#include <drizzled/session.h>

#include "drizzled/temporal.h"

namespace drizzled
{

String *Item_func_now::val_str(String *)
{
  assert(fixed == 1);
  str_value.set(buff, buff_length, &my_charset_bin);
  return &str_value;
}


void Item_func_now::fix_length_and_dec()
{
  decimals= DATETIME_DEC;
  collation.set(&my_charset_bin);
  
  memset(&ltime, 0, sizeof(type::Time));

  ltime.time_type= DRIZZLE_TIMESTAMP_DATETIME;

  store_now_in_TIME(&ltime);
  value= (int64_t) TIME_to_uint64_t_datetime(&ltime);

  buff_length= (uint) my_datetime_to_str(&ltime, buff);
  max_length= buff_length;
}

/**
    Converts current time in time_t to type::Time represenatation for local
    time zone. Defines time zone (local) used for whole NOW function.
*/
void Item_func_now_local::store_now_in_TIME(type::Time *now_time)
{
  Session *session= current_session;
  time_t tmp= session->getCurrentTimestampEpoch();

  (void) cached_temporal.from_time_t(tmp);

  now_time->year= cached_temporal.years();
  now_time->month= cached_temporal.months();
  now_time->day= cached_temporal.days();
  now_time->hour= cached_temporal.hours();
  now_time->minute= cached_temporal.minutes();
  now_time->second= cached_temporal.seconds();
}


/**
    Converts current time in time_t to type::Time represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_TIMESTAMP function.
*/
void Item_func_now_utc::store_now_in_TIME(type::Time *now_time)
{
  Session *session= current_session;
  time_t tmp= session->getCurrentTimestampEpoch();

  (void) cached_temporal.from_time_t(tmp);

  now_time->year= cached_temporal.years();
  now_time->month= cached_temporal.months();
  now_time->day= cached_temporal.days();
  now_time->hour= cached_temporal.hours();
  now_time->minute= cached_temporal.minutes();
  now_time->second= cached_temporal.seconds();
}

bool Item_func_now::get_temporal(DateTime &to)
{
  to= cached_temporal;
  return true;
}

bool Item_func_now::get_date(type::Time *res,
                             uint32_t )
{
  *res= ltime;
  return 0;
}


int Item_func_now::save_in_field(Field *to, bool )
{
  to->set_notnull();
  return to->store_time(&ltime, DRIZZLE_TIMESTAMP_DATETIME);
}

} /* namespace drizzled */
