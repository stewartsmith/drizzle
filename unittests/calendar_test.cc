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

#include <drizzled/calendar.h>

using namespace drizzled;

TEST(calendar_julian_day_number_from_gregorian_date_test, CalculationTest)
{
  uint32_t year, month, day;

  year= 2010; month= 4; day= 2;
  EXPECT_EQ(2455289, julian_day_number_from_gregorian_date(year, month, day));
  
  year= 1976; month= 12; day= 21;
  EXPECT_EQ(2443134, julian_day_number_from_gregorian_date(year, month, day));
  
  year= 2050; month= 6; day= 15;
  EXPECT_EQ(2469973, julian_day_number_from_gregorian_date(year, month, day));
  
  year= 1999; month= 12; day= 31;
  EXPECT_EQ(2451544, julian_day_number_from_gregorian_date(year, month, day));
  
  year= 2008; month= 2; day= 29;
  EXPECT_EQ(2454526, julian_day_number_from_gregorian_date(year, month, day));
}

TEST(calendar_gregorian_date_from_julian_day_number_test, CalculationTest)
{
  int64_t julian_day;
  uint32_t year_out, month_out, day_out;
  
  julian_day= 2455289;
  gregorian_date_from_julian_day_number(julian_day, &year_out, &month_out, &day_out);
  EXPECT_TRUE((year_out == 2010) && (month_out == 4) && (day_out == 2));
  
  julian_day= 2443134;
  gregorian_date_from_julian_day_number(julian_day, &year_out, &month_out, &day_out);
  EXPECT_TRUE((year_out == 1976) && (month_out == 12) && (day_out == 21));
  
  julian_day= 2469973;
  gregorian_date_from_julian_day_number(julian_day, &year_out, &month_out, &day_out);
  EXPECT_TRUE((year_out == 2050) && (month_out == 6) && (day_out == 15));
  
  julian_day= 2451544;
  gregorian_date_from_julian_day_number(julian_day, &year_out, &month_out, &day_out);
  EXPECT_TRUE((year_out == 1999) && (month_out == 12) && (day_out == 31));
  
  julian_day= 2454526;
  gregorian_date_from_julian_day_number(julian_day, &year_out, &month_out, &day_out);
  EXPECT_TRUE((year_out == 2008) && (month_out == 2) && (day_out == 29));
}




TEST(calendar_day_of_week_test, MondayFirst)
{
  /* Friday 2010 IV 2 */
  ASSERT_EQ(4, day_of_week(2455289, false));
}

TEST(calendar_day_of_week_test, SundayFirst)
{
  /* Friday 2010 IV 2 */
  ASSERT_EQ(5, day_of_week(2455289, true));
}




TEST(calendar_in_unix_epoch_range_test, MinOfRange)
{
  uint32_t year= 1970, month= 1, day= 1, hour= 0, minute= 0, second= 0;

  ASSERT_TRUE(in_unix_epoch_range(year, month, day, hour, minute, second));
}

TEST(calendar_in_unix_epoch_range_test, MaxOfRange)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 7;

  ASSERT_TRUE(in_unix_epoch_range(year, month, day, hour, minute, second));
}

TEST(calendar_in_unix_epoch_range_test, OneSecondBeforeMinOfRange)
{
  uint32_t year= 1969, month= 12, day= 31, hour= 23, minute= 59, second= 59;

  ASSERT_FALSE(in_unix_epoch_range(year, month, day, hour, minute, second));
}

TEST(calendar_in_unix_epoch_range_test, OneSecondAfterMaxOfRange)
{
  uint32_t year= 2038, month= 1, day= 19, hour= 3, minute= 14, second= 8;

  ASSERT_FALSE(in_unix_epoch_range(year, month, day, hour, minute, second));
}

TEST(calendar_in_unix_epoch_range_test, InsideRange)
{
  uint32_t year= 1980, month= 11, day= 1, hour= 5, minute= 8, second= 5;
  EXPECT_TRUE(in_unix_epoch_range(year, month, day, hour, minute, second));

  year= 2010; month= 4; day= 2; hour= 21; minute= 44; second= 0;
  EXPECT_TRUE(in_unix_epoch_range(year, month, day, hour, minute, second));

  year= 2020; month= 7; day= 13; hour= 16; minute= 56; second= 59;
  EXPECT_TRUE(in_unix_epoch_range(year, month, day, hour, minute, second));
}
