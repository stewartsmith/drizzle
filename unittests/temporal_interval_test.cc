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

#include <boost/shared_ptr.hpp>

#include <cstdlib>
#include <gtest/gtest.h>
#include <drizzled/item.h>
#include <drizzled/session.h>
#include <drizzled/sql_string.h>
#include <drizzled/temporal_interval.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/drizzled.h>

#include "temporal_generator.h"

using namespace drizzled;

class ItemStub : public Item
{
  ItemStub(Session *fake_session) : Item(fake_session, this)
  {
    string_to_return= NULL;
    int_to_return= 0;
  }
  
  public:
    String *string_to_return;
    int int_to_return;
    
    static boost::shared_ptr<ItemStub> get_item_stub(Session *fake_session)
    {
      boost::shared_ptr<ItemStub> result(new ItemStub(fake_session));
      return result;
    }
    
    virtual enum Type type() const { return FIELD_ITEM; };
    virtual double val_real() { return 0; };
    virtual int64_t val_int() { return int_to_return; };
    virtual String *val_str(String *str)
    {
      (void) str;
      return string_to_return;
    };
    virtual type::Decimal *val_decimal(type::Decimal *decimal_buffer)
    {
      (void) decimal_buffer;
      return NULL;
    };
};

class TemporalIntervalTest : public ::testing::Test
{
  protected:
    String string_to_return;
    String buffer;
    bool result;
    boost::shared_ptr<ItemStub> item;
    TemporalInterval interval;
    Session *fake_session;

    plugin::Client *fake_client;
    
    virtual void SetUp()
    {
      init_thread_environment();
      fake_client= plugin::Listen::getNullClient();
      fake_session= new Session(fake_client);
      fake_session->thread_stack= (char*) &fake_session;
      fake_session->initGlobals();
      item= ItemStub::get_item_stub(fake_session);


      /*28 for full date and time format with sign*/
      string_to_return.alloc(28);
      buffer.alloc(28);
      item->string_to_return= &string_to_return;
      item->int_to_return= 0;
      item->null_value= false;
    }
};

TEST_F(TemporalIntervalTest, initFromItem_intervalWeek)
{
  char string[]= "30";
  item->string_to_return->set_ascii(string, strlen(string));
  item->int_to_return= 30;

  interval.initFromItem(item.get(), INTERVAL_WEEK, &buffer);

  ASSERT_EQ(210, interval.get_day());
}

TEST_F(TemporalIntervalTest, initFromItem_intervalDayMicrosecond)
{
  char string[]= "7 12:45:19.777";
  item->string_to_return->set_ascii(string, strlen(string));

  interval.initFromItem(item.get(), INTERVAL_DAY_MICROSECOND, &buffer);

  EXPECT_EQ(7, interval.get_day());
  EXPECT_EQ(12, interval.get_hour());
  EXPECT_EQ(45, interval.get_minute());
  EXPECT_EQ(19, interval.get_second());
  EXPECT_EQ(777000, interval.get_second_part());
}

TEST_F(TemporalIntervalTest, initFromItem_intervalDayMicrosecond_tooFewArguments_shouldOmitHighEndItems)
{
  char string[]= "45:19.777";
  item->string_to_return->set_ascii(string, strlen(string));
  
  interval.initFromItem(item.get(), INTERVAL_DAY_MICROSECOND, &buffer);
  
  EXPECT_EQ(0, interval.get_day());
  EXPECT_EQ(0, interval.get_hour());
  EXPECT_EQ(45, interval.get_minute());
  EXPECT_EQ(19, interval.get_second());
  EXPECT_EQ(777000, interval.get_second_part());
}


TEST(TemporalIntervalAddDateTest, addDate_positiveDayMicrosecond)
{
  type::Time drizzle_time= {1990, 3, 25, 15, 5, 16, 876543, false, DRIZZLE_TIMESTAMP_DATETIME};
  TemporalInterval *interval= TemporalGenerator::TemporalIntervalGen::make_temporal_interval(
                               0, 0, 6, 13, 54, 3, 435675, false);

  interval->addDate(&drizzle_time, INTERVAL_DAY_MICROSECOND);

  EXPECT_EQ(1990, drizzle_time.year);
  EXPECT_EQ(4, drizzle_time.month);
  EXPECT_EQ(1, drizzle_time.day);
  EXPECT_EQ(4, drizzle_time.hour);
  EXPECT_EQ(59, drizzle_time.minute);
  EXPECT_EQ(20, drizzle_time.second);
  EXPECT_EQ(312218, drizzle_time.second_part);
}

TEST(TemporalIntervalAddDateTest, addDate_negativeDayMicrosecond)
{
  type::Time drizzle_time= {1990, 4, 1, 4, 59, 20, 312218, false, DRIZZLE_TIMESTAMP_DATETIME};
  TemporalInterval *interval= TemporalGenerator::TemporalIntervalGen::make_temporal_interval(
                               0, 0, 6, 13, 54, 3, 435675, true);
  
  interval->addDate(&drizzle_time, INTERVAL_DAY_MICROSECOND);
  
  EXPECT_EQ(1990, drizzle_time.year);
  EXPECT_EQ(3, drizzle_time.month);
  EXPECT_EQ(25, drizzle_time.day);
  EXPECT_EQ(15, drizzle_time.hour);
  EXPECT_EQ(5, drizzle_time.minute);
  EXPECT_EQ(16, drizzle_time.second);
  EXPECT_EQ(876543, drizzle_time.second_part);
}

TEST(TemporalIntervalAddDateTest, addDate_positiveDayMicrosecond_shouldCountLeapDayToo)
{
  type::Time drizzle_time= {2004, 2, 25, 15, 5, 16, 876543, false, DRIZZLE_TIMESTAMP_DATETIME};
  TemporalInterval *interval= TemporalGenerator::TemporalIntervalGen::make_temporal_interval(
                               0, 0, 6, 13, 54, 3, 435675, false);
  
  interval->addDate(&drizzle_time, INTERVAL_DAY_MICROSECOND);
  
  EXPECT_EQ(2004, drizzle_time.year);
  EXPECT_EQ(3, drizzle_time.month);
  EXPECT_EQ(3, drizzle_time.day);
  EXPECT_EQ(4, drizzle_time.hour);
  EXPECT_EQ(59, drizzle_time.minute);
  EXPECT_EQ(20, drizzle_time.second);
  EXPECT_EQ(312218, drizzle_time.second_part);
}

TEST(TemporalIntervalAddDateTest, addDate_negativeWeek)
{
  type::Time drizzle_time= {1998, 1, 25, 0, 0, 0, 0, false, DRIZZLE_TIMESTAMP_DATE};
  TemporalInterval *interval= TemporalGenerator::TemporalIntervalGen::make_temporal_interval(
                              0, 0, 28, 0, 0, 0, 0, true);
  
  interval->addDate(&drizzle_time, INTERVAL_WEEK);
  
  EXPECT_EQ(1997, drizzle_time.year);
  EXPECT_EQ(12, drizzle_time.month);
  EXPECT_EQ(28, drizzle_time.day);
}

TEST(TemporalIntervalAddDateTest, addDate_addPositiveYearToLeapDay)
{
  type::Time drizzle_time= {2004, 2, 29, 0, 0, 0, 0, false, DRIZZLE_TIMESTAMP_DATE};
  TemporalInterval *interval= TemporalGenerator::TemporalIntervalGen::make_temporal_interval(
                               5, 0, 0, 0, 0, 0, 0, false);
  
  interval->addDate(&drizzle_time, INTERVAL_YEAR);
  
  EXPECT_EQ(2009, drizzle_time.year);
  EXPECT_EQ(2, drizzle_time.month);
  EXPECT_EQ(28, drizzle_time.day);
}

TEST(TemporalIntervalAddDateTest, addDate_addOneMonthToLastDayInMonth_shouldChangeToProperLastDay)
{
  type::Time drizzle_time= {2004, 7, 31, 0, 0, 0, 0, false, DRIZZLE_TIMESTAMP_DATE};
  TemporalInterval *interval= TemporalGenerator::TemporalIntervalGen::make_temporal_interval(
                               0, 2, 0, 0, 0, 0, 0, false);
  
  interval->addDate(&drizzle_time, INTERVAL_MONTH);
  
  EXPECT_EQ(2004, drizzle_time.year);
  EXPECT_EQ(9, drizzle_time.month);
  EXPECT_EQ(30, drizzle_time.day);
}
