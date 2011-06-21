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

#include <drizzled/item/basic_constant.h>
#include <drizzled/item/field.h>
#include <drizzled/item/ident.h>
#include <drizzled/type/decimal.h>
#include <drizzled/util/test.h>

namespace drizzled
{

class Item_cache: public Item_basic_constant
{
protected:
  Item *example;
  table_map used_table_map;
  /*
    Field that this object will get value from. This is set/used by
    index-based subquery engines to detect and remove the equality injected
    by IN->EXISTS transformation.
    For all other uses of Item_cache, cached_field doesn't matter.
  */
  Field *cached_field;
  enum enum_field_types cached_field_type;
public:
  Item_cache():
    example(0), used_table_map(0), cached_field(0), cached_field_type(DRIZZLE_TYPE_VARCHAR)
  {
    fixed= 1;
    null_value= 1;
  }
  Item_cache(enum_field_types field_type_arg):
    example(0), used_table_map(0), cached_field(0), cached_field_type(field_type_arg)
  {
    fixed= 1;
    null_value= 1;
  }

  void set_used_tables(table_map map) { used_table_map= map; }

  virtual void allocate(uint32_t) {};
  virtual bool setup(Item *item)
  {
    example= item;
    max_length= item->max_length;
    decimals= item->decimals;
    collation.set(item->collation);
    unsigned_flag= item->unsigned_flag;
    if (item->type() == FIELD_ITEM)
      cached_field= ((Item_field *)item)->field;
    return 0;
  };
  virtual void store(Item *)= 0;
  enum Type type() const { return CACHE_ITEM; }
  enum_field_types field_type() const { return cached_field_type; }
  static Item_cache* get_cache(const Item *item);
  table_map used_tables() const { return used_table_map; }
  virtual void keep_array() {}
  virtual void print(String *str);
  bool eq_def(Field *field);
  bool eq(const Item *item, bool) const
  {
    return this == item;
  }
  bool basic_const_item() const
  {
    return test(example && example->basic_const_item());
  }
};

} /* namespace drizzled */

