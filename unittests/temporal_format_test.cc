/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Pawel Blokus
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

#include <drizzled/temporal.h>
#include <drizzled/temporal_format.h>

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <vector>

#include "temporal_generator.h"

using namespace drizzled;

class TemporalFormatTest
{
  protected:
    TemporalFormat *tf;
    Temporal *temporal;
    bool result;

  TemporalFormatTest()
  {
    tf= NULL;
    temporal= NULL;
  }

  ~TemporalFormatTest()
  {
    delete tf;
    delete temporal;
  }
};

BOOST_FIXTURE_TEST_SUITE(TemporalFormatTestSuite, TemporalFormatTest)
BOOST_AUTO_TEST_CASE(constructor_WithValidRegExp_shouldCompile)
{
  tf= new TemporalFormat("^(\\d{4})[-/.]$");
  
  result= tf->is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(constructor_WithInvalidRegExp_shouldNotCompile)
{
  tf= new TemporalFormat("^\\d{4)[-/.]$");

  result= tf->is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(matches_OnNotMatchingString_shouldReturn_False)
{
  tf= new TemporalFormat("^(\\d{4})[-/.]$");
  char matched[] ="1234/ABC";

  result= tf->matches(matched, sizeof(matched) - sizeof(char), NULL);
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(matches_OnMatchingString_FormatWithIndexesSet_shouldPopulateTemporal)
{
  char regexp[]= "^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})[T|\\s+](\\d{2}):(\\d{2}):(\\d{2})$";
  char matched[]= "1999/9/14T23:29:05";
  tf= TemporalGenerator::TemporalFormatGen::make_temporal_format(regexp, 1, 2, 3, 4, 5, 6, 0, 0);
  temporal= new DateTime();

  
  result= tf->matches(matched, sizeof(matched) - sizeof(char), temporal);
  BOOST_REQUIRE(result);
  
  BOOST_REQUIRE_EQUAL(1999, temporal->years());
  BOOST_REQUIRE_EQUAL(9, temporal->months());
  BOOST_REQUIRE_EQUAL(14, temporal->days());
  BOOST_REQUIRE_EQUAL(23, temporal->hours());
  BOOST_REQUIRE_EQUAL(29, temporal->minutes());
  BOOST_REQUIRE_EQUAL(5, temporal->seconds());
}

BOOST_AUTO_TEST_CASE(matches_FormatWithMicroSecondIndexSet_shouldAddTrailingZeros)
{
  tf= TemporalGenerator::TemporalFormatGen::make_temporal_format("^(\\d{1,6})$", 0, 0, 0, 0, 0, 0, 1, 0);
  char matched[]= "560";
  temporal= new Time();
  
  tf->matches(matched, sizeof(matched) - sizeof(char), temporal);
  
  BOOST_REQUIRE_EQUAL(560000, temporal->useconds());
}

BOOST_AUTO_TEST_CASE(matches_FormatWithNanoSecondIndexSet_shouldAddTrailingZeros)
{
  tf= TemporalGenerator::TemporalFormatGen::make_temporal_format("^(\\d{1,9})$", 0, 0, 0, 0, 0, 0, 0, 1);
  char matched[]= "4321";
  temporal= new Time();
  
  tf->matches(matched, sizeof(matched) - sizeof(char), temporal);
  
  BOOST_REQUIRE_EQUAL(432100000, temporal->nseconds());
}
BOOST_AUTO_TEST_SUITE_END()

namespace drizzled
{
extern std::vector<TemporalFormat *> known_datetime_formats;
extern std::vector<TemporalFormat *> known_date_formats;
extern std::vector<TemporalFormat *> known_time_formats;
extern std::vector<TemporalFormat *> all_temporal_formats;
}

BOOST_AUTO_TEST_SUITE(TemporalFormatTestSuite)
BOOST_AUTO_TEST_CASE(init_temporal_formats_vectorsWithKnownFormats_shouldHaveExpectedLengths)
{
  init_temporal_formats();

  BOOST_REQUIRE_EQUAL(13, known_datetime_formats.size());	
  BOOST_REQUIRE_EQUAL(8, known_date_formats.size());
  BOOST_REQUIRE_EQUAL(11, known_time_formats.size());
  BOOST_REQUIRE_EQUAL(19, all_temporal_formats.size());
}

BOOST_AUTO_TEST_CASE(deinit_temporal_formats_vectorsWithKnownFormats_shouldHaveZeroLengths)
{
  init_temporal_formats();
  deinit_temporal_formats();
  
  BOOST_REQUIRE_EQUAL(0, known_datetime_formats.size());
  BOOST_REQUIRE_EQUAL(0, known_date_formats.size());
  BOOST_REQUIRE_EQUAL(0, known_time_formats.size());
  BOOST_REQUIRE_EQUAL(0, all_temporal_formats.size());
}
BOOST_AUTO_TEST_SUITE_END()
