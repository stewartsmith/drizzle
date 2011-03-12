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

#include <config.h>

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

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

BOOST_AUTO_TEST_SUITE(PthreadAtomicOperations)
BOOST_AUTO_TEST_CASE(fetch_and_store)
{
  atomic_pthread<uint32_t> u235;

  BOOST_REQUIRE_EQUAL(0, u235.fetch_and_store(1));

  u235.fetch_and_store(15);

  BOOST_REQUIRE_EQUAL(15, u235.fetch_and_store(100));
  BOOST_REQUIRE_EQUAL(100, u235);
}

BOOST_AUTO_TEST_CASE(fetch_and_increment)
{
  atomic_pthread<uint32_t> u235;

  BOOST_REQUIRE_EQUAL(0, u235.fetch_and_increment());
  BOOST_REQUIRE_EQUAL(1, u235);
}

BOOST_AUTO_TEST_CASE(fetch_and_add)
{
  atomic_pthread<uint32_t> u235;

  BOOST_REQUIRE_EQUAL(0, u235.fetch_and_add(2));
  BOOST_REQUIRE_EQUAL(2, u235);
}

BOOST_AUTO_TEST_CASE(add_and_fetch)
{
  atomic_pthread<uint32_t> u235;

  BOOST_REQUIRE_EQUAL(10, u235.add_and_fetch(10));
  BOOST_REQUIRE_EQUAL(10, u235);
}

BOOST_AUTO_TEST_CASE(fetch_and_decrement)
{
  atomic_pthread<uint32_t> u235;

  u235.fetch_and_store(15);

  BOOST_REQUIRE_EQUAL(15, u235.fetch_and_decrement());
  BOOST_REQUIRE_EQUAL(14, u235);
}

BOOST_AUTO_TEST_CASE(compare_and_swap)
{
  atomic_pthread<uint32_t> u235;

  u235.fetch_and_store(100);

  BOOST_REQUIRE(not u235.compare_and_swap(42, 200));
  BOOST_REQUIRE(u235.compare_and_swap(200, 100));
  BOOST_REQUIRE_EQUAL(200, u235);
}

BOOST_AUTO_TEST_CASE(increment)
{
  atomic_pthread<uint32_t> u235;
  u235.fetch_and_store(200);
  BOOST_REQUIRE_EQUAL(201, u235.increment());
}

BOOST_AUTO_TEST_CASE(decrement)
{
  atomic_pthread<uint32_t> u235;
  u235.fetch_and_store(200);

  BOOST_REQUIRE_EQUAL(199, u235.decrement());
}
BOOST_AUTO_TEST_SUITE_END()
