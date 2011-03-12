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

class DateTimestampTestCompareOperators
{
 protected:
  Timestamp sample_timestamp;
  bool result;
  
  Date identical_with_sample_timestamp, before_sample_timestamp, after_sample_timestamp;
  
  DateTimestampTestCompareOperators()
  {
    TemporalGenerator::TimestampGen::make_timestamp(&sample_timestamp, 2010, 9, 8, 0, 0, 0);
    TemporalGenerator::DateGen::make_date(&before_sample_timestamp, 1980, 1, 1);
    TemporalGenerator::DateGen::make_date(&identical_with_sample_timestamp, 2010, 9, 8);
    TemporalGenerator::DateGen::make_date(&after_sample_timestamp, 2019, 5, 30);
  }
};

class DateTimeTimestampTestCompareOperators
{
 protected:
  Timestamp sample_timestamp;
  bool result;
  
  DateTime identical_with_sample_timestamp, before_sample_timestamp, after_sample_timestamp;
  
  DateTimeTimestampTestCompareOperators()
  {
    TemporalGenerator::TimestampGen::make_timestamp(&sample_timestamp, 2010, 9, 8, 0, 0, 0);
    TemporalGenerator::DateTimeGen::make_datetime(&before_sample_timestamp, 1990, 12, 31, 12, 12, 30);
    TemporalGenerator::DateTimeGen::make_datetime(&identical_with_sample_timestamp, 2010, 9, 8, 0, 0, 0);
    TemporalGenerator::DateTimeGen::make_datetime(&after_sample_timestamp, 2020, 4, 4, 4, 4, 4);
  }
};

class TimestampTimestampTestCompareOperators
{
 protected:
  Timestamp sample_timestamp;
  bool result;
  
  Timestamp identical_with_sample_timestamp, before_sample_timestamp, after_sample_timestamp;
  
  TimestampTimestampTestCompareOperators()
  {
    TemporalGenerator::TimestampGen::make_timestamp(&sample_timestamp, 2010, 9, 8, 0, 0, 0);
    TemporalGenerator::TimestampGen::make_timestamp(&before_sample_timestamp, 2010, 9, 7, 23, 59, 59);
    TemporalGenerator::TimestampGen::make_timestamp(&identical_with_sample_timestamp, 2010, 9, 8, 0, 0, 0);
    TemporalGenerator::TimestampGen::make_timestamp(&after_sample_timestamp, 2010, 9, 8, 0, 0, 1);
  }
};


BOOST_AUTO_TEST_SUITE(TimestampCompareTestSuite)
BOOST_FIXTURE_TEST_CASE(DateTimestampTestCompareTest, DateTimestampTestCompareOperators)
{
  this->result= (this->sample_timestamp == this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp == this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp != this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp != this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp > this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp > this->after_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp > this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp >= this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp >= this->after_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp >= this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp < this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp < this->after_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp < this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp <= this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp <= this->after_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp <= this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);
}

BOOST_FIXTURE_TEST_CASE(DateTimeTimestampTestCompareTest, DateTimeTimestampTestCompareOperators)
{
  this->result= (this->sample_timestamp == this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp == this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp != this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp != this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp > this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp > this->after_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp > this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp >= this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp >= this->after_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp >= this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp < this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp < this->after_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp < this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp <= this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp <= this->after_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp <= this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);
}

BOOST_FIXTURE_TEST_CASE(TimestampTimestampTestCompareTest, TimestampTimestampTestCompareOperators)
{
  this->result= (this->sample_timestamp == this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp == this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp != this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp != this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp > this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp > this->after_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp > this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp >= this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp >= this->after_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp >= this->before_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp < this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp < this->after_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp < this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);

  this->result= (this->sample_timestamp <= this->identical_with_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp <= this->after_sample_timestamp);  
  BOOST_REQUIRE(this->result);

  this->result= (this->sample_timestamp <= this->before_sample_timestamp);  
  BOOST_REQUIRE(not this->result);
}
BOOST_AUTO_TEST_SUITE_END()

class TimestampTest
{
  protected:
    Timestamp timestamp;
    bool result;
};

BOOST_FIXTURE_TEST_SUITE(TimestampRangesSuite, TimestampTest)
BOOST_AUTO_TEST_CASE(is_valid_minOfTimestampRange_shouldReturn_True)
{
  uint32_t year= 1970, month= 1, day= 1, hour= 0, minute= 0, second= 0;
  TemporalGenerator::TimestampGen::make_timestamp(&timestamp, year, month, day, hour, minute, second);

  result= timestamp.is_valid();

  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_maxOfTimestampRange_shouldReturn_True)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 7;
  TemporalGenerator::TimestampGen::make_timestamp(&timestamp, year, month, day, hour, minute, second);
  
  result= timestamp.is_valid();
  
  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_oneSecondBeforeTimestampMinOfRange_shouldReturn_False)
{
  uint32_t year= 1969, month= 12, day= 31, hour= 23, minute= 59, second= 59;
  TemporalGenerator::TimestampGen::make_timestamp(&timestamp, year, month, day, hour, minute, second);
  
  result= timestamp.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_oneSecondAfterTimestampMaxOfRange_shouldReturn_False)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 8;
  TemporalGenerator::TimestampGen::make_timestamp(&timestamp, year, month, day, hour, minute, second);
  
  result= timestamp.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_InsideOfTimestampRange_shouldReturn_True)
{
  uint32_t year= 1980, month= 11, day= 1, hour= 5, minute= 8, second= 5;
  TemporalGenerator::TimestampGen::make_timestamp(&timestamp, year, month, day, hour, minute, second);
  
  result= timestamp.is_valid();
  
  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(to_time_t)
{
  time_t time;
  TemporalGenerator::TimestampGen::make_timestamp(&timestamp, 2009, 6, 3, 4, 59, 1);
  
  timestamp.to_time_t(time);
  
  BOOST_REQUIRE_EQUAL(1244005141, time);
}

BOOST_AUTO_TEST_CASE(outputStreamOperator_shouldWrite_hyphenSeperatedDateElements_and_colonSeperatedTimeElements)
{
  std::ostringstream output;
  std::string expected= "2010-05-01 08:07:06";
  std::string returned;
  TemporalGenerator::TimestampGen::make_timestamp(&timestamp, 2010, 5, 1, 8, 7, 6);
  
  output << timestamp;
  returned= output.str();
  
  BOOST_REQUIRE_EQUAL(expected, returned);
}
BOOST_AUTO_TEST_SUITE_END()
