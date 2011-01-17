/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <drizzled/identifier.h>

using namespace drizzled;

TEST(table_identifier_test_standard, Create)
{
  identifier::Table identifier("test", "a");
  EXPECT_EQ("test/a", identifier.getPath());
  std::string path;
  identifier.getSQLPath(path);
  EXPECT_EQ("test.a", path);
}

TEST(table_identifier_test_temporary, Create)
{
  identifier::Table identifier("test", "a", message::Table::TEMPORARY);
  EXPECT_EQ("/#sql", identifier.getPath().substr(0, 5));
  std::string path;
  identifier.getSQLPath(path);
  EXPECT_EQ("test.#a", path);
}

TEST(table_identifier_test_internal, Create)
{
  identifier::Table identifier("test", "a", message::Table::TEMPORARY);
  EXPECT_EQ("/#sql", identifier.getPath().substr(0, 5));
  std::string path;
  identifier.getSQLPath(path);
  EXPECT_EQ("test.#a", path);
}

TEST(table_identifier_test_build_tmptable_filename, Static)
{
  std::vector<char> pathname;

  identifier::Table::build_tmptable_filename(pathname);

  EXPECT_GT(pathname.size(), 0);
  EXPECT_GT(strlen(&pathname[0]), 0);
}

TEST(table_identifier_test_key, Key)
{
  identifier::Table identifier("test", "a");

  const identifier::Table::Key key= identifier.getKey();

  EXPECT_EQ(key.size(), 7);
  EXPECT_EQ(key.vector()[0], 't');
  EXPECT_EQ(key.vector()[1], 'e');
  EXPECT_EQ(key.vector()[2], 's');
  EXPECT_EQ(key.vector()[3], 't');
  EXPECT_EQ(key.vector()[4], 0);
  EXPECT_EQ(key.vector()[5], 'a');
  EXPECT_EQ(key.vector()[6], 0);
}

TEST(table_identifier_test_key, KeyCompare)
{
  identifier::Table identifier("test", "a");
  identifier::Table identifier2("test", "a");

  EXPECT_EQ((identifier.getKey() == identifier.getKey()), true);
}
