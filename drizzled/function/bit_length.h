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

#ifndef DRIZZLED_FUNCTION_BIT_LENGTH_H
#define DRIZZLED_FUNCTION_BIT_LENGTH_H

#include <drizzled/function/func.h>
#include <drizzled/function/length.h>

class Item_func_bit_length :public Item_func_length
{
public:
  Item_func_bit_length(Item *a) :Item_func_length(a) {}
  int64_t val_int()
    { assert(fixed == 1); return Item_func_length::val_int()*8; }
  const char *func_name() const { return "bit_length"; }
};

#endif /* DRIZZLED_FUNCTION_BIT_LENGTH_H */
