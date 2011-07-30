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

/* Sort of string pointers in string-order with radix or qsort */

#include <config.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>

namespace drizzled {
namespace internal {

void my_string_ptr_sort(unsigned char *base, uint32_t items, size_t size)
{
#if INT_MAX > 65536L
  if (size <= 20 && items >= 1000 && items < 100000)
  {
    unsigned char** ptr= (unsigned char**) malloc(items*sizeof(char*));
    radixsort_for_str_ptr((unsigned char**) base,items,size,ptr);
    free((unsigned char*) ptr);
  }
  else
#else
  assert(false);
#endif
  {
    if (size && items)
    {
      my_qsort2(base,items, sizeof(unsigned char*), get_ptr_compare(size),
                (void*) &size);
    }
  }
}

} /* namespace internal */
} /* namespace drizzled */
