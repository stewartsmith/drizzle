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

#ifndef DRIZZLED_DYNAMIC_ARRAY_H
#define DRIZZLED_DYNAMIC_ARRAY_H

#include <stddef.h>

namespace drizzled
{

typedef struct st_dynamic_array
{
  unsigned char *buffer;
  size_t elements;
  size_t max_element;
  uint32_t alloc_increment;
  uint32_t size_of_element;

  template<class T>
  T& operator[](size_t i)
  {
    return reinterpret_cast<T*>(buffer)[i];
  }

  size_t size() const
  {
    return elements;
  }
} DYNAMIC_ARRAY;

#define my_init_dynamic_array(A,B,C,D) init_dynamic_array2(A,B,NULL,C,D)
#define my_init_dynamic_array_ci(A,B,C,D) init_dynamic_array2(A,B,NULL,C,D)

extern bool init_dynamic_array2(DYNAMIC_ARRAY *array,uint32_t element_size,
                                   void *init_buffer, uint32_t init_alloc,
                                   uint32_t alloc_increment);
/* init_dynamic_array() function is deprecated */
extern bool init_dynamic_array(DYNAMIC_ARRAY *array,uint32_t element_size,
                                  uint32_t init_alloc,uint32_t alloc_increment);
extern bool insert_dynamic(DYNAMIC_ARRAY *array,unsigned char * element);
extern unsigned char *alloc_dynamic(DYNAMIC_ARRAY *array);
extern unsigned char *pop_dynamic(DYNAMIC_ARRAY*);
extern bool set_dynamic(DYNAMIC_ARRAY *array,unsigned char * element,uint32_t array_index);
extern void get_dynamic(DYNAMIC_ARRAY *array,unsigned char * element,uint32_t array_index);
extern void delete_dynamic(DYNAMIC_ARRAY *array);
extern void delete_dynamic_element(DYNAMIC_ARRAY *array, uint32_t array_index);
extern void freeze_size(DYNAMIC_ARRAY *array);
extern int  get_index_dynamic(DYNAMIC_ARRAY *array, unsigned char * element);
#define dynamic_array_ptr(array,array_index) ((array)->buffer+(array_index)*(array)->size_of_element)
#define dynamic_element(array,array_index,type) ((type)((array)->buffer) +(array_index))
#define push_dynamic(A,B) insert_dynamic((A),(B))
#define reset_dynamic(array) ((array)->elements= 0)
#define sort_dynamic(A,cmp) my_qsort((A)->buffer, (A)->elements, (A)->size_of_element, (cmp))

} /* namespace drizzled */

#endif /* DRIZZLED_DYNAMIC_ARRAY_H */
