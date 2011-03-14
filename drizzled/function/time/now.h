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

/* Abstract CURRENT_TIMESTAMP function. See also Item_func_curtime */

class Item_func_now :public Item_date_func
{
protected:
  int64_t value;
  char buff[type::Time::MAX_STRING_LENGTH];
  uint32_t buff_length;
  type::Time ltime;
  DateTime cached_temporal;

public:
  Item_func_now() :Item_date_func() {}
  Item_func_now(Item *a) :Item_date_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  int64_t val_int() { assert(fixed == 1); return value; }
  int save_in_field(Field *to, bool no_conversions);
  String *val_str(String *str);
  void fix_length_and_dec();
  /**
   * For NOW() and sisters, there is no argument, and we 
   * return a cached Date value that we create during fix_length_and_dec.
   *
   * Always returns true, since a DateTime can always be constructed
   * from a time_t
   *
   * @param Reference to a DateTime to populate
   */
  bool get_temporal(DateTime &temporal);
  bool get_date(type::Time &res, uint32_t fuzzy_date);
  virtual void store_now_in_TIME(type::Time &now_time)=0;
};

class Item_func_now_local :public Item_func_now
{
public:
  Item_func_now_local() :Item_func_now() {}
  Item_func_now_local(Item *a) :Item_func_now(a) {}
  const char *func_name() const { return "now"; }
  virtual void store_now_in_TIME(type::Time &now_time);
  virtual enum Functype functype() const { return NOW_FUNC; }
};


class Item_func_now_utc :public Item_func_now
{
public:
  Item_func_now_utc() :Item_func_now() {}
  Item_func_now_utc(Item *a) :Item_func_now(a) {}
  const char *func_name() const { return "utc_timestamp"; }
  virtual void store_now_in_TIME(type::Time &now_time);
};

} /* namespace drizzled */

