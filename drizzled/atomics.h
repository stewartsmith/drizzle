/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *  Copyright 2005-2008 Intel Corporation.  All Rights Reserved.
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

#ifndef DRIZZLED_ATOMICS_H
#define DRIZZLED_ATOMICS_H

#if defined(HAVE_LIBTBB)
# include <tbb/atomic.h>
#else

# if defined(__SUNPRO_CC)
#  include <drizzled/atomic/sun_studio.h>
# endif
# if defined(HAVE_GCC_ATOMIC_BUILTINS) || defined(__SUNPRO_CC)
#  include <drizzled/atomic/gcc_traits.h>
#  define ATOMIC_TRAITS internal::gcc_traits
# else  /* use pthread impl */
#  define ATOMIC_TRAITS internal::pthread_traits
# endif

# include <pthread.h>
# include <drizzled/atomic/pthread_traits.h>


namespace tbb {

namespace internal {


template<typename I>            // Primary template
struct atomic_base {
  volatile I my_value;
};

template<typename I, typename D, typename T >
class atomic_impl: private atomic_base<I>
{
  T traits;
public:
  typedef I value_type;


  value_type fetch_and_add( D addend )
  {
    return traits.fetch_and_add(&this->my_value, addend);
  }

  value_type fetch_and_increment()
  {
    return traits.fetch_and_increment(&this->my_value);
  }

  value_type fetch_and_decrement()
  {
    return traits.fetch_and_decrement(&this->my_value);
  }

  value_type fetch_and_store( value_type value )
  {
    return traits.fetch_and_store(&this->my_value, value);
  }

  value_type compare_and_swap( value_type value, value_type comparand )
  {
    return traits.compare_and_swap(&this->my_value, value, comparand);
  }

  operator value_type() const volatile
  {
    return traits.fetch(&this->my_value);
  }

  value_type& _internal_reference() const
  {
    return this->my_value;
  }

protected:
  value_type store_with_release( value_type rhs )
  {
    return traits.store_with_release(&this->my_value, rhs);
  }

public:
  value_type operator+=( D addend )
  {
      return fetch_and_add(addend)+addend;
  }

  value_type operator-=( D addend )
  {
    // Additive inverse of addend computed using binary minus,
    // instead of unary minus, for sake of avoiding compiler warnings.
    return operator+=(D(0)-addend);
  }

  value_type operator++() {
    return fetch_and_add(1)+1;
  }

  value_type operator--() {
    return fetch_and_add(D(-1))-1;
  }

  value_type operator++(int) {
    return fetch_and_add(1);
  }

  value_type operator--(int) {
    return fetch_and_add(D(-1));
  }

};

} /* namespace internal */

//! Primary template for atomic.
/** See the Reference for details.
    @ingroup synchronization */
template<typename T>
struct atomic {
};

#define __TBB_DECL_ATOMIC(T)                                            \
  template<> struct atomic<T>                                           \
  : internal::atomic_impl<T,T,ATOMIC_TRAITS<T,T> > {                    \
    atomic<T>() : internal::atomic_impl<T,T,ATOMIC_TRAITS<T,T> >() {}   \
    T operator=( T rhs ) { return store_with_release(rhs); }            \
  };


__TBB_DECL_ATOMIC(long)
__TBB_DECL_ATOMIC(unsigned long)
__TBB_DECL_ATOMIC(unsigned int)
__TBB_DECL_ATOMIC(int)
__TBB_DECL_ATOMIC(unsigned short)
__TBB_DECL_ATOMIC(short)
__TBB_DECL_ATOMIC(char)
__TBB_DECL_ATOMIC(signed char)
__TBB_DECL_ATOMIC(unsigned char)
__TBB_DECL_ATOMIC(bool)

/* 32-bit platforms don't have a GCC atomic operation for 64-bit types,
 * so we'll use pthread locks to handler 64-bit types on that platforms
 */
#  if SIZEOF_SIZE_T >= SIZEOF_LONG_LONG
__TBB_DECL_ATOMIC(long long)
__TBB_DECL_ATOMIC(unsigned long long)
#  else
#   define __TBB_DECL_ATOMIC64(T)                                            \
  template<> struct atomic<T>                                           \
  : internal::atomic_impl<T,T,internal::pthread_traits<T,T> > {                    \
    atomic<T>() : internal::atomic_impl<T,T,internal::pthread_traits<T,T> >() {}   \
    T operator=( T rhs ) { return store_with_release(rhs); }            \
  };
__TBB_DECL_ATOMIC64(long long)
__TBB_DECL_ATOMIC64(unsigned long long)
#  endif

}
# endif /* defined(HAVE_LIBTBB) */

#endif /* DRIZZLED_ATOMIC_H */
