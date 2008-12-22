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

#ifndef DRIZZLED_FUNCTION_TIME_CURTIME_H
#define DRIZZLED_FUNCTION_TIME_CURTIME_H

#include <drizzled/function/time/str_timefunc.h>

/* Abstract CURTIME function. Children should define what time zone is used */

class Item_func_curtime :public Item_str_timefunc
{
  int64_t value;
  char buff[9*2+32];
  uint32_t buff_length;
public:
  Item_func_curtime() :Item_str_timefunc() {}
  Item_func_curtime(Item *a) :Item_str_timefunc(a) {}
  double val_real() { assert(fixed == 1); return (double) value; }
  int64_t val_int() { assert(fixed == 1); return value; }
  String *val_str(String *str);
  void fix_length_and_dec();
  /*
    Abstract method that defines which time zone is used for conversion.
    Converts time current time in time_t representation to broken-down
    DRIZZLE_TIME representation using UTC-SYSTEM or per-thread time zone.
  */
  virtual void store_now_in_TIME(DRIZZLE_TIME *now_time)=0;
  bool result_as_int64_t() { return true; }
  bool check_vcol_func_processor(unsigned char *int_arg  __attribute__((unused)))
  { return true; }
};

class Item_func_curtime_local :public Item_func_curtime
{
public:
  Item_func_curtime_local() :Item_func_curtime() {}
  Item_func_curtime_local(Item *a) :Item_func_curtime(a) {}
  const char *func_name() const { return "curtime"; }
  virtual void store_now_in_TIME(DRIZZLE_TIME *now_time);
};


class Item_func_curtime_utc :public Item_func_curtime
{
public:
  Item_func_curtime_utc() :Item_func_curtime() {}
  Item_func_curtime_utc(Item *a) :Item_func_curtime(a) {}
  const char *func_name() const { return "utc_time"; }
  virtual void store_now_in_TIME(DRIZZLE_TIME *now_time);
};

#endif /* DRIZZLED_FUNCTION_TIME_CURTIME_H */
