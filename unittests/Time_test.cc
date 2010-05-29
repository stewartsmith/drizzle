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

#include "generator.h"

using namespace drizzled;

class TimeTest: public ::testing::Test
{
protected:
  Time sample_time;
  bool result;
  
  Time identical_with_sample_time, before_sample_time, after_sample_time;
  
  virtual void SetUp()
  {
    Generator::TimeGen::make_time(&sample_time, 18, 34, 59);
    
    Generator::TimeGen::make_time(&before_sample_time, 18, 34, 58);
    Generator::TimeGen::make_time(&identical_with_sample_time, 18, 34, 59);
    Generator::TimeGen::make_time(&after_sample_time, 18, 35, 0);
    
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
  this->result= (this->sample_time < this->identical_with_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_True)
{
  this->result= (this->sample_time < this->after_sample_time);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  this->result= (this->sample_time < this->before_sample_time);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, is_valid_onValidTime_shouldReturn_True)
{
  result= sample_time.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(TimeTest, is_valid_onValidMinimalTime_shouldReturn_True)
{
  Generator::TimeGen::make_min_time(&sample_time);
  
  result= sample_time.is_valid();
  
  ASSERT_TRUE(result);
}

TEST_F(TimeTest, is_valid_onValidMaximalTime_shouldReturn_True)
{
  Generator::TimeGen::make_max_time(&sample_time);
  
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

TEST_F(TimeTest, from_string_validString_shouldPopulateCorrectly)
{
  char valid_string[Time::MAX_STRING_LENGTH]= "18:34:59";
  uint32_t hours, minutes, seconds;
  
  result = sample_time.from_string(valid_string, Time::MAX_STRING_LENGTH);
  ASSERT_TRUE(result);
  
  hours= sample_time.hours();
  minutes = sample_time.minutes();
  seconds = sample_time.seconds();
  
  EXPECT_EQ(18, hours);
  EXPECT_EQ(34, minutes);
  EXPECT_EQ(59, seconds);
}

TEST_F(TimeTest, from_string_invalidString_shouldReturn_False)
{
  char invalid_string[Time::MAX_STRING_LENGTH]= "1o:34:59";
  
  result = sample_time.from_string(invalid_string, Time::MAX_STRING_LENGTH);
  ASSERT_FALSE(result);
}

TEST_F(TimeTest, from_int32_t_onValueCreatedBy_to_int32_t_shouldProduceOriginalTime)
{
  uint32_t decoded_hours, decoded_minutes, decoded_seconds;
  int32_t representation;
  Time decoded_time;
  
  sample_time.to_int32_t(&representation);
  decoded_time.from_int32_t(representation);
  
  decoded_hours = decoded_time.hours();
  decoded_minutes = decoded_time.minutes();
  decoded_seconds = decoded_time.seconds();
  
  EXPECT_EQ(18, decoded_hours);
  EXPECT_EQ(34, decoded_minutes);
  EXPECT_EQ(59, decoded_seconds);
}

TEST_F(TimeTest, from_time_t)
{
  uint32_t hours, minutes, seconds;
  
  sample_time.from_time_t(59588);
  
  hours = sample_time.hours();
  minutes = sample_time.minutes();
  seconds = sample_time.seconds();
  
  EXPECT_EQ(16, hours);  
  EXPECT_EQ(33, minutes);
  EXPECT_EQ(8, seconds);
}
