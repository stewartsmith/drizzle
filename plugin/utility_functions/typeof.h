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

#include <drizzled/function/str/strfunc.h>

namespace drizzled
{

namespace utility_functions
{

class Typeof :public Item_str_func
{
public:
  Typeof()
  { }

  int64_t val_int();

  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "typeof"; }
  const char *fully_qualified_func_name() const { return "typeof()"; }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};

} /* namespace utility_functions */
} /* namespace drizzled */

