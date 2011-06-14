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

#include <drizzled/item/num.h>

namespace drizzled
{

/* decimal (fixed point) constant */
class Item_decimal :public Item_num
{
protected:
  type::Decimal decimal_value;
public:
  Item_decimal(const char *str_arg, uint32_t length, const charset_info_st * const charset);
  Item_decimal(const char *str, const type::Decimal *val_arg,
               uint32_t decimal_par, uint32_t length);
  Item_decimal(type::Decimal *value_par);
  Item_decimal(int64_t val, bool unsig);
  Item_decimal(double val, int precision, int scale);
  Item_decimal(const unsigned char *bin, int precision, int scale);

  enum Type type() const { return DECIMAL_ITEM; }
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_DECIMAL; }
  int64_t val_int();
  double val_real();
  String *val_str(String*);
  type::Decimal *val_decimal(type::Decimal *)
  { return &decimal_value; }
  int save_in_field(Field *field, bool no_conversions);
  bool basic_const_item() const { return 1; }
  Item *clone_item()
  {
    return new Item_decimal(name, &decimal_value, decimals, max_length);
  }
  virtual void print(String *str);
  Item_num *neg()
  {
    class_decimal_neg(&decimal_value);
    unsigned_flag= !decimal_value.sign();
    return this;
  }
  uint32_t decimal_precision() const { return decimal_value.precision(); }
  bool eq(const Item *, bool binary_cmp) const;
  void set_decimal_value(type::Decimal *value_par);
};

} /* namespace drizzled */

