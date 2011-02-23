/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Andrew Hutchings
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

#include <drizzled/global_buffer.h>

BOOST_AUTO_TEST_SUITE(GlobalBufferTests)
BOOST_AUTO_TEST_CASE(overflow)
{
  drizzled::global_buffer_constraint<uint64_t> test_buffer(1024);

  BOOST_REQUIRE(test_buffer.add(512));
  BOOST_REQUIRE(test_buffer.add(512));
  BOOST_REQUIRE(not test_buffer.add(1));
}

BOOST_AUTO_TEST_CASE(subtract)
{
  drizzled::global_buffer_constraint<uint64_t> test_buffer(1024);

  BOOST_REQUIRE(test_buffer.add(1024));
  BOOST_REQUIRE(test_buffer.sub(512));
  BOOST_REQUIRE(test_buffer.add(512));
  BOOST_REQUIRE(not test_buffer.add(1));
}

BOOST_AUTO_TEST_CASE(underflow)
{
  drizzled::global_buffer_constraint<uint64_t> test_buffer(1024);

  BOOST_REQUIRE(test_buffer.add(10));
  BOOST_REQUIRE(not test_buffer.sub(11));
}

BOOST_AUTO_TEST_CASE(change_max)
{
  drizzled::global_buffer_constraint<uint64_t> test_buffer(1024);

  test_buffer.setMaxSize(512);

  BOOST_REQUIRE(not test_buffer.add(513));
}
BOOST_AUTO_TEST_SUITE_END()
