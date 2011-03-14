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
#include <drizzled/function/num1.h>

namespace drizzled
{

class Item_func_neg :public Item_func_num1
{
  bool _is_negative;

public:
  Item_func_neg(Item *a) :
    Item_func_num1(a),
    _is_negative(true)
  {}

  double real_op();
  int64_t int_op();
  type::Decimal *decimal_op(type::Decimal *);
  const char *func_name() const { return "-"; }
  enum Functype functype() const   { return NEG_FUNC; }
  void fix_length_and_dec();
  void fix_num_length_and_dec();
  uint32_t decimal_precision() const { return args[0]->decimal_precision(); }

  bool negative() const
  {
    return _is_negative;
  }
};

} /* namespace drizzled */

