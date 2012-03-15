/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

namespace drizzled {
namespace internal {

template<typename T, typename D>
class gcc_traits
{

public:
  typedef T value_type;

  gcc_traits() {}

  inline value_type add_and_fetch(volatile value_type *value, D addend )
  {
    return __sync_add_and_fetch(value, addend);
  }

  inline value_type fetch_and_add(volatile value_type *value, D addend )
  {
    return __sync_fetch_and_add(value, addend);
  }

  inline value_type fetch_and_increment(volatile value_type *value)
  {
    return __sync_fetch_and_add(value, 1);
  }

  inline value_type fetch_and_decrement(volatile value_type *value)
  {
    return __sync_fetch_and_sub(value, 1);
  }

  inline value_type fetch_and_store(volatile value_type *value,
                                    value_type new_value)
  {
    return __sync_lock_test_and_set(value, new_value);
  }

  inline bool compare_and_swap(volatile value_type *value,
                                     value_type new_value,
                                     value_type comparand )
  {
    return __sync_bool_compare_and_swap(value, comparand, new_value);
  }

  inline value_type fetch(const volatile value_type *value) const volatile
  {
    /* 
     * This is necessary to ensure memory barriers are respected when
     * simply returning the value pointed at.  However, this does not
     * compile on ICC.
     *
     * @todo
     *
     * Look at how to rewrite the below to something that ICC feels is
     * OK and yet respects memory barriers.
     */
    return __sync_fetch_and_add(const_cast<value_type *>(value), 0);
  }

  inline value_type store_with_release(volatile value_type *value,
                                       value_type new_value)
  {
    *value= new_value;
    return *value;
  }

}; /* gcc_traits */


} /* namespace internal */
} /* namespace drizzled */

