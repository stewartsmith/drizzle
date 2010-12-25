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

#include "config.h"

#include <gtest/gtest.h>
#include <drizzled/decimal.h>
#include <drizzled/temporal.h>
#include <drizzled/temporal_format.h>

#include "temporal_generator.h"

using namespace drizzled;

class DateTimeTest: public ::testing::Test
{
  protected:
    DateTime datetime;
    bool result;
    uint32_t years, months, days;
    uint32_t hours, minutes, seconds;
    
    virtual void SetUp()
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

TEST_F(DateTimeTest, is_valid_onValidDateTime_shouldReturn_True)
{
  result= datetime.is_valid();
  ASSERT_TRUE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithYearBelowMinimum_shouldReturn_False)
{
  datetime.set_years(DRIZZLE_MIN_YEARS_SQL - 1);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithYearAboveMaximum_shouldReturn_False)
{
  datetime.set_years(DRIZZLE_MAX_YEARS_SQL + 1);
    
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithMonthSetToZero_shouldReturn_False)
{
  datetime.set_months(0);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}


TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithMonthAboveMaximum_shouldReturn_False)
{
  datetime.set_months(13);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithDaySetToZero_shouldReturn_False)
{
  datetime.set_days(0);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithDayAboveDaysInMonth_shouldReturn_False)
{
  datetime.set_days(32);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithLeapDayInNonLeapYear_shouldReturn_False)
{
  TemporalGenerator::TemporalGen::leap_day_in_non_leap_year(&datetime);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onValidDateTimeWithLeapDayInLeapYear_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::leap_day_in_leap_year(&datetime);
  
  result= datetime.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(DateTimeTest, is_valid_onValidMinimalTime_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::make_min_time(&datetime);
  
  result= datetime.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(DateTimeTest, is_valid_onValidMaximalTime_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::make_max_time(&datetime);
  
  result= datetime.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithHourAboveMaximum23_shouldReturn_False)
{
  datetime.set_hours(24);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithMinutesAboveMaximum59_shouldReturn_False)
{
  datetime.set_minutes(60);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onInvalidDateTimeWithSecondsAboveMaximum61_shouldReturn_False)
{
  datetime.set_seconds(62);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, to_string_shouldProduce_hyphenSeperatedDateElements_and_colonSeperatedTimeElements)
{
  char expected[DateTime::MAX_STRING_LENGTH]= "2010-05-01 08:07:06.123456";
  char returned[DateTime::MAX_STRING_LENGTH];
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, 2010, 5, 1, 8, 7, 6, 123456);
  
  datetime.to_string(returned, DateTime::MAX_STRING_LENGTH);
  
  ASSERT_STREQ(expected, returned);
}

TEST_F(DateTimeTest, to_string_nullBuffer_noMicroSeconds_shouldReturnProperLengthAnyway)
{
  int length= datetime.to_string(NULL, 0);
  
  ASSERT_EQ(DateTime::MAX_STRING_LENGTH - 1 - 7, length);  
}

TEST_F(DateTimeTest, to_int64_t)
{
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, 2030, 8, 7, 14, 5, 13);
  int64_t representation;

  datetime.to_int64_t(&representation);

  ASSERT_EQ(20300807140513LL, representation);
}

TEST_F(DateTimeTest, from_int64_t_no_conversion_format_YYYYMMDDHHMMSSshouldPopulateDateTimeCorrectly)
{
  datetime.from_int64_t(20300807140513LL, false);
  
  assignDateTimeValues();
  
  EXPECT_EQ(2030, years);
  EXPECT_EQ(8, months);
  EXPECT_EQ(7, days);
  EXPECT_EQ(14, hours);
  EXPECT_EQ(5, minutes);
  EXPECT_EQ(13, seconds);
}

TEST_F(DateTimeTest, from_int64_t_with_conversion_format_YYYYMMDDHHMMSS_yearOver2000)
{
  datetime.from_int64_t(20300807140513LL, true);
  
  assignDateTimeValues();
  
  EXPECT_EQ(2030, years);
  EXPECT_EQ(8, months);
  EXPECT_EQ(7, days);
  EXPECT_EQ(14, hours);
  EXPECT_EQ(5, minutes);
  EXPECT_EQ(13, seconds);
}

TEST_F(DateTimeTest, from_int64_t_with_conversion_format_YYYYMMDDHHMMSS_yearBelow2000)
{
  datetime.from_int64_t(19900807140513LL, true);
  
  assignDateTimeValues();
  
  EXPECT_EQ(1990, years);
  EXPECT_EQ(8, months);
  EXPECT_EQ(7, days);
  EXPECT_EQ(14, hours);
  EXPECT_EQ(5, minutes);
  EXPECT_EQ(13, seconds);
}

TEST_F(DateTimeTest, from_int64_t_with_conversion_format_YYMMDDHHMMSS_yearOver2000)
{
  datetime.from_int64_t(300807140513LL, true);
  
  assignDateTimeValues();
  
  EXPECT_EQ(2030, years);
  EXPECT_EQ(8, months);
  EXPECT_EQ(7, days);
  EXPECT_EQ(14, hours);
  EXPECT_EQ(5, minutes);
  EXPECT_EQ(13, seconds);
}

TEST_F(DateTimeTest, from_int64_t_with_conversion_format_YYMMDDHHMMSS_yearBelow2000)
{
  datetime.from_int64_t(900807140513LL, true);
  
  assignDateTimeValues();
  
  EXPECT_EQ(1990, years);
  EXPECT_EQ(8, months);
  EXPECT_EQ(7, days);
  EXPECT_EQ(14, hours);
  EXPECT_EQ(5, minutes);
  EXPECT_EQ(13, seconds);
}

TEST_F(DateTimeTest, DISABLED_to_tm)
{
  years= 2030, months= 8, days= 17, hours= 14, minutes= 45, seconds= 13;
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, years, months, days, hours, minutes, seconds);
  struct tm filled;
  
  datetime.to_tm(&filled);
  
  EXPECT_EQ(2030 - 1900, filled.tm_year);
  EXPECT_EQ(8 - 1, filled.tm_mon);
  EXPECT_EQ(17, filled.tm_mday);
  EXPECT_EQ(14, filled.tm_hour);
  EXPECT_EQ(45, filled.tm_min);
  EXPECT_EQ(13, filled.tm_sec);

  /* TODO:these fail, shouldn't they also be set properly? */
  EXPECT_EQ(228, filled.tm_yday);
  EXPECT_EQ(6, filled.tm_wday);
  EXPECT_EQ(-1, filled.tm_isdst);
}

TEST_F(DateTimeTest, to_decimal)
{
  drizzled::type::Decimal to;
  TemporalGenerator::DateTimeGen::make_datetime(&datetime, 1987, 6, 13, 5, 10, 13, 456);

  datetime.to_decimal(&to);
  
  EXPECT_EQ(19870,to.buf[0]);
  EXPECT_EQ(613051013,to.buf[1]);
  EXPECT_EQ(456000,to.buf[2]);
}



class DateTimeFromStringTest
{
  protected:
    DateTime datetime;
    bool result;
    uint32_t years, months, days;
    uint32_t hours, minutes, seconds;

    void init()
    {
      init_temporal_formats();
    }

    void deinit()
    {
      deinit_temporal_formats();
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

class DateTimeFromStringFullFormatTest: public ::testing::TestWithParam<const char*>, public DateTimeFromStringTest
{
  virtual void SetUp()
  {
    init();
  }
  
  virtual void TearDown()
  {
    deinit();
  }  
};

class DateTimeFromStringNoSecondFormatTest: public ::testing::TestWithParam<const char*>, public DateTimeFromStringTest
{
  virtual void SetUp()
  {
    init();
  }
  
  virtual void TearDown()
  {
    deinit();
  }
};

class DateTimeFromStringDateOnlyTest: public ::testing::TestWithParam<const char*>, public DateTimeFromStringTest
{
  virtual void SetUp()
  {
    init();
  }
  
  virtual void TearDown()
  {
    deinit();
  }
};

TEST_P(DateTimeFromStringFullFormatTest, from_string_validString)
{
  const char *valid_string= GetParam();

  result= datetime.from_string(valid_string, strlen(valid_string));
  ASSERT_TRUE(result);

  assignDateTimeValues();

  EXPECT_EQ(2010, years);
  EXPECT_EQ(5, months);
  EXPECT_EQ(1, days);
  EXPECT_EQ(8, hours);
  EXPECT_EQ(7, minutes);
  EXPECT_EQ(6, seconds);
}
/* TODO:for some reason this was not declared by the macro, needs clarification*/
testing::internal::ParamGenerator<const char*> gtest_ValidStringDateTimeFromStringFullFormatTest_EvalGenerator_();
INSTANTIATE_TEST_CASE_P(ValidString, DateTimeFromStringFullFormatTest,
                        ::testing::Values("20100501080706",
                                          "2010-05-01 08:07:06",
                                          "2010/05/01T08:07:06",
                                          "2010.5.1 08:07:06",
                                          "10-05-01 08:07:06",
                                          "10/5/1 08:07:06",
                                          "10.5.1 08:07:06"));

                                          
TEST_P(DateTimeFromStringNoSecondFormatTest, from_string_validString)
{
  const char *valid_string= GetParam();

  result= datetime.from_string(valid_string, strlen(valid_string));
  ASSERT_TRUE(result);

  assignDateTimeValues();

  EXPECT_EQ(2010, years);
  EXPECT_EQ(5, months);
  EXPECT_EQ(1, days);
  EXPECT_EQ(8, hours);
  EXPECT_EQ(7, minutes);
}

/* TODO:for some reason this was not declared by the macro, needs clarification*/
testing::internal::ParamGenerator<const char*> gtest_ValidStringDateTimeFromStringNoSecondFormatTest_EvalGenerator_();
INSTANTIATE_TEST_CASE_P(ValidString, DateTimeFromStringNoSecondFormatTest,
                        ::testing::Values("2010-05-01 08:07",
                                          "2010/05/01 08:07",
                                          "2010.5.1 08:07",
                                          "10-05-01 08:07",
                                          "10/5/1 08:07",
                                          "10.5.1 08:07"));



TEST_P(DateTimeFromStringDateOnlyTest, from_string_validString)
{
  const char *valid_string= GetParam();

  result= datetime.from_string(valid_string, strlen(valid_string));
  ASSERT_TRUE(result);

  assignDateTimeValues();

  EXPECT_EQ(2010, years);
  EXPECT_EQ(6, months);
  EXPECT_EQ(7, days);
}

/* TODO:for some reason this was not declared by the macro, needs clarification*/
testing::internal::ParamGenerator<const char*> gtest_ValidStringDateTimeFromStringDateOnlyTest_EvalGenerator_();
INSTANTIATE_TEST_CASE_P(ValidString, DateTimeFromStringDateOnlyTest,
                        ::testing::Values("20100607", /* YYYYMMDD */
                                          "06/07/2010",/* MM[-/.]DD[-/.]YYYY (US common format)*/
                                          "10.06.07",/* YY[-/.]MM[-/.]DD */
                                          "10/6/7",/* YY[-/.][M]M[-/.][D]D */
                                          "2010-6-7"/* YYYY[-/.][M]M[-/.][D]D */));

                                          