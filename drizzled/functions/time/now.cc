/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/functions/time/now.h>
#include <drizzled/tztime.h>

String *Item_func_now::val_str(String *str __attribute__((unused)))
{
  assert(fixed == 1);
  str_value.set(buff,buff_length, &my_charset_bin);
  return &str_value;
}


void Item_func_now::fix_length_and_dec()
{
  decimals= DATETIME_DEC;
  collation.set(&my_charset_bin);

  store_now_in_TIME(&ltime);
  value= (int64_t) TIME_to_uint64_t_datetime(&ltime);

  buff_length= (uint) my_datetime_to_str(&ltime, buff);
  max_length= buff_length;
}

/**
    Converts current time in my_time_t to DRIZZLE_TIME represenatation for local
    time zone. Defines time zone (local) used for whole NOW function.
*/
void Item_func_now_local::store_now_in_TIME(DRIZZLE_TIME *now_time)
{
  Session *session= current_session;
  session->variables.time_zone->gmt_sec_to_TIME(now_time,
                                             (my_time_t)session->query_start());
  session->time_zone_used= 1;
}


/**
    Converts current time in my_time_t to DRIZZLE_TIME represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_TIMESTAMP function.
*/
void Item_func_now_utc::store_now_in_TIME(DRIZZLE_TIME *now_time)
{
  my_tz_UTC->gmt_sec_to_TIME(now_time,
                             (my_time_t)(current_session->query_start()));
  /*
    We are not flagging this query as using time zone, since it uses fixed
    UTC-SYSTEM time-zone.
  */
}

bool Item_func_now::get_date(DRIZZLE_TIME *res,
                             uint32_t fuzzy_date __attribute__((unused)))
{
  *res= ltime;
  return 0;
}


int Item_func_now::save_in_field(Field *to, bool no_conversions __attribute__((unused)))
{
  to->set_notnull();
  return to->store_time(&ltime, DRIZZLE_TIMESTAMP_DATETIME);
}

