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
#include <drizzled/temporal.h>

namespace drizzled
{

/* Abstract CURDATE function. See also Item_func_curtime. */
class Item_func_curdate :public Item_date
{
protected:
  type::Time ltime;
  Date cached_temporal;
public:
  Item_func_curdate() :Item_date() {}
  void fix_length_and_dec();
  /**
   * All functions which inherit from Item_date must implement
   * their own get_temporal() method, which takes a supplied
   * Date reference and populates it with a correct
   * date based on the semantics of the function.
   *
   * For CURDATE() and sisters, there is no argument, and we 
   * return a cached Date value that we create during fix_length_and_dec.
   *
   * Always returns true, since a Date can always be constructed
   * from a time_t
   *
   * @param Reference to a Date to populate
   */
  bool get_temporal(Date &temporal);
  virtual void store_now_in_TIME(type::Time *now_time)=0;
};

class Item_func_curdate_local :public Item_func_curdate
{
public:
  Item_func_curdate_local() :Item_func_curdate() {}
  const char *func_name() const { return "curdate"; }
  void store_now_in_TIME(type::Time *now_time);
};

class Item_func_curdate_utc :public Item_func_curdate
{
public:
  Item_func_curdate_utc() :Item_func_curdate() {}
  const char *func_name() const { return "utc_date"; }
  void store_now_in_TIME(type::Time *now_time);
};

} /* namespace drizzled */

