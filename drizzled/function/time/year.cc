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
#include <drizzled/function/time/year.h>

int64_t Item_func_year::val_int()
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  (void) get_arg0_date(&ltime, TIME_FUZZY_DATE);
  return (int64_t) ltime.year;
}

/* information about this Item tree monotonicity

  SYNOPSIS
    Item_func_year::get_monotonicity_info()

  DESCRIPTION
  Get information about monotonicity of the function represented by this item
  tree.

  RETURN
    See enum_monotonicity_info.
*/

enum_monotonicity_info Item_func_year::get_monotonicity_info() const
{
  if (args[0]->type() == Item::FIELD_ITEM &&
      (args[0]->field_type() == DRIZZLE_TYPE_DATE ||
       args[0]->field_type() == DRIZZLE_TYPE_DATETIME))
    return MONOTONIC_INCREASING;
  return NON_MONOTONIC;
}

int64_t Item_func_year::val_int_endpoint(bool left_endp, bool *incl_endp)
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  if (get_arg0_date(&ltime, TIME_FUZZY_DATE))
  {
    /* got NULL, leave the incl_endp intact */
    return INT64_MIN;
  }

  /*
    Handle the special but practically useful case of datetime values that
    point to year bound ("strictly less" comparison stays intact) :

      col < '2007-01-01 00:00:00'  -> YEAR(col) <  2007

    which is different from the general case ("strictly less" changes to
    "less or equal"):

      col < '2007-09-15 23:00:00'  -> YEAR(col) <= 2007
  */
  if (!left_endp && ltime.day == 1 && ltime.month == 1 &&
      !(ltime.hour || ltime.minute || ltime.second || ltime.second_part))
    ; /* do nothing */
  else
    *incl_endp= true;
  return ltime.year;
}

