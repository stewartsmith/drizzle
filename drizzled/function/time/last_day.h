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

#pragma once

#include <drizzled/function/time/date.h>

namespace drizzled {

class Item_func_last_day :public Item_date
{
public:
  Item_func_last_day(Item *a) :Item_date(a) {}
  const char *func_name() const { return "last_day"; }
  /**
   * All functions which inherit from Item_date must implement
   * their own get_temporal() method, which takes a supplied
   * Date reference and populates it with a correct
   * date based on the semantics of the function.
   *
   * For LAST_DATE(), we interpret the function's argument
   * as a DateTime string and then figure out the last day of the
   * month of that date, and populate our reference accordingly.
   *
   * Returns whether the function was able to correctly fill
   * the supplied date temporal with a proper date.
   *
   * For a NULL parameter, we return false and set null_value
   * to true.
   *
   * @param Reference to a Date to populate
   */
  bool get_temporal(Date &temporal);
};

} /* namespace drizzled */

