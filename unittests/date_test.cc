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

template <typename TemporalType>
class DateTestCompareOperators : public ::testing::Test
{
 protected:
  Date sample_date;
  bool result;
  
  TemporalType identical_with_sample_date, before_sample_date, after_sample_date;
  
  void initBeforeIdenticalAfter();

  virtual void SetUp()
  {
    TemporalGenerator::DateGen::make_date(&sample_date, 2010, 9, 8);
    initBeforeIdenticalAfter();
  }
};

template<> void DateTestCompareOperators<Date>::initBeforeIdenticalAfter()
{
  TemporalGenerator::DateGen::make_date(&before_sample_date, 1980, 1, 1);
  TemporalGenerator::DateGen::make_date(&identical_with_sample_date, 2010, 9, 8);
  TemporalGenerator::DateGen::make_date(&after_sample_date, 2019, 5, 30);
}

template<> void DateTestCompareOperators<DateTime>::initBeforeIdenticalAfter()
{
  TemporalGenerator::DateTimeGen::make_datetime(&before_sample_date, 1990, 12, 31, 12, 12, 30);
  TemporalGenerator::DateTimeGen::make_datetime(&identical_with_sample_date, 2010, 9, 8, 0, 0, 0);
  TemporalGenerator::DateTimeGen::make_datetime(&after_sample_date, 2020, 4, 4, 4, 4, 4);
}

template<> void DateTestCompareOperators<Timestamp>::initBeforeIdenticalAfter()
{
  TemporalGenerator::TimestampGen::make_timestamp(&before_sample_date, 1980, 1, 1, 13, 56, 41);
  TemporalGenerator::TimestampGen::make_timestamp(&identical_with_sample_date, 2010, 9, 8, 0, 0, 0);
  TemporalGenerator::TimestampGen::make_timestamp(&after_sample_date, 2019, 5, 30, 9, 10, 13);
}

typedef ::testing::Types<Date, DateTime, Timestamp> typesForDateTestCompareOperators;
TYPED_TEST_CASE(DateTestCompareOperators, typesForDateTestCompareOperators);

TYPED_TEST(DateTestCompareOperators, operatorEqual_ComparingWithIdencticalTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date == this->identical_with_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorEqual_ComparingWithDifferentTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date == this->before_sample_date);
  
  ASSERT_FALSE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorNotEqual_ComparingWithIdencticalTemporal_ShouldReturn_False)
{ 
  this->result= (this->sample_date != this->identical_with_sample_date);
  
  ASSERT_FALSE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorNotEqual_ComparingWithDifferentTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date != this->before_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date > this->identical_with_sample_date);
  
  ASSERT_FALSE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThan_ComparingWithLaterTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date > this->after_sample_date);
  
  ASSERT_FALSE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThan_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date > this->before_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date >= this->identical_with_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date >= this->after_sample_date);
  
  ASSERT_FALSE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date >= this->before_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date < this->identical_with_sample_date);
  
  ASSERT_FALSE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThan_ComparingWithLaterTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date < this->after_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThan_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date < this->before_sample_date);
  
  ASSERT_FALSE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date <= this->identical_with_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date <= this->after_sample_date);
  
  ASSERT_TRUE(this->result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date <= this->before_sample_date);
  
  ASSERT_FALSE(this->result);
}

class DateTest : public ::testing::Test
{
  protected:
    Date date;
    bool result;
    
    virtual void SetUp()
    {
      TemporalGenerator::DateGen::make_valid_date(&date);
    }
};

TEST_F(DateTest, operatorAssign_shouldCopyDateRelatadComponents)
{
  Date copy= date;

  EXPECT_EQ(date.years(), copy.years());
  EXPECT_EQ(date.months(), copy.months());
  EXPECT_EQ(date.days(), copy.days());
}

TEST_F(DateTest, is_valid_onValidDate_shouldReturn_True)
{
  result= date.is_valid();
  ASSERT_TRUE(result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithYearBelowMinimum_shouldReturn_False)
{
  date.set_years(DRIZZLE_MIN_YEARS_SQL - 1);
  
  result= date.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithYearAboveMaximum_shouldReturn_False)
{
  date.set_years(DRIZZLE_MAX_YEARS_SQL + 1);
    
  result= date.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithMonthSetToZero_shouldReturn_False)
{
  date.set_months(0);
  
  result= date.is_valid();
  
  ASSERT_FALSE(result);
}


TEST_F(DateTest, is_valid_onInvalidDateWithMonthAboveMaximum_shouldReturn_False)
{
  date.set_months(13);
  
  result= date.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithDaySetToZero_shouldReturn_False)
{
  date.set_days(0);
  
  result= date.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithDayAboveDaysInMonth_shouldReturn_False)
{
  date.set_days(32);
  
  result= date.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithLeapDayInNonLeapYear_shouldReturn_False)
{
  TemporalGenerator::TemporalGen::leap_day_in_non_leap_year(&date);
  
  result= date.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTest, is_valid_onValidDateWithLeapDayInLeapYear_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::leap_day_in_leap_year(&date);
  
  result= date.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(DateTest, to_string_shouldProduce_hyphenSeperatedDateElements)
{
  char expected[Date::MAX_STRING_LENGTH]= "2010-05-01";
  char returned[Date::MAX_STRING_LENGTH];
  TemporalGenerator::DateGen::make_date(&date, 2010, 5, 1);
  
  date.to_string(returned, Date::MAX_STRING_LENGTH);
  
  ASSERT_STREQ(expected, returned);  
}

TEST_F(DateTest, to_string_nullBuffer_shouldReturnProperLengthAnyway)
{
  int length= date.to_string(NULL, 0);
  
  ASSERT_EQ(Date::MAX_STRING_LENGTH - 1, length);  
}

TEST_F(DateTest, from_string_validString_shouldPopulateCorrectly)
{
  char valid_string[Date::MAX_STRING_LENGTH]= "2010-05-01";
  uint32_t years, months, days;

  init_temporal_formats();
  
  result= date.from_string(valid_string, Date::MAX_STRING_LENGTH - 1);
  ASSERT_TRUE(result);
  
  years= date.years();
  months= date.months();
  days= date.days();

  deinit_temporal_formats();
  
  EXPECT_EQ(2010, years);
  EXPECT_EQ(5, months);
  EXPECT_EQ(1, days);
}

TEST_F(DateTest, from_string_invalidString_shouldReturn_False)
{
  char valid_string[Date::MAX_STRING_LENGTH]= "2x10-05-01";

  init_temporal_formats();
  result= date.from_string(valid_string, Date::MAX_STRING_LENGTH - 1);
  deinit_temporal_formats();
  
  ASSERT_FALSE(result);
}

TEST_F(DateTest, to_int64_t)
{
  TemporalGenerator::DateGen::make_date(&date, 2030, 8, 17);
  int64_t representation;
  
  date.to_int64_t(&representation);
  
  ASSERT_EQ(20300817, representation);
}

TEST_F(DateTest, to_int32_t)
{
  TemporalGenerator::DateGen::make_date(&date, 2030, 8, 17);
  int32_t representation;

  date.to_int32_t(&representation);

  ASSERT_EQ(20300817, representation);
}

TEST_F(DateTest, from_int32_t_shouldPopulateDateCorrectly)
{
  uint32_t decoded_years, decoded_months, decoded_days;

  date.from_int32_t(20300817);
  
  decoded_years= date.years();
  decoded_months= date.months();
  decoded_days= date.days();
  
  EXPECT_EQ(2030, decoded_years);
  EXPECT_EQ(8, decoded_months);
  EXPECT_EQ(17, decoded_days);
}

TEST_F(DateTest, to_julian_day_number)
{
  int64_t julian_day;
  TemporalGenerator::DateGen::make_date(&date, 1999, 12, 31);
  
  date.to_julian_day_number(&julian_day);
  
  ASSERT_EQ(2451544, julian_day);
}

TEST_F(DateTest, from_julian_day_number)
{
  int64_t julian_day= 2451544;
  uint32_t years, months, days;
   
  date.from_julian_day_number(julian_day);
  
  years= date.years();
  months= date.months();
  days= date.days();
    
  EXPECT_EQ(1999, years);
  EXPECT_EQ(12, months);
  EXPECT_EQ(31, days);
}

TEST_F(DateTest, DISABLED_to_tm)
{
  uint32_t years= 2030, months= 8, days= 17;
  TemporalGenerator::DateGen::make_date(&date, years, months, days);
  struct tm filled;
  
  date.to_tm(&filled);
  
  EXPECT_EQ(130, filled.tm_year);
  EXPECT_EQ(7, filled.tm_mon);
  EXPECT_EQ(17, filled.tm_mday);
  EXPECT_EQ(0, filled.tm_hour);
  EXPECT_EQ(0, filled.tm_min);
  EXPECT_EQ(0, filled.tm_sec);

  /* TODO:these fail, shouldn't they also be set properly? */
  EXPECT_EQ(228, filled.tm_yday);
  EXPECT_EQ(6, filled.tm_wday);
  EXPECT_EQ(-1, filled.tm_isdst);
}

TEST_F(DateTest, from_tm)
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
  
  EXPECT_EQ(1956, years);  
  EXPECT_EQ(3, months);
  EXPECT_EQ(30, days);
}

TEST_F(DateTest, to_time_t)
{
  time_t time;
  TemporalGenerator::DateGen::make_date(&date, 1990, 9, 9);
  
  date.to_time_t(&time);
  
  ASSERT_EQ(652838400, time);
}

TEST_F(DateTest, from_time_t)
{
  uint32_t years, months, days;
  
  date.from_time_t(652838400);
  
  years= date.years();
  months= date.months();
  days= date.days();
  
  EXPECT_EQ(1990, years);  
  EXPECT_EQ(9, months);
  EXPECT_EQ(9, days);
}

TEST_F(DateTest, to_decimal)
{
  drizzled::my_decimal to;
  TemporalGenerator::DateGen::make_date(&date, 1987, 5, 6);

  date.to_decimal(&to);

  ASSERT_EQ(19870506, to.buf[0]);
}

class DateFromStringTest: public ::testing::TestWithParam<const char*>
{
  protected:
    Date date;
    bool result;
    uint32_t years, months, days;
    
    virtual void SetUp()
    {
      init_temporal_formats();
    }
    
    virtual void TearDown()
    {
      deinit_temporal_formats();
    }
    
    void assign_date_values()
    {
      years= date.years();
      months= date.months();
      days= date.days();
    }
};

TEST_P(DateFromStringTest, from_string)
{
  const char *valid_string= GetParam();
  
  result= date.from_string(valid_string, strlen(valid_string));
  ASSERT_TRUE(result);
  
  assign_date_values();
  
  EXPECT_EQ(2010, years);
  EXPECT_EQ(6, months);
  EXPECT_EQ(7, days);
}

/* TODO:for some reason this was not declared by the macro, needs clarification*/
testing::internal::ParamGenerator<const char*> gtest_ValidStringDateFromStringTest_EvalGenerator_();

INSTANTIATE_TEST_CASE_P(ValidString, DateFromStringTest,
                        ::testing::Values("20100607", /* YYYYMMDD */
                                          "06/07/2010",/* MM[-/.]DD[-/.]YYYY (US common format)*/
                                          "10.06.07",/* YY[-/.]MM[-/.]DD */
                                          "10/6/7",/* YY[-/.][M]M[-/.][D]D */
                                          "2010-6-7"/* YYYY[-/.][M]M[-/.][D]D */));
                                          

                                          