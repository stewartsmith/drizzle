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

#include <drizzled/charset.h>
#include <drizzled/item/basic_constant.h>

namespace drizzled {

class Item_null :public Item_basic_constant
{
public:

  Item_null(char *name_par=0)
  {
    maybe_null= null_value= true;
    max_length= 0;
    name= name_par ? name_par : (char*) "NULL";
    fixed= 1;
    collation.set(&my_charset_bin, DERIVATION_IGNORABLE);
  }
  enum Type type() const { return NULL_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  double val_real();
  int64_t val_int();
  String *val_str(String *str);
  type::Decimal *val_decimal(type::Decimal *);
  int save_in_field(Field *field, bool no_conversions);
  int save_safe_in_field(Field *field);
  void send(plugin::Client *client, String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const   { return DRIZZLE_TYPE_NULL; }
  bool basic_const_item() const { return 1; }
  Item *clone_item() { return new Item_null(name); }
  bool is_null() { return true; }

  virtual void print(String *str);

  Item *safe_charset_converter(const charset_info_st * const tocs);
};

class Item_null_result :public Item_null
{
public:
  Field *result_field;
  Item_null_result() : Item_null(), result_field(0) {}
  bool is_result_field() { return result_field != 0; }
  void save_in_result_field(bool no_conversions)
  {
    save_in_field(result_field, no_conversions);
  }
};

} /* namespace drizzled */

