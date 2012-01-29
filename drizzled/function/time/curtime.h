/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2011 Matthew Rheaume
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hopethat it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <drizzled/function/time/time.h>
#include <drizzled/temporal.h>

namespace drizzled
{

class Item_func_curtime :public Item_time
{
protected:
  type::Time ltime;
  Time cached_temporal;
public:
  Item_func_curtime() :Item_time() {}
  void fix_length_and_dec();
  
  bool get_temporal(Time &temporal);
  bool get_time(type::Time &res);
  virtual void store_now_in_TIME(type::Time *now_time)=0;
};

class Item_func_curtime_local :public Item_func_curtime
{
public:
  Item_func_curtime_local() :Item_func_curtime() {}
  const char *func_name() const { return "curtime"; }
  void store_now_in_TIME(type::Time *now_time);
};

class Item_func_curtime_utc :public Item_func_curtime
{
public:
  Item_func_curtime_utc() :Item_func_curtime() {}
  const char *func_name() const { return "utc_time"; }
  void store_now_in_TIME(type::Time *now_time);
};

} /* namespace drizzled */
