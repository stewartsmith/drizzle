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

class TimeTest: public ::testing::Test
{
protected:
  Time sample_time;
  bool result;
  uint32_t hours, minutes, seconds;
  
  Time identical_with_sample_time, before_sample_time, after_sample_time;
  
  virtual void SetUp()
  {
    TemporalGenerator::TimeGen::make_time(&sample_time, 18, 34, 59);
    
    TemporalGenerator::TimeGen::make_time(&before_sample_time, 18, 34, 58);
    TemporalGenerator::TimeGen::make_time(&identical_with_sample_time, 18, 34, 59);
    TemporalGenerator::TimeGen::make_time(&after_sample_time, 18, 35, 0);
    
  }

  void assign_time_values()
  {
    hours= sample_time.hours();
    minutes= sample_time.minutes();
    seconds= sample_time.seconds();
  }

  void from_string(const char *string)
  {
    
    init_temporal_formats();
    result= sample_time.from_string(string, strlen(string));
    deinit_temporal_formats();
    
    assign_time_values();
  }
};

TEST_F(TimeTest, operatorEqual_ComparingWithIdencticalTime_ShouldReturn_True)
{
  this->result= (this->sample_time == this->identical_with_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorEqual_ComparingWithDifferentTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time == this->before_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorNotEqual_ComparingWithIdencticalTemporal_ShouldReturn_False)
{ 
  this->result= (this->sample_time != this->identical_with_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorNotEqual_ComparingWithDifferentTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time != this->before_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorGreaterThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time > this->identical_with_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorGreaterThan_ComparingWithLaterTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time > this->after_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorGreaterThan_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time > this->before_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorGreaterThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time >= this->identical_with_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorGreaterThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time >= this->after_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorGreaterThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time >= this->before_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time < this->identical_with_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorLessThan_ComparingWithLaterTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time < this->after_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThan_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time < this->before_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time <= this->identical_with_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time <= this->after_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time <= this->before_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, is_valid_onValidTime_shouldReturn_True)
{
  result= sample_time.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(TimeTest, is_valid_onValidMinimalTime_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::make_min_time(&sample_time);
  
  result= sample_time.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(TimeTest, is_valid_onValidMaximalTime_shouldReturn_True)
{
  TemporalGenerator::TemporalGen::make_max_time(&sample_time);
  
  result= sample_time.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(TimeTest, is_valid_onInvalidTimeWithHourAboveMaximum23_shouldReturn_False)
{
  sample_time.set_hours(24);
  
  result= sample_time.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(TimeTest, is_valid_onInvalidTimeWithMinutesAboveMaximum59_shouldReturn_False)
{
  sample_time.set_minutes(60);
  
  result= sample_time.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(TimeTest, is_valid_onInvalidTimeWithSecondsAboveMaximum59_shouldReturn_False)
{
  sample_time.set_seconds(60);
  
  result= sample_time.is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(TimeTest, to_string_shouldProduce_colonSeperatedTimeElements)
{
  char expected[Time::MAX_STRING_LENGTH]= "18:34:59";
  char returned[Time::MAX_STRING_LENGTH];
  
  sample_time.to_string(returned, Time::MAX_STRING_LENGTH);
  
  ASSERT_STREQ(expected, returned);  
}

TEST_F(TimeTest, to_string_nullBuffer_shouldReturnProperLengthAnyway)
{
  int length= sample_time.to_string(NULL, 0);
  
  ASSERT_EQ(Time::MAX_STRING_LENGTH - 1, length);  
}

TEST_F(TimeTest, to_int32_t)
{
  int32_t representation;

  sample_time.to_int32_t(&representation);

  ASSERT_EQ(representation, 183459);
}

TEST_F(TimeTest, from_int32_t_shouldPopulateTimeCorrectly)
{
  sample_time.from_int32_t(183459);
  
  assign_time_values();;
  
  EXPECT_EQ(18, hours);
  EXPECT_EQ(34, minutes);
  EXPECT_EQ(59, seconds);
}

TEST_F(TimeTest, from_time_t)
{
  sample_time.from_time_t(59588);
  
  assign_time_values();
  
  EXPECT_EQ(16, hours);  
  EXPECT_EQ(33, minutes);
  EXPECT_EQ(8, seconds);
}

TEST_F(TimeTest, to_decimal)
{
  drizzled::my_decimal to;
  TemporalGenerator::TimeGen::make_time(&sample_time, 8, 4, 9, 56);

  sample_time.to_decimal(&to);
  
  ASSERT_EQ(80409, to.buf[0]);
  ASSERT_EQ(56000, to.buf[1]);
}

TEST_F(TimeTest, from_string_invalidString_shouldReturn_False)
{
  char invalid_string[Time::MAX_STRING_LENGTH]= "1o:34:59";
  
  init_temporal_formats();
  result= sample_time.from_string(invalid_string, strlen(invalid_string));
  deinit_temporal_formats();
  
  ASSERT_FALSE(result);
}

TEST_F(TimeTest, from_string_validString_minuteAndSecond_shouldPopulateCorrectly)
{
  char valid_string[Time::MAX_STRING_LENGTH]= "4:52";

  from_string(valid_string);

  EXPECT_EQ(4, minutes);
  EXPECT_EQ(52, seconds);
}

TEST_F(TimeTest, from_string_validString_minuteAndSecondNoColon_shouldPopulateCorrectly)
{
  char valid_string[Time::MAX_STRING_LENGTH]= "3456";
  
  from_string(valid_string);
  
  EXPECT_EQ(34, minutes);
  EXPECT_EQ(56, seconds);
}

TEST_F(TimeTest, from_string_validString_secondsOnly_shouldPopulateCorrectly)
{
  char valid_string[Time::MAX_STRING_LENGTH]= "59";
  
  from_string(valid_string);
  
  EXPECT_EQ(59, seconds);
}

class TimeFromStringTest: public ::testing::TestWithParam<const char*>
{
  protected:
    Time time;
    bool result;
    uint32_t hours, minutes, seconds;
    
    virtual void SetUp()
    {
      init_temporal_formats();
    }
    
    virtual void TearDown()
    {
      deinit_temporal_formats();
    }
    
    void assign_time_values()
    {
      hours= time.hours();
      minutes= time.minutes();
      seconds= time.seconds();
    }
};

TEST_P(TimeFromStringTest, from_string)
{
  const char *valid_string= GetParam();

  result= time.from_string(valid_string, strlen(valid_string));
  ASSERT_TRUE(result);
  
  assign_time_values();

  EXPECT_EQ(8, hours);
  EXPECT_EQ(4, minutes);
  EXPECT_EQ(9, seconds);
}

/* TODO:for some reason this was not declared by the macro, needs clarification*/
testing::internal::ParamGenerator<const char*> gtest_ValidStringTimeFromStringTest_EvalGenerator_();

INSTANTIATE_TEST_CASE_P(ValidString, TimeFromStringTest,
                        ::testing::Values("080409",
                                          "80409",
                                          "08:04:09",
                                          "8:04:09",
                                          "8:04:9",
                                          "8:4:9"));
                                          