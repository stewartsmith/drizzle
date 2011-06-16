/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
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

#include <cstddef>

namespace drizzled {

class DYNAMIC_ARRAY
{
public:
  unsigned char *buffer;
  size_t max_element;
  uint32_t alloc_increment;
  uint32_t size_of_element;

  void push_back(void*);

  size_t size() const
  {
    return elements;
  }

  void set_size(size_t v)
  {
    elements = v;
  }
private:
  size_t elements;
};

#define my_init_dynamic_array(A,B,C,D) init_dynamic_array2(A,B,NULL,C,D)
#define my_init_dynamic_array_ci(A,B,C,D) init_dynamic_array2(A,B,NULL,C,D)

void init_dynamic_array2(DYNAMIC_ARRAY*, uint32_t element_size, void *init_buffer, uint32_t init_alloc, uint32_t alloc_increment);
unsigned char *alloc_dynamic(DYNAMIC_ARRAY *array);
unsigned char *pop_dynamic(DYNAMIC_ARRAY*);
void delete_dynamic(DYNAMIC_ARRAY *array);

} /* namespace drizzled */

