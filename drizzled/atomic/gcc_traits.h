/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_ATOMIC_GCC_TRAITS_H
#define DRIZZLED_ATOMIC_GCC_TRAITS_H

namespace tbb {
namespace internal {

template<typename T, typename D>
class gcc_traits
{

public:
  typedef T value_type;

  gcc_traits() {}

  /* YES. I know these are semantically backwards...
   * so... TODO: Ensure we're doing the "right" thing here
   */
  inline value_type fetch_and_add(volatile value_type *value, D addend )
  {
    return __sync_add_and_fetch(value, addend);
  }

  inline value_type fetch_and_increment(volatile value_type *value)
  {
    return __sync_add_and_fetch(value, 1);
  }

  inline value_type fetch_and_decrement(volatile value_type *value)
  {
    return __sync_sub_and_fetch(value, 1);
  }

  inline value_type fetch_and_store(volatile value_type *value,
                                    value_type new_value)
  {
    /* TODO: Is this the right one? */
    return __sync_lock_test_and_set(value, new_value);
  }

  inline value_type compare_and_swap(volatile value_type *value,
                                     value_type new_value,
                                     value_type comparand )
  {
    return __sync_val_compare_and_swap(value, comparand, new_value);
  }

  inline value_type fetch(const volatile value_type *value) const volatile
  {
    return __sync_add_and_fetch(const_cast<value_type *>(value), 0);
  }

  inline value_type store_with_release(volatile value_type *value,
                                       value_type new_value)
  {
    *value= new_value;
    return *value;
  }

}; /* gcc_traits */


} /* namespace internal */
} /* namespace tbb */

#endif /* DRIZZLED_ATOMIC_GCC_TRAITS_H */
