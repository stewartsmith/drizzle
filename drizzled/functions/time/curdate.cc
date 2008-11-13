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
#include <drizzled/functions/time/curdate.h>
#include <drizzled/tztime.h>

void Item_func_curdate::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  decimals=0;
  max_length=MAX_DATE_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;

  store_now_in_TIME(&ltime);

  /* We don't need to set second_part and neg because they already 0 */
  ltime.hour= ltime.minute= ltime.second= 0;
  ltime.time_type= DRIZZLE_TIMESTAMP_DATE;
  value= (int64_t) TIME_to_uint64_t_date(&ltime);
}

String *Item_func_curdate::val_str(String *str)
{
  assert(fixed == 1);
  if (str->alloc(MAX_DATE_STRING_REP_LENGTH))
  {
    null_value= 1;
    return (String *) 0;
  }
  make_date((DATE_TIME_FORMAT *) 0, &ltime, str);
  return str;
}

/**
    Converts current time in my_time_t to DRIZZLE_TIME represenatation for local
    time zone. Defines time zone (local) used for whole CURDATE function.
*/
void Item_func_curdate_local::store_now_in_TIME(DRIZZLE_TIME *now_time)
{
  Session *session= current_session;
  session->variables.time_zone->gmt_sec_to_TIME(now_time,
                                             (my_time_t)session->query_start());
  session->time_zone_used= 1;
}

/**
    Converts current time in my_time_t to DRIZZLE_TIME represenatation for UTC
    time zone. Defines time zone (UTC) used for whole UTC_DATE function.
*/
void Item_func_curdate_utc::store_now_in_TIME(DRIZZLE_TIME *now_time)
{
  my_tz_UTC->gmt_sec_to_TIME(now_time,
                             (my_time_t)(current_session->query_start()));
  /*
    We are not flagging this query as using time zone, since it uses fixed
    UTC-SYSTEM time-zone.
  */
}

bool Item_func_curdate::get_date(DRIZZLE_TIME *res,
                                 uint32_t fuzzy_date __attribute__((unused)))
{
  *res=ltime;
  return 0;
}


