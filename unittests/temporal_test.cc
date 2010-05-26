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

class DateTest : public ::testing::Test {
 protected:
  Date sample_date, date_identical_with_sample_date;
  Date date_before_sample_date, date_after_sample_date;
  
  DateTime datetime_identical_with_sample_date;
  DateTime datetime_before_sample_date, datetime_after_sample_date;
  
  Timestamp timestamp_identical_with_sample_date;
  Timestamp timestamp_before_sample_date, timestamp_after_sample_date;
  
  virtual void SetUp()
  {
    Generator::DateGen::make_date(&sample_date, 2010, 9, 8);

    Generator::DateGen::make_date(&date_before_sample_date, 1980, 1, 1);
    Generator::DateGen::make_date(&date_identical_with_sample_date, 2010, 9, 8);
    Generator::DateGen::make_date(&date_after_sample_date, 2019, 5, 30);

    Generator::DateTimeGen::make_datetime(&datetime_before_sample_date, 1990, 12, 31, 12, 12, 30, 1000);
    Generator::DateTimeGen::make_datetime(&datetime_identical_with_sample_date, 2010, 9, 8, 0, 0, 0, 0);
    Generator::DateTimeGen::make_datetime(&datetime_after_sample_date, 2020, 4, 4, 4, 4, 4, 4000);
    
    Generator::TimestampGen::make_timestamp(&timestamp_before_sample_date, 1980, 1, 1);
    Generator::TimestampGen::make_timestamp(&timestamp_identical_with_sample_date, 2010, 9, 8);
    Generator::TimestampGen::make_timestamp(&timestamp_after_sample_date, 2019, 5, 30);

  }
};


TEST_F(DateTest, operatorEqual_ComparingWithIdencticalDate_ShouldReturn_True)
{
  bool result= (sample_date == date_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorEqual_ComparingWithDifferentDate_ShouldReturn_False)
{
  bool result= (sample_date == date_before_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithIdencticalDate_ShouldReturn_False)
{ 
  bool result= (sample_date != date_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithDifferentDate_ShouldReturn_True)
{
  bool result= (sample_date != date_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithIdenticalDate_ShouldReturn_False)
{
  bool result= (sample_date > date_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithLaterDate_ShouldReturn_False)
{
  bool result= (sample_date > date_after_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithEarlierDate_ShouldReturn_True)
{
  bool result= (sample_date > date_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithIdenticalDate_ShouldReturn_True)
{
  bool result= (sample_date >= date_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithLaterDate_ShouldReturn_False)
{
  bool result= (sample_date >= date_after_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithEarlierDate_ShouldReturn_True)
{
  bool result= (sample_date >= date_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithIdenticalDate_ShouldReturn_False)
{
  bool result= (sample_date < date_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithLaterDate_ShouldReturn_True)
{
  bool result= (sample_date < date_after_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithEarlierDate_ShouldReturn_False)
{
  bool result= (sample_date < date_before_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithIdenticalDate_ShouldReturn_True)
{
  bool result= (sample_date < date_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithLaterDate_ShouldReturn_True)
{
  bool result= (sample_date < date_after_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithEarlierDate_ShouldReturn_False)
{
  bool result= (sample_date < date_before_sample_date);
  
  ASSERT_EQ(false, result);
}

/* Date operators for comparing with DateTime */
TEST_F(DateTest, operatorEqual_ComparingWithIdencticalDateTime_ShouldReturn_True)
{
  bool result= (sample_date == datetime_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorEqual_ComparingWithDifferentDateTime_ShouldReturn_False)
{
  bool result= (sample_date == datetime_before_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithIdencticalDateTime_ShouldReturn_False)
{ 
  bool result= (sample_date != datetime_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithDifferentDateTime_ShouldReturn_True)
{
  bool result= (sample_date != datetime_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithIdenticalDateTime_ShouldReturn_False)
{
  bool result= (sample_date > datetime_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithLaterDateTime_ShouldReturn_False)
{
  bool result= (sample_date > datetime_after_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithEarlierDateTime_ShouldReturn_True)
{
  bool result= (sample_date > datetime_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithIdenticalDateTime_ShouldReturn_True)
{
  bool result= (sample_date >= datetime_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithLaterDateTime_ShouldReturn_False)
{
  bool result= (sample_date >= datetime_after_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithEarlierDateTime_ShouldReturn_True)
{
  bool result= (sample_date >= datetime_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithIdenticalDateTime_ShouldReturn_False)
{
  bool result= (sample_date < datetime_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithLaterDateTime_ShouldReturn_True)
{
  bool result= (sample_date < datetime_after_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithEarlierDateTime_ShouldReturn_False)
{
  bool result= (sample_date < datetime_before_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithIdenticalDateTime_ShouldReturn_True)
{
  bool result= (sample_date < datetime_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithLaterDateTime_ShouldReturn_True)
{
  bool result= (sample_date < datetime_after_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithEarlierDateTime_ShouldReturn_False)
{
  bool result= (sample_date < datetime_before_sample_date);
  
  ASSERT_EQ(false, result);
}


/* Date operators for comparing with Timestamp */
TEST_F(DateTest, operatorEqual_ComparingWithIdencticalTimestamp_ShouldReturn_True)
{
  bool result= (sample_date == timestamp_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorEqual_ComparingWithDifferentTimestamp_ShouldReturn_False)
{
  bool result= (sample_date == timestamp_before_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithIdencticalTimestamp_ShouldReturn_False)
{ 
  bool result= (sample_date != timestamp_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithDifferentTimestamp_ShouldReturn_True)
{
  bool result= (sample_date != timestamp_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithIdenticalTimestamp_ShouldReturn_False)
{
  bool result= (sample_date > timestamp_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithLaterTimestamp_ShouldReturn_False)
{
  bool result= (sample_date > timestamp_after_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithEarlierTimestamp_ShouldReturn_True)
{
  bool result= (sample_date > timestamp_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithIdenticalTimestamp_ShouldReturn_True)
{
  bool result= (sample_date >= timestamp_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithLaterTimestamp_ShouldReturn_False)
{
  bool result= (sample_date >= timestamp_after_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThanOrEqual_ComparingWithEarlierTimestamp_ShouldReturn_True)
{
  bool result= (sample_date >= timestamp_before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithIdenticalTimestamp_ShouldReturn_False)
{
  bool result= (sample_date < timestamp_identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithLaterTimestamp_ShouldReturn_True)
{
  bool result= (sample_date < timestamp_after_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThan_ComparingWithEarlierTimestamp_ShouldReturn_False)
{
  bool result= (sample_date < timestamp_before_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithIdenticalTimestamp_ShouldReturn_True)
{
  bool result= (sample_date < timestamp_identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithLaterTimestamp_ShouldReturn_True)
{
  bool result= (sample_date < timestamp_after_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorLessThanOrEqual_ComparingWithEarlierTimestamp_ShouldReturn_False)
{
  bool result= (sample_date < timestamp_before_sample_date);
  
  ASSERT_EQ(false, result);
}