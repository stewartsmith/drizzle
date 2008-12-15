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
#include <drizzled/function/time/last_day.h>

bool Item_func_last_day::get_date(DRIZZLE_TIME *ltime, uint32_t fuzzy_date)
{
  if (get_arg0_date(ltime, fuzzy_date & ~TIME_FUZZY_DATE) ||
      (ltime->month == 0))
  {
    null_value= 1;
    return 1;
  }
  null_value= 0;
  uint32_t month_idx= ltime->month-1;
  ltime->day= days_in_month[month_idx];
  if ( month_idx == 1 && calc_days_in_year(ltime->year) == 366)
    ltime->day= 29;
  ltime->hour= ltime->minute= ltime->second= 0;
  ltime->second_part= 0;
  ltime->time_type= DRIZZLE_TIMESTAMP_DATE;
  return 0;
}

