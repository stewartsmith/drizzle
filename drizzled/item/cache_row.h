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

class Item_cache_row: public Item_cache
{
  Item_cache  **values;
  uint32_t item_count;
  bool save_array;
public:

  Item_cache_row()
  :Item_cache(), values(0), item_count(2), save_array(0) {}

  /*
    'allocate' used only in row transformer, to preallocate space for row
    cache.
  */
  void allocate(uint32_t num);
  /*
    'setup' is needed only by row => it not called by simple row subselect
    (only by IN subselect (in subselect optimizer))
  */
  bool setup(Item *item);
  void store(Item *item);
  void illegal_method_call(const char * method_name);
  void make_field(SendField *field);
  double val_real();
  int64_t val_int();
  String *val_str(String *val);
  type::Decimal *val_decimal(type::Decimal *val);

  enum Item_result result_type() const;

  uint32_t cols();
  Item *element_index(uint32_t i);
  Item **addr(uint32_t i);
  bool check_cols(uint32_t c);
  bool null_inside();
  void bring_value();
  void keep_array();
  void cleanup();

};

} /* namespace drizzled */

