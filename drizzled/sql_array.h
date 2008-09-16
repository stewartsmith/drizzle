/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <mysys/my_sys.h>

/* 
  Array of pointers to Elem that uses memory from MEM_ROOT

  MEM_ROOT has no realloc() so this is supposed to be used for cases when
  reallocations are rare.
*/

template <class Elem> class Array
{
  enum {alloc_increment = 16};
  Elem **buffer;
  uint n_elements, max_element;
public:
  Array(MEM_ROOT *mem_root, uint prealloc=16)
  {
    buffer= (Elem**)alloc_root(mem_root, prealloc * sizeof(Elem**));
    max_element = buffer? prealloc : 0;
    n_elements= 0;
  }

  Elem& at(int idx)
  {
    return *(((Elem*)buffer) + idx);
  }

  Elem **front()
  {
    return buffer;
  }

  Elem **back()
  {
    return buffer + n_elements;
  }

  bool append(MEM_ROOT *mem_root, Elem *el)
  {
    if (n_elements == max_element)
    {
      Elem **newbuf;
      if (!(newbuf= (Elem**)alloc_root(mem_root, (n_elements + alloc_increment)*
                                                  sizeof(Elem**))))
      {
        return false;
      }
      memcpy(newbuf, buffer, n_elements*sizeof(Elem*));
      buffer= newbuf;
    }
    buffer[n_elements++]= el;
    return false;
  }

  int elements()
  {
    return n_elements;
  }

  void clear()
  {
    n_elements= 0;
  }

  typedef int (*CMP_FUNC)(Elem * const *el1, Elem *const *el2);

  void sort(CMP_FUNC cmp_func)
  {
    my_qsort(buffer, n_elements, sizeof(Elem*), (qsort_cmp)cmp_func);
  }
};

