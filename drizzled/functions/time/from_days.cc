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
#include <drizzled/functions/time/from_days.h>

bool Item_func_from_days::get_date(DRIZZLE_TIME *ltime, uint32_t fuzzy_date __attribute__((unused)))
{
  int64_t value=args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 1;
  memset(ltime, 0, sizeof(DRIZZLE_TIME));
  get_date_from_daynr((long) value, &ltime->year, &ltime->month, &ltime->day);
  ltime->time_type= DRIZZLE_TIMESTAMP_DATE;
  return 0;
}

