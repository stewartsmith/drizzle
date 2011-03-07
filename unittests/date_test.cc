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

#include <drizzled/type/decimal.h>
#include <drizzled/temporal.h>
#include <drizzled/temporal_format.h>

#include "temporal_generator.h"

using namespace drizzled;

class DateTestCompareOperators
{
 protected:
  Date sample_date;
  bool result;
  
  Date identical_with_sample_date, before_sample_date, after_sample_date;
  
  DateTestCompareOperators()
  {
    TemporalGenerator::DateGen::make_date(&sample_date, 2010, 9, 8);
    TemporalGenerator::DateGen::make_date(&before_sample_date, 1980, 1, 1);
    TemporalGenerator::DateGen::make_date(&identical_with_sample_date, 2010, 9, 8);
    TemporalGenerator::DateGen::make_date(&after_sample_date, 2019, 5, 30);
  }
};

class DateTimeTestCompareOperators
{
 protected:
  Date sample_date;
  bool result;
  
  DateTime identical_with_sample_date, before_sample_date, after_sample_date;
  
  DateTimeTestCompareOperators()
  {
    TemporalGenerator::DateGen::make_date(&sample_date, 2010, 9, 8);
    TemporalGenerator::DateTimeGen::make_datetime(&before_sample_date, 1990, 12, 31, 12, 12, 30);
    TemporalGenerator::DateTimeGen::make_datetime(&identical_with_sample_date, 2010, 9, 8, 0, 0, 0);
    TemporalGenerator::DateTimeGen::make_datetime(&after_sample_date, 2020, 4, 4, 4, 4, 4);
  }
};

class TimestampTestCompareOperators
{
 protected:
  Date sample_date;
  bool result;
  
  Timestamp identical_with_sample_date, before_sample_date, after_sample_date;
  
  TimestampTestCompareOperators()
  {
    TemporalGenerator::DateGen::make_date(&sample_date, 2010, 9, 8);
    TemporalGenerator::TimestampGen::make_timestamp(&before_sample_date, 1980, 1, 1, 13, 56, 41);
    TemporalGenerator::TimestampGen::make_timestamp(&identical_with_sample_date, 2010, 9, 8, 0, 0, 0);
    TemporalGenerator::TimestampGen::make_timestamp(&after_sample_date, 2019, 5, 30, 9, 10, 13);
  }
};

BOOST_AUTO_TEST_SUITE(DateTestCompare)
BOOST_FIXTURE_TEST_CASE(operatorComparingDate, DateTestCompareOperators)
{
  result= (sample_date == identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date == before_sample_date);  
  BOOST_REQUIRE(not result);

  result= (sample_date != identical_with_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date != before_sample_date);  
  BOOST_REQUIRE(result);

  result= (sample_date > identical_with_sample_date);
  BOOST_REQUIRE(not this->result);

  result= (sample_date > after_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date > before_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date >= identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date >= after_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date >= before_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date < identical_with_sample_date);
  
  BOOST_REQUIRE(not result);

  result= (sample_date < after_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date < before_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date <= identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date <= after_sample_date);
  
  BOOST_REQUIRE(result);

  result= (sample_date <= before_sample_date);
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(operatorComparingDateTime, DateTimeTestCompareOperators)
{
  result= (sample_date == identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date == before_sample_date);  
  BOOST_REQUIRE(not result);

  result= (sample_date != identical_with_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date != before_sample_date);  
  BOOST_REQUIRE(result);

  result= (sample_date > identical_with_sample_date);
  BOOST_REQUIRE(not this->result);

  result= (sample_date > after_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date > before_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date >= identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date >= after_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date >= before_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date < identical_with_sample_date);
  
  BOOST_REQUIRE(not result);

  result= (sample_date < after_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date < before_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date <= identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date <= after_sample_date);
  
  BOOST_REQUIRE(result);

  result= (sample_date <= before_sample_date);
  BOOST_REQUIRE(not result);
}


BOOST_FIXTURE_TEST_CASE(operatorComparingTimestamp, TimestampTestCompareOperators)
{
  result= (sample_date == identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date == before_sample_date);  
  BOOST_REQUIRE(not result);

  result= (sample_date != identical_with_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date != before_sample_date);  
  BOOST_REQUIRE(result);

  result= (sample_date > identical_with_sample_date);
  BOOST_REQUIRE(not this->result);

  result= (sample_date > after_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date > before_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date >= identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date >= after_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date >= before_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date < identical_with_sample_date);
  
  BOOST_REQUIRE(not result);

  result= (sample_date < after_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date < before_sample_date);
  BOOST_REQUIRE(not result);

  result= (sample_date <= identical_with_sample_date);
  BOOST_REQUIRE(result);

  result= (sample_date <= after_sample_date);
  
  BOOST_REQUIRE(result);

  result= (sample_date <= before_sample_date);
  BOOST_REQUIRE(not result);
}
BOOST_AUTO_TEST_SUITE_END()


class DateTest
{
  protected:
    Date date;
    bool result;
    
    DateTest()
    {
      TemporalGenerator::DateGen::make_valid_date(&date);
    }
};

BOOST_AUTO_TEST_SUITE(CurrentDateValidationTest)
BOOST_FIXTURE_TEST_CASE(operatorAssign_shouldCopyDateRelatadComponents, DateTest)
{
  Date copy= date;

  BOOST_REQUIRE_EQUAL(date.years(), copy.years());
  BOOST_REQUIRE_EQUAL(date.months(), copy.months());
  BOOST_REQUIRE_EQUAL(date.days(), copy.days());
}

BOOST_FIXTURE_TEST_CASE(is_valid_onValidDate_shouldReturn_True, DateTest)
{
  result= date.is_valid();
  BOOST_REQUIRE(result);
}

BOOST_FIXTURE_TEST_CASE(is_valid_onInvalidDateWithYearBelowMinimum_shouldReturn_False, DateTest)
{
  date.set_years(DRIZZLE_MIN_YEARS_SQL - 1);
  
  result= date.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(is_valid_onInvalidDateWithYearAboveMaximum_shouldReturn_False, DateTest)
{
  date.set_years(DRIZZLE_MAX_YEARS_SQL + 1);
    
  result= date.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(is_valid_onInvalidDateWithMonthSetToZero_shouldReturn_False, DateTest)
{
  date.set_months(0);
  
  result= date.is_valid();
  
  BOOST_REQUIRE(not result);
}


BOOST_FIXTURE_TEST_CASE(is_valid_onInvalidDateWithMonthAboveMaximum_shouldReturn_False, DateTest)
{
  date.set_months(13);
  
  result= date.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(is_valid_onInvalidDateWithDaySetToZero_shouldReturn_False, DateTest)
{
  date.set_days(0);
  
  result= date.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(is_valid_onInvalidDateWithDayAboveDaysInMonth_shouldReturn_False, DateTest)
{
  date.set_days(32);
  
  result= date.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(is_valid_onInvalidDateWithLeapDayInNonLeapYear_shouldReturn_False, DateTest)
{
  TemporalGenerator::TemporalGen::leap_day_in_non_leap_year(&date);
  
  result= date.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(is_valid_onValidDateWithLeapDayInLeapYear_shouldReturn_True, DateTest)
{
  TemporalGenerator::TemporalGen::leap_day_in_leap_year(&date);
  
  result= date.is_valid();
  
  BOOST_REQUIRE(result);
}

BOOST_FIXTURE_TEST_CASE(to_string_shouldProduce_hyphenSeperatedDateElements, DateTest)
{
  char expected[Date::MAX_STRING_LENGTH]= "2010-05-01";
  char returned[Date::MAX_STRING_LENGTH];
  TemporalGenerator::DateGen::make_date(&date, 2010, 5, 1);
  
  date.to_string(returned, Date::MAX_STRING_LENGTH);
  
  BOOST_REQUIRE_EQUAL(expected, returned);
}

BOOST_FIXTURE_TEST_CASE(to_string_nullBuffer_shouldReturnProperLengthAnyway, DateTest)
{
  int length= date.to_string(NULL, 0);
  
  BOOST_REQUIRE_EQUAL(Date::MAX_STRING_LENGTH - 1, length);  
}

BOOST_FIXTURE_TEST_CASE(from_string_validString_shouldPopulateCorrectly, DateTest)
{
  char valid_string[Date::MAX_STRING_LENGTH]= "2010-05-01";
  uint32_t years, months, days;

  init_temporal_formats();
  
  result= date.from_string(valid_string, Date::MAX_STRING_LENGTH - 1);
  BOOST_REQUIRE(result);
  
  years= date.years();
  months= date.months();
  days= date.days();

  deinit_temporal_formats();
  
  BOOST_REQUIRE_EQUAL(2010, years);
  BOOST_REQUIRE_EQUAL(5, months);
  BOOST_REQUIRE_EQUAL(1, days);
}

BOOST_FIXTURE_TEST_CASE(from_string_invalidString_shouldReturn_False, DateTest)
{
  char valid_string[Date::MAX_STRING_LENGTH]= "2x10-05-01";

  init_temporal_formats();
  result= date.from_string(valid_string, Date::MAX_STRING_LENGTH - 1);
  deinit_temporal_formats();
  
  BOOST_REQUIRE(not result);
}

BOOST_FIXTURE_TEST_CASE(to_int64_t, DateTest)
{
  TemporalGenerator::DateGen::make_date(&date, 2030, 8, 17);
  int64_t representation;
  
  date.to_int64_t(&representation);
  
  BOOST_REQUIRE_EQUAL(20300817, representation);
}

BOOST_FIXTURE_TEST_CASE(to_int32_t, DateTest)
{
  TemporalGenerator::DateGen::make_date(&date, 2030, 8, 17);
  int32_t representation;

  date.to_int32_t(&representation);

  BOOST_REQUIRE_EQUAL(20300817, representation);
}

BOOST_FIXTURE_TEST_CASE(from_int32_t_shouldPopulateDateCorrectly, DateTest)
{
  uint32_t decoded_years, decoded_months, decoded_days;

  date.from_int32_t(20300817);
  
  decoded_years= date.years();
  decoded_months= date.months();
  decoded_days= date.days();
  
  BOOST_REQUIRE_EQUAL(2030, decoded_years);
  BOOST_REQUIRE_EQUAL(8, decoded_months);
  BOOST_REQUIRE_EQUAL(17, decoded_days);
}

BOOST_FIXTURE_TEST_CASE(to_julian_day_number, DateTest)
{
  int64_t julian_day;
  TemporalGenerator::DateGen::make_date(&date, 1999, 12, 31);
  
  date.to_julian_day_number(&julian_day);
  
  BOOST_REQUIRE_EQUAL(2451544, julian_day);
}

BOOST_FIXTURE_TEST_CASE(from_julian_day_number, DateTest)
{
  int64_t julian_day= 2451544;
  uint32_t years, months, days;
   
  date.from_julian_day_number(julian_day);
  
  years= date.years();
  months= date.months();
  days= date.days();
    
  BOOST_REQUIRE_EQUAL(1999, years);
  BOOST_REQUIRE_EQUAL(12, months);
  BOOST_REQUIRE_EQUAL(31, days);
}

BOOST_FIXTURE_TEST_CASE(to_tm, DateTest)
{
  uint32_t years= 2030, months= 8, days= 17;
  TemporalGenerator::DateGen::make_date(&date, years, months, days);
  struct tm filled;
  
  date.to_tm(&filled);
  
  BOOST_REQUIRE_EQUAL(130, filled.tm_year);
  BOOST_REQUIRE_EQUAL(7, filled.tm_mon);
  BOOST_REQUIRE_EQUAL(17, filled.tm_mday);
  BOOST_REQUIRE_EQUAL(0, filled.tm_hour);
  BOOST_REQUIRE_EQUAL(0, filled.tm_min);
  BOOST_REQUIRE_EQUAL(0, filled.tm_sec);

}

BOOST_FIXTURE_TEST_CASE(from_tm, DateTest)
{
  uint32_t years, months, days;
  struct tm from;
  from.tm_year= 1956 - 1900;
  from.tm_mon= 2;
  from.tm_mday= 30;
  
  date.from_tm(&from);
  
  years= date.years();
  months= date.months();
  days= date.days();
  
  BOOST_REQUIRE_EQUAL(1956, years);  
  BOOST_REQUIRE_EQUAL(3, months);
  BOOST_REQUIRE_EQUAL(30, days);
}

BOOST_FIXTURE_TEST_CASE(to_time_t, DateTest)
{
  time_t time;
  TemporalGenerator::DateGen::make_date(&date, 1990, 9, 9);
  
  date.to_time_t(time);
  
  BOOST_REQUIRE_EQUAL(652838400, time);
}

BOOST_FIXTURE_TEST_CASE(from_time_t, DateTest)
{
  uint32_t years, months, days;
  
  date.from_time_t(652838400);
  
  years= date.years();
  months= date.months();
  days= date.days();
  
  BOOST_REQUIRE_EQUAL(1990, years);  
  BOOST_REQUIRE_EQUAL(9, months);
  BOOST_REQUIRE_EQUAL(9, days);
}

BOOST_FIXTURE_TEST_CASE(to_decimal, DateTest)
{
  drizzled::type::Decimal to;
  TemporalGenerator::DateGen::make_date(&date, 1987, 5, 6);

  date.to_decimal(&to);

  BOOST_REQUIRE_EQUAL(19870506, to.buf[0]);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(DateStringTest)
BOOST_AUTO_TEST_CASE(DateFromStringTest)
{
  Date date;
  const char *valid_strings[]= {"20100607", /* YYYYMMDD */
                               "06/07/2010",/* MM[-/.]DD[-/.]YYYY (US common format)*/
                               "10.06.07",/* YY[-/.]MM[-/.]DD */
                               "10/6/7",/* YY[-/.][M]M[-/.][D]D */
                               "2010-6-7"/* YYYY[-/.][M]M[-/.][D]D */};

  init_temporal_formats();
  for (int it= 0; it < 5; it++)
  {
    const char *valid_string= valid_strings[it];
    bool result= date.from_string(valid_string, strlen(valid_string));
    BOOST_REQUIRE(result);
    
    BOOST_REQUIRE_EQUAL(2010, date.years());
    BOOST_REQUIRE_EQUAL(6, date.months());
    BOOST_REQUIRE_EQUAL(7, date.days());
  }
  deinit_temporal_formats();
}
BOOST_AUTO_TEST_SUITE_END()
