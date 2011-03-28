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

namespace drizzled
{

class Item_hex_string: public Item_basic_constant
{
public:
  Item_hex_string() {}
  Item_hex_string(const char *str,uint32_t str_length);
  enum Type type() const { return VARBIN_ITEM; }
  double val_real()
  {
    assert(fixed == 1);
    return (double) (uint64_t) Item_hex_string::val_int();
  }
  int64_t val_int();
  bool basic_const_item() const { return 1; }
  String *val_str(String*) { assert(fixed == 1); return &str_value; }
  type::Decimal *val_decimal(type::Decimal *);
  int save_in_field(Field *field, bool no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum Item_result cast_to_int_type() const { return INT_RESULT; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_VARCHAR; }
  virtual void print(String *str);
  bool eq(const Item *item, bool binary_cmp) const;
  virtual Item *safe_charset_converter(const charset_info_st * const tocs);
};

} /* namespace drizzled */

