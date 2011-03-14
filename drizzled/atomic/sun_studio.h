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

#define _KERNEL
#include <atomic.h>
#undef _KERNEL

inline bool __sync_fetch_and_add(volatile bool* ptr, bool val)
{
  bool ret= *ptr;
  (val == true) ? atomic_inc_8((volatile uint8_t *)ptr) : atomic_add_8((volatile uint8_t *)ptr, (int8_t)val);
  return ret;
}
 
inline int8_t __sync_fetch_and_add(volatile int8_t* ptr, int8_t val)
{
  int8_t ret= *ptr; 
  (val == 1) ? atomic_inc_8((volatile uint8_t*)ptr) : atomic_add_8((volatile uint8_t*)ptr, val);
  return ret;
}

inline int16_t __sync_fetch_and_add(volatile int16_t* ptr, int16_t val)
{
  int16_t ret= *ptr;
  (val == 1) ? atomic_inc_16((volatile uint16_t*)ptr) : atomic_add_16((volatile uint16_t*)ptr, val);
  return ret;
}

inline int32_t __sync_fetch_and_add(volatile int32_t* ptr, int32_t val)
{
  int32_t ret= *ptr;
  (val == 1) ? atomic_inc_32((volatile uint32_t*)ptr) : atomic_add_32((volatile uint32_t*)ptr, val);
  return ret;
}

inline uint8_t __sync_fetch_and_add(volatile uint8_t* ptr, uint8_t val)
{
  uint8_t ret= *ptr;
  (val == 1) ? atomic_inc_8(ptr) : atomic_add_8(ptr, (int8_t)val);
  return ret;
}

inline uint16_t __sync_fetch_and_add(volatile uint16_t* ptr, uint16_t val)
{
  uint16_t ret= *ptr;
  (val == 1) ? atomic_inc_16(ptr) : atomic_add_16(ptr, (int16_t)val);
  return ret;
}

inline uint32_t __sync_fetch_and_add(volatile uint32_t* ptr, uint32_t val)
{
  uint32_t ret= *ptr;
  (val == 1) ? atomic_inc_32(ptr) : atomic_add_32(ptr, (int32_t)val);
  return ret;
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_fetch_and_add(volatile uint64_t* ptr, uint64_t val)
{
  uint64_t ret= *ptr;
  (val == 1) ? atomic_inc_64(ptr) : atomic_add_64(ptr, (int64_t)val);
  return ret;
}

inline int64_t __sync_fetch_and_add(volatile int64_t* ptr, int64_t val)
{
  int64_t ret= *ptr;
  (val == 1) ? atomic_inc_64((volatile uint64_t*)ptr) : atomic_add_64((volatile uint64_t*)ptr, val);
  return ret;
}
# endif /* defined(_KERNEL) || defined(_INT64_TYPE) */

inline uint8_t __sync_fetch_and_sub(volatile uint8_t* ptr, uint8_t val)
{
  uint8_t ret= *ptr;
  (val == 1) ? atomic_dec_8(ptr) : atomic_add_8(ptr, 0-(int8_t)val);
  return ret;
}

inline uint16_t __sync_fetch_and_sub(volatile uint16_t* ptr, uint16_t val)
{
  uint16_t ret= *ptr;
  (val == 1) ? atomic_dec_16(ptr) : atomic_add_16(ptr, 0-(int16_t)val);
  return ret;
}

inline uint32_t __sync_fetch_and_sub(volatile uint32_t* ptr, uint32_t val)
{
  uint32_t ret= *ptr;
  (val == 1) ? atomic_dec_32(ptr) : atomic_add_32(ptr, 0-(int32_t)val);
  return ret;
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_fetch_and_sub(volatile uint64_t* ptr, uint64_t val)
{
  uint64_t ret= *ptr;
  (val == 1) ? atomic_dec_64(ptr) : atomic_add_64(ptr, 0-(int64_t)val);
  return ret;
}
inline int64_t __sync_fetch_and_sub(volatile int64_t* ptr, uint64_t val)
{
  int64_t ret= *ptr;
  (val == 1) ? atomic_dec_64((volatile uint64_t *) ptr) : atomic_add_64((volatile uint64_t *) ptr, 0-(int64_t)val);
  return ret;
}
# endif /* defined(_KERNEL) || defined(_INT64_TYPE) */

inline bool __sync_add_and_fetch(volatile bool* ptr, bool val)
{
  return (val == true) ? atomic_inc_8_nv((volatile uint8_t *)ptr) : atomic_add_8_nv((volatile uint8_t *)ptr, (int8_t)val);
}
 
inline int8_t __sync_add_and_fetch(volatile int8_t* ptr, int8_t val)
{
  return (val == 1) ? atomic_inc_8_nv((volatile uint8_t*)ptr) : atomic_add_8_nv((volatile uint8_t*)ptr, val);
}

inline int16_t __sync_add_and_fetch(volatile int16_t* ptr, int16_t val)
{
  return (val == 1) ? atomic_inc_16_nv((volatile uint16_t*)ptr) : atomic_add_16_nv((volatile uint16_t*)ptr, val);
}

inline int32_t __sync_add_and_fetch(volatile int32_t* ptr, int32_t val)
{
  return (val == 1) ? atomic_inc_32_nv((volatile uint32_t*)ptr) : atomic_add_32_nv((volatile uint32_t*)ptr, val);
}

inline uint8_t __sync_add_and_fetch(volatile uint8_t* ptr, uint8_t val)
{
  return (val == 1) ? atomic_inc_8_nv(ptr) : atomic_add_8_nv(ptr, (int8_t)val);
}

inline uint16_t __sync_add_and_fetch(volatile uint16_t* ptr, uint16_t val)
{
  return (val == 1) ? atomic_inc_16_nv(ptr) : atomic_add_16_nv(ptr, (int16_t)val);
}

inline uint32_t __sync_add_and_fetch(volatile uint32_t* ptr, uint32_t val)
{
  return (val == 1) ? atomic_inc_32_nv(ptr) : atomic_add_32_nv(ptr, (int32_t)val);
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_add_and_fetch(volatile uint64_t* ptr, uint64_t val)
{
  return (val == 1) ? atomic_inc_64_nv(ptr) : atomic_add_64_nv(ptr, (int64_t)val);
}

inline int64_t __sync_add_and_fetch(volatile int64_t* ptr, int64_t val)
{
  return (val == 1) ? atomic_inc_64_nv((volatile uint64_t*)ptr) : atomic_add_64_nv((volatile uint64_t*)ptr, val);
}
# endif /* defined(_KERNEL) || defined(_INT64_TYPE) */

inline uint8_t __sync_sub_and_fetch(volatile uint8_t* ptr, uint8_t val)
{
  return (val == 1) ? atomic_dec_8_nv(ptr) : atomic_add_8_nv(ptr, 0-(int8_t)val);
}

inline uint16_t __sync_sub_and_fetch(volatile uint16_t* ptr, uint16_t val)
{
  return (val == 1) ? atomic_dec_16_nv(ptr) : atomic_add_16_nv(ptr, 0-(int16_t)val);
}

inline uint32_t __sync_sub_and_fetch(volatile uint32_t* ptr, uint32_t val)
{
  return (val == 1) ? atomic_dec_32_nv(ptr) : atomic_add_32_nv(ptr, 0-(int32_t)val);
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_sub_and_fetch(volatile uint64_t* ptr, uint64_t val)
{
  return (val == 1) ? atomic_dec_64_nv(ptr) : atomic_add_64_nv(ptr, 0-(int64_t)val);
}
inline int64_t __sync_sub_and_fetch(volatile int64_t* ptr, uint64_t val)
{
  return (val == 1) ? atomic_dec_64_nv((volatile uint64_t *) ptr) : atomic_add_64_nv((volatile uint64_t *) ptr, 0-(int64_t)val);
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

inline int8_t __sync_val_compare_and_swap(volatile int8_t* ptr,
                                           int8_t old_val, int8_t val)
{
  atomic_cas_8((volatile uint8_t *)ptr, old_val, val);
  return *ptr;
}

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

inline int8_t __sync_bool_compare_and_swap(volatile int8_t* ptr,
                                           int8_t old_val, int8_t val)
{
  int8_t orig= *ptr;
  return orig == atomic_cas_8((volatile uint8_t *)ptr, old_val, val);
}

inline uint8_t __sync_bool_compare_and_swap(volatile uint8_t* ptr,
                                           uint8_t old_val, uint8_t val)
{
  uint8_t orig= *ptr;
  return orig == atomic_cas_8(ptr, old_val, val);
}

inline uint16_t __sync_bool_compare_and_swap(volatile uint16_t* ptr,
                                            uint16_t old_val, uint16_t val)
{
  uint16_t orig= *ptr;
  return orig == atomic_cas_16(ptr, old_val, val);
}

inline uint32_t __sync_bool_compare_and_swap(volatile uint32_t* ptr,
                                            uint32_t old_val, uint32_t val)
{
  uint32_t orig= *ptr;
  return orig == atomic_cas_32(ptr, old_val, val);
}

# if defined(_KERNEL) || defined(_INT64_TYPE)
inline uint64_t __sync_bool_compare_and_swap(volatile uint64_t* ptr,
                                            uint64_t old_val, uint64_t val)
{
  uint64_t orig= *ptr;
  return orig == atomic_cas_64(ptr, old_val, val);
}
#endif /* defined(_KERNEL) || defined(_INT64_TYPE) */

