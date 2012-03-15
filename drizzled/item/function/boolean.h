/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <drizzled/function/math/int.h>

namespace drizzled
{

namespace item
{

namespace function
{

class Boolean :public Item_int_func
{
public:
  Boolean() :
    Item_int_func()
  {}

  Boolean(Item *a) :
    Item_int_func(a)
  {}

  Boolean(Item *a,Item *b) :
    Item_int_func(a,b)
  {}

  Boolean(Session *session, Boolean *item) :
    Item_int_func(session, item)
  {}

  bool is_bool_func()
  {
    return true;
  }

  void fix_length_and_dec()
  {
    decimals= 0;
    max_length= 1;
  }

  uint32_t decimal_precision() const
  {
    return 1;
  }
};

} /* namespace function */
} /* namespace item */
} /* namespace drizzled */

