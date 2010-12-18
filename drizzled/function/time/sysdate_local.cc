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

#include <drizzled/function/time/sysdate_local.h>
#include <drizzled/tztime.h>
#include <drizzled/session.h>

namespace drizzled
{

/**
    Converts current time in time_t to DRIZZLE_TIME represenatation for local
    time zone. Defines time zone (local) used for whole SYSDATE function.
*/
void Item_func_sysdate_local::store_now_in_TIME(DRIZZLE_TIME *now_time)
{
  Session *session= current_session;
  session->variables.time_zone->gmt_sec_to_TIME(now_time, time(NULL));
}


String *Item_func_sysdate_local::val_str(String *)
{
  assert(fixed == 1);
  store_now_in_TIME(&ltime);
  buff_length= (uint) my_datetime_to_str(&ltime, buff);
  str_value.set(buff, buff_length, &my_charset_bin);
  return &str_value;
}


int64_t Item_func_sysdate_local::val_int()
{
  assert(fixed == 1);
  store_now_in_TIME(&ltime);
  return (int64_t) TIME_to_uint64_t_datetime(&ltime);
}

double Item_func_sysdate_local::val_real()
{
  assert(fixed == 1);
  store_now_in_TIME(&ltime);
  return uint64_t2double(TIME_to_uint64_t_datetime(&ltime));
}


void Item_func_sysdate_local::fix_length_and_dec()
{
  decimals= 0;
  collation.set(&my_charset_bin);
  max_length= DateTime::MAX_STRING_LENGTH*MY_CHARSET_BIN_MB_MAXLEN;
}


bool Item_func_sysdate_local::get_date(DRIZZLE_TIME *res,
                                       uint32_t )
{
  store_now_in_TIME(&ltime);
  *res= ltime;
  return 0;
}


int Item_func_sysdate_local::save_in_field(Field *to, bool )
{
  store_now_in_TIME(&ltime);
  to->set_notnull();
  to->store_time(&ltime, DRIZZLE_TIMESTAMP_DATETIME);
  return 0;
}

} /* namespace drizzled */
