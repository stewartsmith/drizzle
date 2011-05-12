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

#include <drizzled/function/func.h>

namespace drizzled
{

class Item_func_min_max :public Item_func
{
  Item_result cmp_type;
  String tmp_value;
  int cmp_sign;
  /* TRUE <=> arguments should be compared in the DATETIME context. */
  bool compare_as_dates;
  /* An item used for issuing warnings while string to DATETIME conversion. */
  Item *datetime_item;
protected:
  enum_field_types cached_field_type;
public:
  Item_func_min_max(List<Item> &list,int cmp_sign_arg) :Item_func(list),
    cmp_type(INT_RESULT), cmp_sign(cmp_sign_arg), compare_as_dates(false),
    datetime_item(0) {}
  double val_real();
  int64_t val_int();
  String *val_str(String *);
  type::Decimal *val_decimal(type::Decimal *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cmp_type; }
  bool result_as_int64_t() { return compare_as_dates; };
  uint32_t cmp_datetimes(uint64_t *value);
  enum_field_types field_type() const { return cached_field_type; }
};

class Item_func_min :public Item_func_min_max
{
public:
  Item_func_min(List<Item> &list) :Item_func_min_max(list,1) {}
  const char *func_name() const { return "least"; }
};

class Item_func_max :public Item_func_min_max
{
public:
  Item_func_max(List<Item> &list) :Item_func_min_max(list,-1) {}
  const char *func_name() const { return "greatest"; }
};

} /* namespace drizzled */

