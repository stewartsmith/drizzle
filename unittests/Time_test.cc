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

class TimeTest: public ::testing::Test
{
  Time sample_time;
  bool result;
  
  Time identical_with_sample_date, before_sample_date, after_sample_date;
  
  void initBeforeIdenticalAfter();
  
  virtual void SetUp()
  {
    Generator::DateGen::make_date(&sample_date, 2010, 9, 8);
    initBeforeIdenticalAfter();
  }
}

TEST_F(TimeTest, operatorEqual_ComparingWithIdencticalTime_ShouldReturn_True)
{
  this->result= (this->sample_date == this->identical_with_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorEqual_ComparingWithDifferentTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date == this->before_sample_date);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorNotEqual_ComparingWithIdencticalTemporal_ShouldReturn_False)
{ 
  this->result= (this->sample_date != this->identical_with_sample_date);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorNotEqual_ComparingWithDifferentTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date != this->before_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorGreaterThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date > this->identical_with_sample_date);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorGreaterThan_ComparingWithLaterTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date > this->after_sample_date);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorGreaterThan_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date > this->before_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorGreaterThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date >= this->identical_with_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorGreaterThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date >= this->after_sample_date);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorGreaterThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date >= this->before_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThan_ComparingWithIdenticalTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date < this->identical_with_sample_date);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorLessThan_ComparingWithLaterTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date < this->after_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThan_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date < this->before_sample_date);
  
  ASSERT_FALSE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithIdenticalTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date < this->identical_with_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithLaterTemporal_ShouldReturn_True)
{
  this->result= (this->sample_date < this->after_sample_date);
  
  ASSERT_TRUE(this->result);
}

TEST_F(TimeTest, operatorLessThanOrEqual_ComparingWithEarlierTemporal_ShouldReturn_False)
{
  this->result= (this->sample_date < this->before_sample_date);
  
  ASSERT_FALSE(this->result);
}