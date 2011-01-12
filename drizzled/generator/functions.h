/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef DRIZZLED_GENERATOR_FUNCTIONS_H
#define DRIZZLED_GENERATOR_FUNCTIONS_H

#include "drizzled/plugin/function.h"

namespace drizzled {
namespace generator {

class Functions
{
  Session &session;
  typedef std::vector <std::string> vector;
  std::string function_name;
  vector function_list;
  vector::iterator iter;

public:

  Functions(Session &arg);

  operator std::string*()
  {
    if (iter == function_list.end())
      return NULL;

    function_name= *iter;
    iter++;

    return &function_name;
  }
};

} /* namespace generator */
} /* namespace drizzled */

#endif /* DRIZZLED_GENERATOR_FUNCTIONS_H */
