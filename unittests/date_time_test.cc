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
#include <drizzled/temporal.h>
#include <drizzled/temporal_format.h>

#include "generator.h"

using namespace drizzled;

class DateTimeTest: public ::testing::Test
{
  protected:
    DateTime datetime;
    bool result;
    
    virtual void SetUp()
    {
      Generator::DateTimeGen::make_valid_datetime(&datetime);
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
  Generator::TemporalGen::leap_day_in_non_leap_year(&datetime);
  
  result= datetime.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTimeTest, is_valid_onValidDateTimeWithLeapDayInLeapYear_shouldReturn_True)
{
  Generator::TemporalGen::leap_day_in_leap_year(&datetime);
  
  result= datetime.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(DateTimeTest, is_valid_onValidMinimalTime_shouldReturn_True)
{
  Generator::TemporalGen::make_min_time(&datetime);
  
  result= datetime.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(DateTimeTest, is_valid_onValidMaximalTime_shouldReturn_True)
{
  Generator::TemporalGen::make_max_time(&datetime);
  
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
  char expected[DateTime::MAX_STRING_LENGTH]= "2010-05-01 08:07:06";
  char returned[DateTime::MAX_STRING_LENGTH];
  Generator::DateTimeGen::make_datetime(&datetime, 2010, 5, 1, 8, 7, 6);
  
  datetime.to_string(returned, DateTime::MAX_STRING_LENGTH);
  
  ASSERT_STREQ(expected, returned);  
}

TEST_F(DateTimeTest, to_string_nullBuffer_noMicroSeconds_shouldReturnProperLengthAnyway)
{
  int length= datetime.to_string(NULL, 0);
  
  ASSERT_EQ(DateTime::MAX_STRING_LENGTH - 1 - 7, length);  
}

TEST_F(DateTimeTest, from_string_validString_shouldPopulateCorrectly)
{
  char valid_string[DateTime::MAX_STRING_LENGTH]= "2010-05-01 08:07:06";
  uint32_t years, months, days, hours, minutes, seconds;

  init_temporal_formats();
  
  result = datetime.from_string(valid_string, DateTime::MAX_STRING_LENGTH - 1);
  ASSERT_TRUE(result);
  
  years = datetime.years();
  months = datetime.months();
  days = datetime.days();
  hours = datetime.hours();
  minutes = datetime.minutes();
  seconds = datetime.seconds();

  deinit_temporal_formats();
  
  EXPECT_EQ(2010, years);
  EXPECT_EQ(5, months);
  EXPECT_EQ(1, days);
  EXPECT_EQ(8, hours);
  EXPECT_EQ(7, minutes);
  EXPECT_EQ(6, seconds);
}

TEST_F(DateTimeTest, from_int32_t_onValueCreatedBy_to_int32_t_shouldProduceOriginalDate)
{
  uint32_t years = 2030, months = 8, days = 17, hours = 14, minutes = 45, seconds = 13;
  Generator::DateTimeGen::make_datetime(&datetime, years, months, days, hours, minutes, seconds);
  uint32_t decoded_years, decoded_months, decoded_days;
  uint32_t decoded_hours, decoded_minutes, decoded_seconds;
  int64_t representation;
  DateTime decoded_datetime;
  
  datetime.to_int64_t(&representation);
  decoded_datetime.from_int64_t(representation);
  
  decoded_years = decoded_datetime.years();
  decoded_months = decoded_datetime.months();
  decoded_days = decoded_datetime.days();
  decoded_hours = decoded_datetime.hours();
  decoded_minutes = decoded_datetime.minutes();
  decoded_seconds = decoded_datetime.seconds();
  
  EXPECT_EQ(years, decoded_years);
  EXPECT_EQ(months, decoded_months);
  EXPECT_EQ(days, decoded_days);
  EXPECT_EQ(hours, decoded_hours);
  EXPECT_EQ(minutes, decoded_minutes);
  EXPECT_EQ(seconds, decoded_seconds);
}

TEST_F(DateTimeTest, DISABLED_to_tm)
{
  uint32_t years = 2030, months = 8, days = 17, hours = 14, minutes = 45, seconds = 13;
  Generator::DateTimeGen::make_datetime(&datetime, years, months, days, hours, minutes, seconds);
  struct tm filled;
  
  datetime.to_tm(&filled);
  
  EXPECT_EQ(2030 - 1900, filled.tm_year);
  EXPECT_EQ(8 - 1, filled.tm_mon);
  EXPECT_EQ(17, filled.tm_mday);
  EXPECT_EQ(14, filled.tm_hour);
  EXPECT_EQ(45, filled.tm_min);
  EXPECT_EQ(13, filled.tm_sec);

  /* these fail, shouldn't they also be set properly? */
  EXPECT_EQ(228, filled.tm_yday);
  EXPECT_EQ(6, filled.tm_wday);
  EXPECT_EQ(-1, filled.tm_isdst);
}
