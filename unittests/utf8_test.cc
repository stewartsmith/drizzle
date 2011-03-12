/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <string>

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <drizzled/utf8/utf8.h>

using namespace drizzled;

BOOST_AUTO_TEST_SUITE(UTF8)
BOOST_AUTO_TEST_CASE(is_single)
{
  BOOST_REQUIRE(utf8::is_single('a'));
  const char *multi_byte= "ç";
  BOOST_REQUIRE(not utf8::is_single(*multi_byte));
  BOOST_REQUIRE(not utf8::is_single(*(multi_byte + 1)));
}

BOOST_AUTO_TEST_CASE(codepoint_length)
{
  uint32_t one_byte= 0x0024;
  uint32_t two_bytes= 0x00A2;
  uint32_t three_bytes= 0x20AC;
  uint32_t four_bytes= 0x024B62;
  BOOST_REQUIRE_EQUAL(1, utf8::codepoint_length(one_byte));
  BOOST_REQUIRE_EQUAL(2, utf8::codepoint_length(two_bytes));
  BOOST_REQUIRE_EQUAL(3, utf8::codepoint_length(three_bytes));
  BOOST_REQUIRE_EQUAL(4, utf8::codepoint_length(four_bytes));
}

BOOST_AUTO_TEST_CASE(sequence_length)
{
  const char *one_byte= "$";
  const char *two_bytes= "¢";
  const char *three_bytes= "€";
  const char *four_bytes= "𤭢";
  BOOST_REQUIRE_EQUAL(1, utf8::sequence_length(*one_byte));
  BOOST_REQUIRE_EQUAL(2, utf8::sequence_length(*two_bytes));
  BOOST_REQUIRE_EQUAL(3, utf8::sequence_length(*three_bytes));
  BOOST_REQUIRE_EQUAL(4, utf8::sequence_length(*four_bytes));
}

BOOST_AUTO_TEST_CASE(char_length)
{
  const char *one_byte= "$";
  const char *two_bytes= "¢";
  const char *three_bytes= "€";
  const char *four_bytes= "𤭢";
  const char *test_string= "__tañgè Ñãmé";
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(one_byte));
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(two_bytes));
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(three_bytes));
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(four_bytes));
  BOOST_REQUIRE_EQUAL(12, utf8::char_length(test_string));
}

BOOST_AUTO_TEST_CASE(char_length_string)
{
  std::string one_byte= "$";
  std::string two_bytes= "¢";
  std::string three_bytes= "€";
  std::string four_bytes= "𤭢";
  std::string test_string= "__tañgè Ñãmé";
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(one_byte));
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(two_bytes));
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(three_bytes));
  BOOST_REQUIRE_EQUAL(1, utf8::char_length(four_bytes));
  BOOST_REQUIRE_EQUAL(12, utf8::char_length(test_string));
}
BOOST_AUTO_TEST_SUITE_END()
