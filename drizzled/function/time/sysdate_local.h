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

#include <drizzled/function/time/now.h>

namespace drizzled
{

/*
  This is like NOW(), but always uses the real current time, not the
  query_start(). This matches the Oracle behavior.
*/
class Item_func_sysdate_local :public Item_func_now
{
public:
  Item_func_sysdate_local() :Item_func_now() {}
  Item_func_sysdate_local(Item *a) :Item_func_now(a) {}
  bool const_item() const { return 0; }
  const char *func_name() const { return "sysdate"; }
  void store_now_in_TIME(type::Time &now_time);
  double val_real();
  int64_t val_int();
  int save_in_field(Field *to, bool no_conversions);
  String *val_str(String *str);
  void fix_length_and_dec();
  bool get_date(type::Time &res, uint32_t fuzzy_date);
  void update_used_tables()
  {
    Item_func_now::update_used_tables();
    used_tables_cache|= RAND_TABLE_BIT;
  }
};

} /* namespace drizzled */

