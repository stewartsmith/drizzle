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
  Date sample_date, identical_with_sample_date, before_sample_date, after_sample_date;
  
  virtual void SetUp
  {
    Generator::DateGen::make_date(&before_sample_date, 1980, 1, 1);
    
    Generator::DateGen::make_date(&sample_date, 2010, 9, 8);
    Generator::DateGen::make_date(&identical_with_sample_date, 2010, 9, 8);

    Generator::DateGen::make_date(&after_sample_date, 2010, 9, 8);
  }
};


TEST_F(DateTest, operatorEqual_ComparingWithIdencticalDate_ShouldReturn_True)
{
  bool result= (sample_date == identical_with_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorEqual_ComparingWithDifferentDate_ShouldReturn_False)
{
  bool result= (sample_date == before_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithIdencticalDate_ShouldReturn_False)
{ 
  bool result= (sample_date != identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorNotEqual_ComparingWithDifferentDate_ShouldReturn_True)
{
  bool result= (sample_date != before_sample_date);
  
  ASSERT_EQ(true, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithIdenticalDate_ShouldReturn_False)
{
  bool result= (sample_date > identical_with_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithLaterDate_ShouldReturn_False)
{
  bool result= (sample_date > after_sample_date);
  
  ASSERT_EQ(false, result);
}

TEST_F(DateTest, operatorGreaterThan_ComparingWithEarlierDate_ShouldReturn_True)
{
  bool result= (sample_date > before_sample_date);
  
  ASSERT_EQ(true, result);
}
