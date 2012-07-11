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

#include <config.h>

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <drizzled/identifier.h>

using namespace drizzled;

BOOST_AUTO_TEST_SUITE(TableIdentifierTest)
BOOST_AUTO_TEST_CASE(CreateStandard)
{
  identifier::Table identifier(identifier::Catalog(str_ref("local")), "test", "a");
  BOOST_REQUIRE_EQUAL("local/test/a", identifier.getPath());
  BOOST_REQUIRE_EQUAL("test.a", identifier.getSQLPath());
}

BOOST_AUTO_TEST_CASE(CreateTemporary)
{
  identifier::Table identifier(identifier::Catalog(str_ref("local")),"test", "a", message::Table::TEMPORARY);
  BOOST_REQUIRE_EQUAL("/#sql", identifier.getPath().substr(0, 5));
  BOOST_REQUIRE_EQUAL("test.#a", identifier.getSQLPath());
}

BOOST_AUTO_TEST_CASE(CreateInternal)
{
  identifier::Table identifier(identifier::Catalog(str_ref("local")),"test", "a", message::Table::TEMPORARY);
  BOOST_REQUIRE_EQUAL("/#sql", identifier.getPath().substr(0, 5));
  BOOST_REQUIRE_EQUAL("test.#a", identifier.getSQLPath());
}

BOOST_AUTO_TEST_CASE(StaticTmpTable)
{
  std::string pathname(identifier::Table::build_tmptable_filename());

  BOOST_REQUIRE_GT(pathname.size(), 0);
  BOOST_REQUIRE_GT(strlen(&pathname[0]), 0);
}

BOOST_AUTO_TEST_CASE(Key)
{
  identifier::Table identifier(identifier::Catalog(str_ref("LOCAL")),"test", "a");

  const identifier::Table::Key key= identifier.getKey();

  BOOST_REQUIRE_EQUAL(key.size(), 13);
  int i=0;
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 'L');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 'O');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 'C');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 'A');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 'L');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 0);
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 't');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 'e');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 's');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 't');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 0);
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 'a');
  BOOST_REQUIRE_EQUAL(key.vector()[i++], 0);
}

BOOST_AUTO_TEST_CASE(KeyCompare)
{
  identifier::Table identifier(identifier::Catalog(str_ref("local")), "test", "a");
  identifier::Table identifier2(identifier::Catalog(str_ref("local")), "test", "a");

  BOOST_REQUIRE_EQUAL((identifier.getKey() == identifier.getKey()), true);
}
BOOST_AUTO_TEST_SUITE_END()
