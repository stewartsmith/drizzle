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

#include <drizzled/item/cache.h>

namespace drizzled {

class Item_cache_int : public Item_cache
{
public:
  Item_cache_int(): Item_cache(), value(0) {}
  Item_cache_int(enum_field_types field_type_arg):
    Item_cache(field_type_arg), value(0) {}

  void store(Item *item);
  void store(Item *item, int64_t val_arg);
  double val_real() { assert(fixed == 1); return (double) value; }
  int64_t val_int() { assert(fixed == 1); return value; }
  String* val_str(String *str);
  type::Decimal *val_decimal(type::Decimal *);
  enum Item_result result_type() const { return INT_RESULT; }
  bool result_as_int64_t() { return true; }
protected:
  int64_t value;
};

} /* namespace drizzled */

