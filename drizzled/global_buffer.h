/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Andrew Hutchings
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

#include <drizzled/atomics.h>

namespace drizzled
{
template <class T>
class global_buffer_constraint
{
public:
  global_buffer_constraint(T max)
  {
    setMaxSize(max); 
  }

  T getMaxSize() const { return max_size; }
  void setMaxSize(T new_size) 
  {
    if (new_size == 0) new_size = std::numeric_limits<T>::max(); 
    max_size= new_size;
  }

  bool add(T addition)
  {
    if (current_size.add_and_fetch(addition) > max_size)
    {
      current_size.add_and_fetch(T(0) - addition);
      return false;
    }
    return true;
  }

  bool sub(T subtract)
  {
    if (current_size < subtract)
      return false;
    else
      current_size.add_and_fetch(T(0) - subtract);

    return true;
  }

private:
  atomic<T> current_size;
  T max_size;
};

}
