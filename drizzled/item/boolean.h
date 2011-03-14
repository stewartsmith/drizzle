/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

namespace item
{

class Boolean: public Item_basic_constant
{
  bool value;

public:
  Boolean(const char *str_arg, bool arg) :
    Item_basic_constant(),
    value(arg)
  {
    max_length= value ? 4 : 5;
    fixed= true;
    name= (const_cast<char *>(str_arg));
  }

  Boolean(bool arg) :
    value(arg)
  {
    max_length= value ? 4 : 5;
    fixed= true;

    if (value)
    {
      name= const_cast<char *>("TRUE");
    }
    else
    {
      name= const_cast<char *>("FALSE");
    }
  }

  enum Type type() const { return BOOLEAN_ITEM; }

  virtual bool val_bool()
  {
    return value;
  }

  double val_real()
  {
    return value ? 1 : 0;
  }

  int64_t val_int() 
  {
    return value ? 1 : 0;
  }

  drizzled::String* val_str(drizzled::String *value_buffer)
  {
    value_buffer->realloc(5);

    if (value)
    {
      value_buffer->append("TRUE");
    }
    else
    {
      value_buffer->append("FALSE");
    }

    return value_buffer;
  }

  type::Decimal* val_decimal(type::Decimal *dec)
  {
    (void)dec;
    return 0;
  }

};

} /* namespace item */
} /* namespace drizzled */

#include <drizzled/item/true.h>
#include <drizzled/item/false.h>

