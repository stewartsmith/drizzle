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

class Item_func_numhybrid: public Item_func
{
protected:
  Item_result hybrid_type;
public:
  Item_func_numhybrid(): Item_func(), hybrid_type(REAL_RESULT)
  {}
  Item_func_numhybrid(Item *a) :Item_func(a), hybrid_type(REAL_RESULT)
  {}
  Item_func_numhybrid(Item *a,Item *b)
    :Item_func(a,b), hybrid_type(REAL_RESULT)
  {}
  Item_func_numhybrid(List<Item> &list)
    :Item_func(list), hybrid_type(REAL_RESULT)
  {}

  enum Item_result result_type () const { return hybrid_type; }
  void fix_length_and_dec();
  void fix_num_length_and_dec();
  virtual void find_num_type()= 0; /* To be called from fix_length_and_dec */

  double val_real();
  int64_t val_int();
  type::Decimal *val_decimal(type::Decimal *);
  String *val_str(String*str);

  /**
     @brief Performs the operation that this functions implements when the
     result type is INT.

     @return The result of the operation.
  */
  virtual int64_t int_op()= 0;

  /**
     @brief Performs the operation that this functions implements when the
     result type is REAL.

     @return The result of the operation.
  */
  virtual double real_op()= 0;

  /**
     @brief Performs the operation that this functions implements when the
     result type is DECIMAL.

     @param A pointer where the DECIMAL value will be allocated.
     @return
       - 0 If the result is NULL
       - The same pointer it was given, with the area initialized to the
         result of the operation.
  */
  virtual type::Decimal *decimal_op(type::Decimal *)= 0;

  /**
     @brief Performs the operation that this functions implements when the
     result type is a string type.

     @return The result of the operation.
  */
  virtual String *str_op(String *)= 0;
  bool is_null() { update_null_value(); return null_value; }
};

} /* namespace drizzled */

