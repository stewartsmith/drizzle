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

#ifndef DRIZZLED_FUNCTION_TIME_TIMEDIFF_H
#define DRIZZLED_FUNCTION_TIME_TIMEDIFF_H

#include <drizzled/function/time/str_timefunc.h>

class Item_func_timediff :public Item_str_timefunc
{
public:
  Item_func_timediff(Item *a, Item *b)
    :Item_str_timefunc(a, b) {}
  String *val_str(String *str);
  const char *func_name() const { return "timediff"; }
  void fix_length_and_dec()
  {
    Item_str_timefunc::fix_length_and_dec();
    maybe_null= 1;
  }
};

#endif /* DRIZZLED_FUNCTION_TIME_TIMEDIFF_H */
