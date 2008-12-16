/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_FUNCTION_BIT_H
#define DRIZZLED_FUNCTION_BIT_H

#include <drizzled/function/func.h>
#include <drizzled/function/int.h>

class Item_func_bit: public Item_int_func
{
public:
  Item_func_bit(Item *a, Item *b) :Item_int_func(a, b) {}
  Item_func_bit(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec() { unsigned_flag= 1; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }
};

class Item_func_bit_xor : public Item_func_bit
{
public:
  Item_func_bit_xor(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "^"; }
};

class Item_func_bit_or :public Item_func_bit
{
public:
  Item_func_bit_or(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "|"; }
};

class Item_func_bit_and :public Item_func_bit
{
public:
  Item_func_bit_and(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "&"; }
};

class Item_func_shift_left :public Item_func_bit
{
public:
  Item_func_shift_left(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "<<"; }
};

class Item_func_shift_right :public Item_func_bit
{
public:
  Item_func_shift_right(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return ">>"; }
};

class Item_func_bit_neg :public Item_func_bit
{
public:
  Item_func_bit_neg(Item *a) :Item_func_bit(a) {}
  int64_t val_int();
  const char *func_name() const { return "~"; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }
};


#endif /* DRIZZLED_FUNCTION_BIT_H */
