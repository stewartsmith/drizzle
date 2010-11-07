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

#include <drizzled/atomics.h>

using namespace drizzled;

TEST(atomic_operations, fetch_and_store)
{
  atomic<uint32_t> u235;

  EXPECT_EQ(0, u235.fetch_and_store(1));

  u235.fetch_and_store(15);

  EXPECT_EQ(15, u235.fetch_and_store(100));
  EXPECT_EQ(100, u235);
}

TEST(atomic_operations, fetch_and_increment)
{
  atomic<uint32_t> u235;

  EXPECT_EQ(0, u235.fetch_and_increment());
  EXPECT_EQ(1, u235);
}

TEST(atomic_operations, fetch_and_add)
{
  atomic<uint32_t> u235;

  EXPECT_EQ(0, u235.fetch_and_add(2));
  EXPECT_EQ(2, u235);
}

TEST(atomic_operations, add_and_fetch)
{
  atomic<uint32_t> u235;

  EXPECT_EQ(10, u235.add_and_fetch(10));
  EXPECT_EQ(10, u235);
}

TEST(atomic_operations, fetch_and_decrement)
{
  atomic<uint32_t> u235;

  u235.fetch_and_store(15);

  EXPECT_EQ(15, u235.fetch_and_decrement());
  EXPECT_EQ(14, u235);
}

TEST(atomic_operations, compare_and_swap)
{
  atomic<uint32_t> u235;

  u235.fetch_and_store(100);

  ASSERT_FALSE(u235.compare_and_swap(42, 200));
  ASSERT_TRUE(u235.compare_and_swap(200, 100));
  EXPECT_EQ(200, u235);
}

TEST(atomic_operations, increment)
{
  atomic<uint32_t> u235;
  u235.fetch_and_store(200);
  EXPECT_EQ(201, u235.increment());
}

TEST(atomic_operations, decrement)
{
  atomic<uint32_t> u235;
  u235.fetch_and_store(200);

  EXPECT_EQ(199, u235.decrement());
}

/*
TEST(atomic_operations, increment_assign)
{
  atomic<uint32_t> u235;
  u235.fetch_and_store(200);

  EXPECT_EQ(242, u235+=42);
}

TEST(atomic_operations, decrement_assign)
{
  atomic<uint32_t> u235;
  u235.fetch_and_store(200);

  EXPECT_EQ(158, u235-=42);
}
*/


