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

#ifndef DRIZZLED_FUNCTIONS_TIME_SEC_TO_TIME_H
#define DRIZZLED_FUNCTIONS_TIME_SEC_TO_TIME_H

class Item_func_sec_to_time :public Item_str_timefunc
{
public:
  Item_func_sec_to_time(Item *item) :Item_str_timefunc(item) {}
  double val_real()
  {
    assert(fixed == 1);
    return (double) Item_func_sec_to_time::val_int();
  }
  int64_t val_int();
  String *val_str(String *);
  void fix_length_and_dec()
  {
    Item_str_timefunc::fix_length_and_dec();
    collation.set(&my_charset_bin);
    maybe_null=1;
  }
  const char *func_name() const { return "sec_to_time"; }
  bool result_as_int64_t() { return true; }
};

#endif /* DRIZZLED_FUNCTIONS_TIME_SEC_TO_TIME_H */
