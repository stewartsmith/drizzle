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

#ifndef DRIZZLED_FUNCTION_TIME_CURDATE_H
#define DRIZZLED_FUNCTION_TIME_CURDATE_H

#include <drizzled/function/time/date.h>

/* Abstract CURDATE function. See also Item_func_curtime. */

class Item_func_curdate :public Item_date
{
  int64_t value;
  DRIZZLE_TIME ltime;
public:
  Item_func_curdate() :Item_date() {}
  int64_t val_int() { assert(fixed == 1); return (value) ; }
  String *val_str(String *str);
  void fix_length_and_dec();
  bool get_date(DRIZZLE_TIME *res, uint32_t fuzzy_date);
  virtual void store_now_in_TIME(DRIZZLE_TIME *now_time)=0;
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

class Item_func_curdate_local :public Item_func_curdate
{
public:
  Item_func_curdate_local() :Item_func_curdate() {}
  const char *func_name() const { return "curdate"; }
  void store_now_in_TIME(DRIZZLE_TIME *now_time);
};

class Item_func_curdate_utc :public Item_func_curdate
{
public:
  Item_func_curdate_utc() :Item_func_curdate() {}
  const char *func_name() const { return "utc_date"; }
  void store_now_in_TIME(DRIZZLE_TIME *now_time);
};

#endif /* DRIZZLED_FUNCTION_TIME_CURDATE_H */
