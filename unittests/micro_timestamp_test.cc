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

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <drizzled/temporal.h>

#include "temporal_generator.h"

using namespace drizzled;

class MicroTimestampTest
{
  protected:
    MicroTimestamp micro_timestamp;
    bool result;
};

BOOST_FIXTURE_TEST_SUITE(MicroTimestampTestSuite, MicroTimestampTest)
BOOST_AUTO_TEST_CASE(is_valid_minOfMicroTimestampRange_shouldReturn_True)
{
  uint32_t year= 1970, month= 1, day= 1, hour= 0, minute= 0, second= 0, microsecond= 0;
  TemporalGenerator::TimestampGen::make_micro_timestamp(&micro_timestamp, year, month, day, hour, minute, second, microsecond);

  result= micro_timestamp.is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_maxOfMicroTimestampRange_shouldReturn_True)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 7, microsecond= 0;
  TemporalGenerator::TimestampGen::make_micro_timestamp(&micro_timestamp, year, month, day, hour, minute, second, microsecond);

  result= micro_timestamp.is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_oneMicroSecondBeforeMicroTimestampMinOfRange_shouldReturn_False)
{
  uint32_t year= 1969, month= 12, day= 31, hour= 23, minute= 59, second= 59, microsecond= 999999;
  TemporalGenerator::TimestampGen::make_micro_timestamp(&micro_timestamp, year, month, day, hour, minute, second, microsecond);

  result= micro_timestamp.is_valid();

  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_oneMicroSecondAfterMicroTimestampMaxOfRange_shouldReturn_False)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 8, microsecond= 1;
  TemporalGenerator::TimestampGen::make_micro_timestamp(&micro_timestamp, year, month, day, hour, minute, second, microsecond);

  result= micro_timestamp.is_valid();

  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_InsideOfMicroTimestampRange_shouldReturn_True)
{
  uint32_t year= 1980, month= 11, day= 1, hour= 5, minute= 8, second= 5, microsecond= 18263;
  TemporalGenerator::TimestampGen::make_micro_timestamp(&micro_timestamp, year, month, day, hour, minute, second, microsecond);

  result= micro_timestamp.is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(to_string_shouldProduce_hyphenSeperatedDateElements_and_colonSeperatedTimeElements)
{
  char expected[MicroTimestamp::MAX_STRING_LENGTH]= "2010-05-01 08:07:06.007654";
  char returned[MicroTimestamp::MAX_STRING_LENGTH];
  TemporalGenerator::TimestampGen::make_micro_timestamp(&micro_timestamp, 2010, 5, 1, 8, 7, 6, 7654);
  
  micro_timestamp.to_string(returned, MicroTimestamp::MAX_STRING_LENGTH);
  
  BOOST_REQUIRE_EQUAL(expected, returned);
}

BOOST_AUTO_TEST_CASE(to_timeval)
{
  struct timeval filled;
  uint32_t year= 2009, month= 6, day= 3, hour= 4, minute= 59, second= 1, microsecond= 675;
  TemporalGenerator::TimestampGen::make_micro_timestamp(&micro_timestamp, year, month, day, hour, minute, second, microsecond);

  micro_timestamp.to_timeval(filled);

  BOOST_REQUIRE_EQUAL(1244005141, filled.tv_sec);
  BOOST_REQUIRE_EQUAL(675, filled.tv_usec);
}
BOOST_AUTO_TEST_SUITE_END()
