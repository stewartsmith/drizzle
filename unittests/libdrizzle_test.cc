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

#include <libdrizzle/drizzle_client.h>
#include <libdrizzle/drizzle_server.h>

BOOST_AUTO_TEST_SUITE(LibDrizzle)
BOOST_AUTO_TEST_CASE(drizzleEscapeString)
{
  const char* orig= "hello \"world\"\n";
  char out[255];
  size_t out_len;

  out_len= drizzle_escape_string(out, orig, strlen(orig));

  BOOST_REQUIRE_EQUAL(17, out_len);
  BOOST_REQUIRE_EQUAL("hello \\\"world\\\"\\n", out);
}

BOOST_AUTO_TEST_CASE(drizzleEscapeStringBinary)
{
  const char orig[6]= {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  char out[255];
  size_t out_len;

  out_len= drizzle_escape_string(out, orig, 6);

  BOOST_REQUIRE_EQUAL(7, out_len);
  BOOST_REQUIRE_EQUAL("\\0\1\2\3\4\5", out);
}

BOOST_AUTO_TEST_CASE(drizzleSafeEscapeString)
{
  const char* orig= "hello \"world\"\n";
  char out[255];
  ssize_t out_len;

  out_len= drizzle_safe_escape_string(out, 255, orig, strlen(orig));

  BOOST_REQUIRE_EQUAL(17, out_len);
  BOOST_REQUIRE_EQUAL("hello \\\"world\\\"\\n", out);
}

BOOST_AUTO_TEST_CASE(drizzleSafeEscapeStringFail)
{
  const char* orig= "hello \"world\"\n";
  char out[5];
  ssize_t out_len;

  out_len= drizzle_safe_escape_string(out, 5, orig, strlen(orig));

  BOOST_REQUIRE_EQUAL(-1, out_len);
}

BOOST_AUTO_TEST_CASE(drizzleHexString)
{
  const unsigned char orig[5]= {0x34, 0x26, 0x80, 0x99, 0xFF};
  char out[255];
  size_t out_len;

  out_len= drizzle_hex_string(out, (char*) orig, 5);

  BOOST_REQUIRE_EQUAL(10, out_len);
  BOOST_REQUIRE_EQUAL("34268099FF", out);
}

BOOST_AUTO_TEST_CASE(drizzleMysqlPasswordHash)
{
  const char* orig= "test password";
  char out[255];

  drizzle_mysql_password_hash(out, orig, strlen(orig));

  BOOST_REQUIRE_EQUAL("3B942720DACACBBA7E3838AF03C5B6B5A6DFE0AB", out);
}
BOOST_AUTO_TEST_SUITE_END()
