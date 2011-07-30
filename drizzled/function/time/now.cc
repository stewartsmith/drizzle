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

#include <drizzled/function/time/now.h>
#include <drizzled/current_session.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/temporal.h>
#include <drizzled/field.h>

namespace drizzled {

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
  
  ltime.reset();

  ltime.time_type= type::DRIZZLE_TIMESTAMP_DATETIME;

  store_now_in_TIME(ltime);

  ltime.convert(value);

  size_t length= type::Time::MAX_STRING_LENGTH;
  ltime.convert(buff, length);

  max_length= buff_length= length;
}

/**
    Converts current time in time_t to type::Time represenatation for local
    time zone. Defines time zone (local) used for whole NOW function.
*/
void Item_func_now_local::store_now_in_TIME(type::Time &now_time)
{
  Session *session= current_session;
  uint32_t fractional_seconds= 0;
  time_t tmp= session->times.getCurrentTimestampEpoch(fractional_seconds);

#if 0
  now_time->store(tmp, fractional_seconds, true);
#endif
  now_time.store(tmp, fractional_seconds);
}


/**
    Converts current time in time_t to type::Time represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_TIMESTAMP function.
*/
void Item_func_now_utc::store_now_in_TIME(type::Time &now_time)
{
  uint32_t fractional_seconds= 0;
  time_t tmp= current_session->times.getCurrentTimestampEpoch(fractional_seconds);
  now_time.store(tmp, fractional_seconds);
}

bool Item_func_now::get_temporal(DateTime &to)
{
  to= cached_temporal;
  return true;
}

bool Item_func_now::get_date(type::Time &res, uint32_t )
{
  res= ltime;
  return 0;
}


int Item_func_now::save_in_field(Field *to, bool )
{
  to->set_notnull();
  return to->store_time(ltime, type::DRIZZLE_TIMESTAMP_DATETIME);
}

} /* namespace drizzled */
