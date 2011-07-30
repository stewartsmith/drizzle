/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* Handling of arrays that can grow dynamicly. */

#include <config.h>
#include <algorithm>
#include <drizzled/dynamic_array.h>
#include <drizzled/internal/my_sys.h>

using namespace std;

namespace drizzled {

/*
  Initiate dynamic array

  SYNOPSIS
    init_dynamic_array2()
      array		Pointer to an array
      element_size	Size of element
      init_buffer       Initial buffer pointer
      init_alloc	Number of initial elements
      alloc_increment	Increment for adding new elements

  DESCRIPTION
    init_dynamic_array() initiates array and allocate space for
    init_alloc eilements.
    Array is usable even if space allocation failed.
    Static buffers must begin immediately after the array structure.

  RETURN VALUE
    true	malloc() failed
    false	Ok
*/

void init_dynamic_array2(DYNAMIC_ARRAY *array, uint32_t element_size,
                            void *init_buffer, uint32_t init_alloc,
                            uint32_t alloc_increment)
{
  if (!alloc_increment)
  {
    alloc_increment=max((8192-MALLOC_OVERHEAD)/element_size,16U);
    if (init_alloc > 8 && alloc_increment > init_alloc * 2)
      alloc_increment=init_alloc*2;
  }

  if (!init_alloc)
  {
    init_alloc=alloc_increment;
    init_buffer= 0;
  }
  array->set_size(0);
  array->max_element=init_alloc;
  array->alloc_increment=alloc_increment;
  array->size_of_element=element_size;
  if ((array->buffer= (unsigned char*) init_buffer))
    return;
  array->buffer= (unsigned char*) malloc(element_size*init_alloc);
}

/*
  Insert element at the end of array. Allocate memory if needed.

  SYNOPSIS
    insert_dynamic()
      array
      element

  RETURN VALUE
    true	Insert failed
    false	Ok
*/

static void insert_dynamic(DYNAMIC_ARRAY *array, void* element)
{
  unsigned char* buffer;
  if (array->size() == array->max_element)
    buffer= alloc_dynamic(array);
  else
  {
    buffer= array->buffer+(array->size() * array->size_of_element);
    array->set_size(array->size() + 1);
  }
  memcpy(buffer,element, array->size_of_element);
}

void DYNAMIC_ARRAY::push_back(void* v)
{
  insert_dynamic(this, v);
}


/*
  Alloc space for next element(s)

  SYNOPSIS
    alloc_dynamic()
      array

  DESCRIPTION
    alloc_dynamic() checks if there is empty space for at least
    one element if not tries to allocate space for alloc_increment
    elements at the end of array.

  RETURN VALUE
    pointer	Pointer to empty space for element
    0		Error
*/

unsigned char *alloc_dynamic(DYNAMIC_ARRAY *array)
{
  if (array->size() == array->max_element)
  {
    char *new_ptr;
    if (array->buffer == (unsigned char *)(array + 1))
    {
      /*
        In this senerio, the buffer is statically preallocated,
        so we have to create an all-new malloc since we overflowed
      */
      new_ptr= (char*) malloc((array->max_element + array->alloc_increment) * array->size_of_element);
      memcpy(new_ptr, array->buffer, array->size() * array->size_of_element);
    }
    else 
      new_ptr= (char*) realloc(array->buffer, (array->max_element + array->alloc_increment) * array->size_of_element);
    array->buffer= (unsigned char*) new_ptr;
    array->max_element+=array->alloc_increment;
  }
  array->set_size(array->size() + 1);
  return array->buffer + ((array->size() - 1) * array->size_of_element);
}

/*
  Empty array by freeing all memory

  SYNOPSIS
    delete_dynamic()
      array	Array to be deleted
*/

void delete_dynamic(DYNAMIC_ARRAY *array)
{
  /*
    Just mark as empty if we are using a static buffer
  */
  if (array->buffer == (unsigned char *)(array + 1))
    array->set_size(0);
  else
  if (array->buffer)
  {
    free(array->buffer);
    array->buffer=0;
    array->set_size(array->max_element=0);
  }
}

} /* namespace drizzled */
