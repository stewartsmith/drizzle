/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

#include <drizzled/item/function/boolean.h>
#include <drizzled/execute.h>
#include <iostream>

namespace drizzled
{

namespace utility_functions
{

class Execute :public drizzled::item::function::Boolean
{
  drizzled::String _res;
  drizzled::Execute execute;

public:
  Execute() :
    drizzled::item::function::Boolean(),
    execute(getSession(), false)
  {
    unsigned_flag= true;
  }

  bool val_bool();

  int64_t val_int()
  {
    return val_bool();
  }
  const char *func_name() const { return "execute"; }
  const char *fully_qualified_func_name() const { return "execute()"; }

  void fix_length_and_dec()
  {
    max_length= 1;
  }

  bool check_argument_count(int n)
  {
    if (n == 2)
    {
      execute.setWait();
    }

    return (n == 1 or n == 2);
  }
};

} /* namespace utility_functions */
} /* namespace drizzled */

