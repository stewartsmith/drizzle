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

template <typename TemporalType>
class DateTestCompareOperators : public ::testing::Test
{
 protected:
  Date sample_date;
  
  TemporalType identical_with_sample_date, before_sample_date, after_sample_date;
  
  void initBeforeIdenticalAfter();

  virtual void SetUp()
  {
    Generator::DateGen::make_date(&sample_date, 2010, 9, 8);
    initBeforeIdenticalAfter();
  }
};

template<> void DateTestCompareOperators<Date>::initBeforeIdenticalAfter()
{
  Generator::DateGen::make_date(&before_sample_date, 1980, 1, 1);
  Generator::DateGen::make_date(&identical_with_sample_date, 2010, 9, 8);
  Generator::DateGen::make_date(&after_sample_date, 2019, 5, 30);
}

template<> void DateTestCompareOperators<DateTime>::initBeforeIdenticalAfter()
{
  Generator::DateTimeGen::make_datetime(&before_sample_date, 1990, 12, 31, 12, 12, 30, 1000);
  Generator::DateTimeGen::make_datetime(&identical_with_sample_date, 2010, 9, 8, 0, 0, 0, 0);
  Generator::DateTimeGen::make_datetime(&after_sample_date, 2020, 4, 4, 4, 4, 4, 4000);
}

template<> void DateTestCompareOperators<Timestamp>::initBeforeIdenticalAfter()
{
  Generator::TimestampGen::make_timestamp(&before_sample_date, 1980, 1, 1);
  Generator::TimestampGen::make_timestamp(&identical_with_sample_date, 2010, 9, 8);
  Generator::TimestampGen::make_timestamp(&after_sample_date, 2019, 5, 30);
}

typedef ::testing::Types<Date, DateTime, Timestamp> typesForDateTestCompareOperators;
TYPED_TEST_CASE(DateTestCompareOperators, typesForDateTestCompareOperators);

TYPED_TEST(DateTestCompareOperators, operatorEqual_ComparingWithIdencticalTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date == this->identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorEqual_ComparingWithDifferentTemporal_ShouldReturn_False)
{
  bool result= (this->sample_date == this->before_sample_date);
  
  ASSERT_EQ(false, result);
}

TYPED_TEST(DateTestCompareOperators, operatorNotEqual_ComparingWithIdencticalTemporal_ShouldReturn_False)
{ 
  bool result= (this->sample_date != this->identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TYPED_TEST(DateTestCompareOperators, operatorNotEqual_ComparingWithDifferentTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date != this->before_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  bool result= (this->sample_date > this->identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThan_ComparingWithLaterTemporal_ShouldReturn_False)
{
  bool result= (this->sample_date > this->after_sample_date);
  
  ASSERT_EQ(false, result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThan_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date > this->before_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date >= this->identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_False)
{
  bool result= (this->sample_date >= this->after_sample_date);
  
  ASSERT_EQ(false, result);
}

TYPED_TEST(DateTestCompareOperators, operatorGreaterThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date >= this->before_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  bool result= (this->sample_date < this->identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThan_ComparingWithLaterTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date < this->after_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThan_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  bool result= (this->sample_date < this->before_sample_date);
  
  ASSERT_EQ(false, result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date < this->identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_True)
{
  bool result= (this->sample_date < this->after_sample_date);
  
  ASSERT_EQ(true, result);
}

TYPED_TEST(DateTestCompareOperators, operatorLessThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  bool result= (this->sample_date < this->before_sample_date);
  
  ASSERT_EQ(false, result);
}

class DateTest : public ::testing::Test
{
  protected:
    Date date;
    
    virtual void SetUp()
    {
      Generator::DateGen::make_valid_date(&date);
    }
}

TEST_F(DateTest, is_valid_onValidDate_shouldReturn_True)
{
  bool result = date.is_valid();
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithYearBelowMinimum_shouldReturn_False)
{
  date.set_years(DRIZZLE_MIN_YEARS_SQL - 1);
  
  bool result = date.is_valid();
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithYearAboveMaximum_shouldReturn_False)
{
  date.set_years(DRIZZLE_MAX_YEARS_SQL + 1);
    
  bool result = date.is_valid();
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithMonthSetToZero_shouldReturn_False)
{
  date.set_months(0);
  
  bool result = date.is_valid();
  
  ASSERT_EQ(false, result);
}


TEST_F(DateTest, is_valid_onInvalidDateWithMonthAboveMaximum_shouldReturn_False)
{
  date.set_months(13);
  
  bool result = date.is_valid();
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithDaySetToZero_shouldReturn_False)
{
  date.set_days(0);
  
  bool result = date.is_valid();
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithDayAboveDaysInMonth_shouldReturn_False)
{
  date.set_days(32);
  
  bool result = date.is_valid();
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, is_valid_onInvalidDateWithLeapDayInNonLeapYear_shouldReturn_False)
{
  leap_day_in_non_leap_year(&date);
  
  bool result = date.is_valid();
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, is_valid_onValidDateWithLeapDayInLeapYear_shouldReturn_True)
{
  leap_day_in_leap_year(&date);
  
  bool result = date.is_valid();
  
  ASSERT_EQ(true, result);
}
