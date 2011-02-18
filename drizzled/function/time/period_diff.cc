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

#include <drizzled/function/time/period_diff.h>
#include <drizzled/calendar.h>

namespace drizzled
{

int64_t Item_func_period_diff::val_int()
{
  assert(fixed == 1);
  uint32_t period1= (uint32_t)args[0]->val_int();
  uint32_t period2= (uint32_t)args[1]->val_int();

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return (int64_t) (year_month_to_months(period1) -year_month_to_months(period2));
}

} /* namespace drizzled */
