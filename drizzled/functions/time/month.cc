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
#include <drizzled/functions/time/month.h>

int64_t Item_func_month::val_int()
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  (void) get_arg0_date(&ltime, TIME_FUZZY_DATE);
  return (int64_t) ltime.month;
}


String* Item_func_monthname::val_str(String* str)
{
  assert(fixed == 1);
  const char *month_name;
  uint32_t   month= (uint) val_int();
  Session *session= current_session;

  if (null_value || !month)
  {
    null_value=1;
    return (String*) 0;
  }
  null_value=0;
  month_name= session->variables.lc_time_names->month_names->type_names[month-1];
  str->set(month_name, strlen(month_name), system_charset_info);
  return str;
}

