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
#include <drizzled/util/test.h>

namespace drizzled
{

class Item_int :public Item_num
{
public:
  int64_t value;

  Item_int(int32_t i,uint32_t length= MY_INT32_NUM_DECIMAL_DIGITS) :
    value((int64_t) i)
    { max_length=length; fixed= 1; }

  Item_int(int64_t i,uint32_t length= MY_INT64_NUM_DECIMAL_DIGITS) :
    value(i)
    { max_length=length; fixed= 1; }

  Item_int(uint64_t i, uint32_t length= MY_INT64_NUM_DECIMAL_DIGITS) :
    value((int64_t)i)
  { max_length=length; fixed=1; }

  Item_int(const char *str_arg,int64_t i,uint32_t length) :
    value(i)
    { max_length= length; name= const_cast<char *>(str_arg); fixed= 1; }

  Item_int(const char *str_arg, uint32_t length=64);

  enum Type type() const { return INT_ITEM; }

  enum Item_result result_type () const { return INT_RESULT; }

  enum_field_types field_type() const { return DRIZZLE_TYPE_LONGLONG; }

  int64_t val_int() { assert(fixed == 1); return value; }

  double val_real() { assert(fixed == 1); return (double) value; }

  type::Decimal *val_decimal(type::Decimal *);

  String *val_str(String*);

  int save_in_field(Field *field, bool no_conversions);

  bool basic_const_item() const { return 1; }

  Item *clone_item() { return new Item_int(name,value,max_length); }

  virtual void print(String *str);

  Item_num *neg() { value= -value; return this; }

  uint32_t decimal_precision() const
  { return (uint32_t)(max_length - test(value < 0)); }

  bool eq(const Item *, bool binary_cmp) const;
};

} /* namespace drizzled */

