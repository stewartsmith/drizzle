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

class NanoTimestampTest
{
  protected:
    NanoTimestamp nano_timestamp;
    bool result;
};

BOOST_FIXTURE_TEST_SUITE(NanoTimestampTestSuite, NanoTimestampTest)
BOOST_AUTO_TEST_CASE(is_valid_minOfNanoTimestampRange_shouldReturn_True)
{
  uint32_t year= 1970, month= 1, day= 1, hour= 0, minute= 0, second= 0, nanosecond= 0;
  TemporalGenerator::TimestampGen::make_nano_timestamp(&nano_timestamp, year, month, day, hour, minute, second, nanosecond);

  result= nano_timestamp.is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_maxOfNanoTimestampRange_shouldReturn_True)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 7, nanosecond= 0;
  TemporalGenerator::TimestampGen::make_nano_timestamp(&nano_timestamp, year, month, day, hour, minute, second, nanosecond);

  result= nano_timestamp.is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_oneMicroSecondBeforeNanoTimestampMinOfRange_shouldReturn_False)
{
  uint32_t year= 1969, month= 12, day= 31, hour= 23, minute= 59, second= 59, nanosecond= 999999999;
  TemporalGenerator::TimestampGen::make_nano_timestamp(&nano_timestamp, year, month, day, hour, minute, second, nanosecond);

  result= nano_timestamp.is_valid();

  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_oneMicroSecondAfterNanoTimestampMaxOfRange_shouldReturn_False)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 8, nanosecond= 1;
  TemporalGenerator::TimestampGen::make_nano_timestamp(&nano_timestamp, year, month, day, hour, minute, second, nanosecond);

  result= nano_timestamp.is_valid();

  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_InsideOfNanoTimestampRange_shouldReturn_True)
{
  uint32_t year= 1980, month= 11, day= 1, hour= 5, minute= 8, second= 5, nanosecond= 3265832;
  TemporalGenerator::TimestampGen::make_nano_timestamp(&nano_timestamp, year, month, day, hour, minute, second, nanosecond);

  result= nano_timestamp.is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(to_timespec)
{
  struct timespec filled;
  uint32_t year= 2009, month= 6, day= 3, hour= 4, minute= 59, second= 1, nanosecond= 675;
  TemporalGenerator::TimestampGen::make_nano_timestamp(&nano_timestamp, year, month, day, hour, minute, second, nanosecond);

  nano_timestamp.to_timespec(&filled);

  BOOST_REQUIRE_EQUAL(1244005141, filled.tv_sec);
  BOOST_REQUIRE_EQUAL(675, filled.tv_nsec);
}
BOOST_AUTO_TEST_SUITE_END()
