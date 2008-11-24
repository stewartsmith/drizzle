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
#include <drizzled/functions/time/yearweek.h>
#include <drizzled/functions/time/week_mode.h>

int64_t Item_func_yearweek::val_int()
{
  assert(fixed == 1);
  uint32_t year,week;
  DRIZZLE_TIME ltime;
  if (get_arg0_date(&ltime, TIME_NO_ZERO_DATE))
    return 0;
  week= calc_week(&ltime,
                  (week_mode((uint) args[1]->val_int()) | WEEK_YEAR),
                  &year);
  return week+year*100;
}

