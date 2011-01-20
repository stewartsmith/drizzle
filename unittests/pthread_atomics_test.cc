/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Stewart Smith
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

#include "config.h"

#include <gtest/gtest.h>

# if defined(__SUNPRO_CC)
#  include <drizzled/atomic/sun_studio.h>
# endif

# if !defined(__ICC) && (defined(HAVE_GCC_ATOMIC_BUILTINS) || defined(__SUNPRO_CC))
#  include <drizzled/atomic/gcc_traits.h>
#  define ATOMIC_TRAITS internal::gcc_traits
# else  /* use pthread impl */
#  define ATOMIC_TRAITS internal::pthread_traits
# endif

#include <pthread.h>
#include <drizzled/atomic/pthread_traits.h>

#include <drizzled/atomics.h>

using namespace drizzled;

template<typename T>
struct atomic_pthread {
};

#   define __DRIZZLE_DECL_ATOMIC_PTHREAD(T)                              \
  template<> struct atomic_pthread<T>                                   \
  : internal::atomic_impl<T,T, internal::pthread_traits<T,T> > {         \
    atomic_pthread<T>()                                                 \
      : internal::atomic_impl<T,T, internal::pthread_traits<T,T> >() {}  \
    T operator=( T rhs ) { return store_with_release(rhs); }            \
  };

__DRIZZLE_DECL_ATOMIC_PTHREAD(unsigned int)

TEST(pthread_atomic_operations, fetch_and_store)
{
  atomic_pthread<uint32_t> u235;

  EXPECT_EQ(0, u235.fetch_and_store(1));

  u235.fetch_and_store(15);

  EXPECT_EQ(15, u235.fetch_and_store(100));
  EXPECT_EQ(100, u235);
}

TEST(pthread_atomic_operations, fetch_and_increment)
{
  atomic_pthread<uint32_t> u235;

  EXPECT_EQ(0, u235.fetch_and_increment());
  EXPECT_EQ(1, u235);
}

TEST(pthread_atomic_operations, fetch_and_add)
{
  atomic_pthread<uint32_t> u235;

  EXPECT_EQ(0, u235.fetch_and_add(2));
  EXPECT_EQ(2, u235);
}

TEST(pthread_atomic_operations, add_and_fetch)
{
  atomic_pthread<uint32_t> u235;

  EXPECT_EQ(10, u235.add_and_fetch(10));
  EXPECT_EQ(10, u235);
}

TEST(pthread_atomic_operations, fetch_and_decrement)
{
  atomic_pthread<uint32_t> u235;

  u235.fetch_and_store(15);

  EXPECT_EQ(15, u235.fetch_and_decrement());
  EXPECT_EQ(14, u235);
}

TEST(pthread_atomic_operations, compare_and_swap)
{
  atomic_pthread<uint32_t> u235;

  u235.fetch_and_store(100);

  ASSERT_FALSE(u235.compare_and_swap(42, 200));
  ASSERT_TRUE(u235.compare_and_swap(200, 100));
  EXPECT_EQ(200, u235);
}

TEST(pthread_atomic_operations, increment)
{
  atomic_pthread<uint32_t> u235;
  u235.fetch_and_store(200);
  EXPECT_EQ(201, u235.increment());
}

TEST(pthread_atomic_operations, decrement)
{
  atomic_pthread<uint32_t> u235;
  u235.fetch_and_store(200);

  EXPECT_EQ(199, u235.decrement());
}

