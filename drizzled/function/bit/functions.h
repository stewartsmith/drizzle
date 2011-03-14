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

#include <drizzled/function/bit.h>

namespace drizzled
{

namespace function
{

namespace bit
{

class Or :public Bit
{
public:
  Or(Item *a, Item *b) :Bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "|"; }
};

class And :public Bit
{
public:
  And(Item *a, Item *b) :Bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "&"; }
};

class ShiftLeft :public Bit
{
public:
  ShiftLeft(Item *a, Item *b) :Bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "<<"; }
};

class ShiftRight :public Bit
{
public:
  ShiftRight(Item *a, Item *b) :Bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return ">>"; }
};

class Neg :public Bit
{
public:
  Neg(Item *a) :Bit(a) {}
  int64_t val_int();
  const char *func_name() const { return "~"; }

  virtual inline void print(String *str)
  {
    Item_func::print(str);
  }
};

class Xor : public Bit
{
public:
  Xor(Item *a, Item *b) :Bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "^"; }
};


} /* namespace bit */
} /* namespace function */
} /* namespace drizzled */


