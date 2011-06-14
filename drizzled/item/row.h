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

#include <drizzled/item.h>

namespace drizzled
{

class Item_row: public Item
{
  Item **items;
  table_map used_tables_cache;
  uint32_t arg_count;
  bool const_item_cache;
  bool with_null;
public:

  using Item::split_sum_func;

  Item_row(List<Item> &);
  Item_row(Item_row *item):
    Item(),
    items(item->items),
    used_tables_cache(item->used_tables_cache),
    arg_count(item->arg_count),
    const_item_cache(item->const_item_cache),
    with_null(0)
  {}

  enum Type type() const { return ROW_ITEM; };
  void illegal_method_call(const char *);
  bool is_null() { return null_value; }
  void make_field(SendField *)
  {
    illegal_method_call((const char*)"make_field");
  };
  double val_real()
  {
    illegal_method_call((const char*)"val");
    return 0;
  };
  int64_t val_int()
  {
    illegal_method_call((const char*)"val_int");
    return 0;
  };
  String *val_str(String *)
  {
    illegal_method_call((const char*)"val_str");
    return 0;
  };
  type::Decimal *val_decimal(type::Decimal *)
  {
    illegal_method_call((const char*)"val_decimal");
    return 0;
  };
  bool fix_fields(Session *session, Item **ref);
  void fix_after_pullout(Select_Lex *new_parent, Item **ref);
  void cleanup();
  void split_sum_func(Session *session, Item **ref_pointer_array, List<Item> &fields);
  table_map used_tables() const { return used_tables_cache; };
  bool const_item() const { return const_item_cache; };
  enum Item_result result_type() const { return ROW_RESULT; }
  void update_used_tables();
  virtual void print(String *str);

  bool walk(Item_processor processor, bool walk_subquery, unsigned char *arg);
  Item *transform(Item_transformer transformer, unsigned char *arg);

  uint32_t cols() { return arg_count; }
  Item* element_index(uint32_t i) { return items[i]; }
  Item** addr(uint32_t i) { return items + i; }
  bool check_cols(uint32_t c);
  bool null_inside() { return with_null; };
  void bring_value();
};

} /* namespace drizzled */

