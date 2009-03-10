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

#ifndef DRIZZLED_ATOMIC_SUN_STUDIO_H
#define DRIZZLED_ATOMIC_SUN_STUDIO_H

#define _KERNEL
#include <atomic.h>
#undef _KERNEL

inline uint8_t __sync_add_and_fetch(volatile uint8_t* ptr, uint8_t val)
{
  (val == 1) ? atomic_inc_8(ptr) : atomic_add_8(ptr, (int8_t)val);
  return *ptr;
}

inline uint16_t __sync_add_and_fetch(volatile uint16_t* ptr, uint16_t val)
{
  (val == 1) ? atomic_inc_16(ptr) : atomic_add_16(ptr, (int16_t)val);
  return *ptr;
}

inline uint32_t __sync_add_and_fetch(volatile uint32_t* ptr, uint32_t val)
{
  (val == 1) ? atomic_inc_32(ptr) : atomic_add_32(ptr, (int32_t)val);
  return *ptr;
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_add_and_fetch(volatile uint64_t* ptr, uint64_t val)
{
  (val == 1) ? atomic_inc_64(ptr) : atomic_add_64(ptr, (int64_t)val);
  return *ptr;
}
# endif /* defined(_KERNEL) || defined(_INT64_TYPE) */


inline uint8_t __sync_sub_and_fetch(volatile uint8_t* ptr, uint8_t val)
{
  (val == 1) ? atomic_dec_8(ptr) : atomic_add_8(ptr, 0-(int8_t)val);
  return *ptr;
}

inline uint16_t __sync_sub_and_fetch(volatile uint16_t* ptr, uint16_t val)
{
  (val == 1) ? atomic_dec_16(ptr) : atomic_add_16(ptr, 0-(int16_t)val);
  return *ptr;
}

inline uint32_t __sync_sub_and_fetch(volatile uint32_t* ptr, uint32_t val)
{
  (val == 1) ? atomic_dec_32(ptr) : atomic_add_32(ptr, 0-(int32_t)val);
  return *ptr;
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_sub_and_fetch(volatile uint64_t* ptr, uint64_t val)
{
  (val == 1) ? atomic_dec_64(ptr) : atomic_add_64(ptr, 0-(int64_t)val);
  return *ptr;
}
# endif /* defined(_KERNEL) || defined(_INT64_TYPE) */


inline uint8_t __sync_lock_test_and_set(volatile uint8_t* ptr, uint8_t val)
{
  atomic_swap_8(ptr, val);
  return *ptr;
}

inline uint16_t __sync_lock_test_and_set(volatile uint16_t* ptr, uint16_t val)
{
  atomic_swap_16(ptr, val);
  return *ptr;
}

inline uint32_t __sync_lock_test_and_set(volatile uint32_t* ptr, uint32_t val)
{
  atomic_swap_32(ptr, val);
  return *ptr;
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_lock_test_and_set(volatile uint64_t* ptr, uint64_t val)
{
  atomic_swap_64(ptr, val);
  return *ptr;
}
#endif /* defined(_KERNEL) || defined(_INT64_TYPE) */

inline uint8_t __sync_val_compare_and_swap(volatile uint8_t* ptr,
                                           uint8_t old_val, uint8_t val)
{
  atomic_cas_8(ptr, old_val, val);
  return *ptr;
}

inline uint16_t __sync_val_compare_and_swap(volatile uint16_t* ptr,
                                            uint16_t old_val, uint16_t val)
{
  atomic_cas_16(ptr, old_val, val);
  return *ptr;
}

inline uint32_t __sync_val_compare_and_swap(volatile uint32_t* ptr,
                                            uint32_t old_val, uint32_t val)
{
  atomic_cas_32(ptr, old_val, val);
  return *ptr;
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_val_compare_and_swap(volatile uint64_t* ptr,
                                            uint64_t old_val, uint64_t val)
{
  atomic_cas_64(ptr, old_val, val);
  return *ptr;
}
#endif /* defined(_KERNEL) || defined(_INT64_TYPE) */


#endif /* DRIZZLED_ATOMIC_SOLARIS_H */
