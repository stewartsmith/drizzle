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

class DateTimeTest
{
  protected:
    DateTime datetime;
    bool result;
    uint32_t years, months, days;
    uint32_t hours, minutes, seconds;
    
    DateTimeTest()
    {
      TemporalGenerator::DateTimeGen::make_valid_datetime(&datetime);
    }

    void assignDateTimeValues()
    {
      years= datetime.years();
      months= datetime.months();
      days= datetime.days();
      hours= datetime.hours();
      minutes= datetime.minutes();
      seconds= datetime.seconds();
    }
};

BOOST_FIXTURE_TEST_SUITE(DateTimeTestValidation, DateTimeTest)
BOOST_AUTO_TEST_CASE(is_valid_onValidDateTime_shouldReturn_True)
{
  result= datetime.is_valid();
  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithYearBelowMinimum_shouldReturn_False)
{
  datetime.set_years(DRIZZLE_MIN_YEARS_SQL - 1);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithYearAboveMaximum_shouldReturn_False)
{
  datetime.set_years(DRIZZLE_MAX_YEARS_SQL + 1);
    
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithMonthSetToZero_shouldReturn_False)
{
  datetime.set_months(0);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}


BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithMonthAboveMaximum_shouldReturn_False)
{
  datetime.set_months(13);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithDaySetToZero_shouldReturn_False)
{
  datetime.set_days(0);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithDayAboveDaysInMonth_shouldReturn_False)
{
  datetime.set_days(32);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithLeapDayInNonLeapYear_shouldReturn_False)
{
  TemporalGenerator::TemporalGen::leap_day_in_non_leap_year(&datetime);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onValidDateTimeWithLeapDayInLeapYear_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::leap_day_in_leap_year(&datetime);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_onValidMinimalTime_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::make_min_time(&datetime);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_onValidMaximalTime_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::make_max_time(&datetime);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithHourAboveMaximum23_shouldReturn_False)
{
  datetime.set_hours(24);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithMinutesAboveMaximum59_shouldReturn_False)
{
  datetime.set_minutes(60);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(is_valid_onInvalidDateTimeWithSecondsAboveMaximum61_shouldReturn_False)
{
  datetime.set_seconds(62);
  
  result= datetime.is_valid();
  
  BOOST_REQUIRE(not result);
}

BOOST_AUTO_TEST_CASE(to_string_shouldProduce_hyphenSeperatedDateElements_and_colonSeperatedTimeElements)
{
  char expected[DateTime::MAX_STRING_LENGTH]= "2010-05-01 08:07:06.123456";
  char returned[DateTime::MAX_STRING_LENGTH];
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, 2010, 5, 1, 8, 7, 6, 123456);
  
  datetime.to_string(returned, DateTime::MAX_STRING_LENGTH);
  
  BOOST_REQUIRE_EQUAL(expected, returned);
}

BOOST_AUTO_TEST_CASE(to_string_nullBuffer_noMicroSeconds_shouldReturnProperLengthAnyway)
{
  int length= datetime.to_string(NULL, 0);
  
  BOOST_REQUIRE_EQUAL(DateTime::MAX_STRING_LENGTH - 1 - 7, length);  
}

BOOST_AUTO_TEST_CASE(to_int64_t)
{
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, 2030, 8, 7, 14, 5, 13);
  int64_t representation;

  datetime.to_int64_t(&representation);

  BOOST_REQUIRE_EQUAL(20300807140513LL, representation);
}

BOOST_AUTO_TEST_CASE(from_int64_t_no_conversion_format_YYYYMMDDHHMMSSshouldPopulateDateTimeCorrectly)
{
  datetime.from_int64_t(20300807140513LL, false);
  
  assignDateTimeValues();
  
  BOOST_REQUIRE_EQUAL(2030, years);
  BOOST_REQUIRE_EQUAL(8, months);
  BOOST_REQUIRE_EQUAL(7, days);
  BOOST_REQUIRE_EQUAL(14, hours);
  BOOST_REQUIRE_EQUAL(5, minutes);
  BOOST_REQUIRE_EQUAL(13, seconds);
}

BOOST_AUTO_TEST_CASE(from_int64_t_with_conversion_format_YYYYMMDDHHMMSS_yearOver2000)
{
  datetime.from_int64_t(20300807140513LL, true);
  
  assignDateTimeValues();
  
  BOOST_REQUIRE_EQUAL(2030, years);
  BOOST_REQUIRE_EQUAL(8, months);
  BOOST_REQUIRE_EQUAL(7, days);
  BOOST_REQUIRE_EQUAL(14, hours);
  BOOST_REQUIRE_EQUAL(5, minutes);
  BOOST_REQUIRE_EQUAL(13, seconds);
}

BOOST_AUTO_TEST_CASE(from_int64_t_with_conversion_format_YYYYMMDDHHMMSS_yearBelow2000)
{
  datetime.from_int64_t(19900807140513LL, true);
  
  assignDateTimeValues();
  
  BOOST_REQUIRE_EQUAL(1990, years);
  BOOST_REQUIRE_EQUAL(8, months);
  BOOST_REQUIRE_EQUAL(7, days);
  BOOST_REQUIRE_EQUAL(14, hours);
  BOOST_REQUIRE_EQUAL(5, minutes);
  BOOST_REQUIRE_EQUAL(13, seconds);
}

BOOST_AUTO_TEST_CASE(from_int64_t_with_conversion_format_YYMMDDHHMMSS_yearOver2000)
{
  datetime.from_int64_t(300807140513LL, true);
  
  assignDateTimeValues();
  
  BOOST_REQUIRE_EQUAL(2030, years);
  BOOST_REQUIRE_EQUAL(8, months);
  BOOST_REQUIRE_EQUAL(7, days);
  BOOST_REQUIRE_EQUAL(14, hours);
  BOOST_REQUIRE_EQUAL(5, minutes);
  BOOST_REQUIRE_EQUAL(13, seconds);
}

BOOST_AUTO_TEST_CASE(from_int64_t_with_conversion_format_YYMMDDHHMMSS_yearBelow2000)
{
  datetime.from_int64_t(900807140513LL, true);
  
  assignDateTimeValues();
  
  BOOST_REQUIRE_EQUAL(1990, years);
  BOOST_REQUIRE_EQUAL(8, months);
  BOOST_REQUIRE_EQUAL(7, days);
  BOOST_REQUIRE_EQUAL(14, hours);
  BOOST_REQUIRE_EQUAL(5, minutes);
  BOOST_REQUIRE_EQUAL(13, seconds);
}

BOOST_AUTO_TEST_CASE(to_tm)
{
  years= 2030, months= 8, days= 17, hours= 14, minutes= 45, seconds= 13;
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, years, months, days, hours, minutes, seconds);
  struct tm filled;
  
  datetime.to_tm(&filled);
  
  BOOST_REQUIRE_EQUAL(2030 - 1900, filled.tm_year);
  BOOST_REQUIRE_EQUAL(8 - 1, filled.tm_mon);
  BOOST_REQUIRE_EQUAL(17, filled.tm_mday);
  BOOST_REQUIRE_EQUAL(14, filled.tm_hour);
  BOOST_REQUIRE_EQUAL(45, filled.tm_min);
  BOOST_REQUIRE_EQUAL(13, filled.tm_sec);

}

BOOST_AUTO_TEST_CASE(to_decimal)
{
  drizzled::type::Decimal to;
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, 1987, 6, 13, 5, 10, 13, 456);

  datetime.to_decimal(&to);
  
  BOOST_REQUIRE_EQUAL(19870,to.buf[0]);
  BOOST_REQUIRE_EQUAL(613051013,to.buf[1]);
  BOOST_REQUIRE_EQUAL(456000,to.buf[2]);
}
BOOST_AUTO_TEST_SUITE_END()


class DateTimeFromStringTest
{
  protected:
    static const char* allStrings[];
    DateTime datetime;
    bool result;
    uint32_t years, months, days;
    uint32_t hours, minutes, seconds;

    DateTimeFromStringTest()
    {
      init_temporal_formats();
    }

    virtual ~DateTimeFromStringTest()
    {
      deinit_temporal_formats();
    }

    virtual int stringCount() { return 0; }
    virtual const char** strings() { return NULL; }

    void assignDateTimeValues()
    {
      years= datetime.years();
      months= datetime.months();
      days= datetime.days();
      hours= datetime.hours();
      minutes= datetime.minutes();
      seconds= datetime.seconds();
    }
};

const char* DateTimeFromStringTest::allStrings[]= {"NULL"};


class DateTimeFromStringFullFormatTest : public DateTimeFromStringTest
{
 protected:
  static const char* allStrings[];

  const char** strings()
  {
    return allStrings;
  }
  
  int stringCount()
  {
    return 7;
  }  
};

const char* DateTimeFromStringFullFormatTest::allStrings[]= {"20100501080706",
            "2010-05-01 08:07:06",
            "2010/05/01T08:07:06",
            "2010.5.1 08:07:06",
            "10-05-01 08:07:06",
            "10/5/1 08:07:06",
            "10.5.1 08:07:06"};

class DateTimeFromStringNoSecondFormatTest : public DateTimeFromStringTest
{
 protected:
  static const char* allStrings[];

  const char** strings()
  {
    return allStrings;
  }
  
  int stringCount()
  {
    return 6;
  }
};

const char* DateTimeFromStringNoSecondFormatTest::allStrings[]= {"2010-05-01 08:07",
            "2010/05/01 08:07",
            "2010.5.1 08:07",
            "10-05-01 08:07",
            "10/5/1 08:07",
            "10.5.1 08:07"};

class DateTimeFromStringDateOnlyTest: public DateTimeFromStringTest
{
 protected:
  static const char* allStrings[];

  const char** strings()
  {
    return allStrings;
  }
  
  int stringCount()
  {
    return 5;
  }
};

const char* DateTimeFromStringDateOnlyTest::allStrings[]= {"20100607", /* YYYYMMDD */
            "06/07/2010",/* MM[-/.]DD[-/.]YYYY (US common format)*/
            "10.06.07",/* YY[-/.]MM[-/.]DD */
            "10/6/7",/* YY[-/.][M]M[-/.][D]D */
            "2010-6-7"/* YYYY[-/.][M]M[-/.][D]D */};

BOOST_AUTO_TEST_SUITE(DateTimeFromStringTestSuite)
BOOST_FIXTURE_TEST_CASE(from_string_validStringFull, DateTimeFromStringFullFormatTest)
{
  for (int it= 0; it < stringCount(); it++)
  {
    const char *valid_string= strings()[it];

    result= datetime.from_string(valid_string, strlen(valid_string));
    BOOST_REQUIRE(result);

    assignDateTimeValues();

    BOOST_REQUIRE_EQUAL(2010, years);
    BOOST_REQUIRE_EQUAL(5, months);
    BOOST_REQUIRE_EQUAL(1, days);
    BOOST_REQUIRE_EQUAL(8, hours);
    BOOST_REQUIRE_EQUAL(7, minutes);
    BOOST_REQUIRE_EQUAL(6, seconds);
  }
}
                                          
BOOST_FIXTURE_TEST_CASE(from_string_validStringNoSecond, DateTimeFromStringNoSecondFormatTest)
{
  for (int it= 0; it < stringCount(); it++)
  {
    const char *valid_string= strings()[it];

    result= datetime.from_string(valid_string, strlen(valid_string));
    BOOST_REQUIRE(result);

    assignDateTimeValues();

    BOOST_REQUIRE_EQUAL(2010, years);
    BOOST_REQUIRE_EQUAL(5, months);
    BOOST_REQUIRE_EQUAL(1, days);
    BOOST_REQUIRE_EQUAL(8, hours);
    BOOST_REQUIRE_EQUAL(7, minutes);
  }
}

BOOST_FIXTURE_TEST_CASE(from_string_validStringDateOnly, DateTimeFromStringDateOnlyTest)
{
  for (int it= 0; it < stringCount(); it++)
  {
    const char *valid_string= strings()[it];

    result= datetime.from_string(valid_string, strlen(valid_string));
    BOOST_REQUIRE(result);

    assignDateTimeValues();

    BOOST_REQUIRE_EQUAL(2010, years);
    BOOST_REQUIRE_EQUAL(6, months);
    BOOST_REQUIRE_EQUAL(7, days);
  }
}

BOOST_AUTO_TEST_SUITE_END()
