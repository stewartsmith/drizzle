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

#include <drizzled/temporal.h>
#include <drizzled/temporal_format.h>
#include <gtest/gtest.h>
#include <vector>

#include "temporal_generator.h"

using namespace drizzled;

class TemporalFormatTest : public ::testing::Test
{
  protected:
    TemporalFormat *tf;
    Temporal *temporal;
    bool result;

  virtual void SetUp()
  {
    tf= NULL;
    temporal= NULL;
  }

  virtual void TearDown()
  {
    if (tf != NULL)
      delete tf;
    if (temporal != NULL)
      delete temporal;
  }
};


TEST_F(TemporalFormatTest, constructor_WithValidRegExp_shouldCompile)
{
  tf= new TemporalFormat("^(\\d{4})[-/.]$");
  
  result= tf->is_valid();

  ASSERT_TRUE(result);
}

TEST_F(TemporalFormatTest, constructor_WithInvalidRegExp_shouldNotCompile)
{
  tf= new TemporalFormat("^\\d{4)[-/.]$");

  result= tf->is_valid();
  
  ASSERT_FALSE(result);
}

TEST_F(TemporalFormatTest, matches_OnNotMatchingString_shouldReturn_False)
{
  tf= new TemporalFormat("^(\\d{4})[-/.]$");
  char matched[] ="1234/ABC";

  result= tf->matches(matched, sizeof(matched) - sizeof(char), NULL);
  
  ASSERT_FALSE(result);
}

TEST_F(TemporalFormatTest, matches_OnMatchingString_FormatWithIndexesSet_shouldPopulateTemporal)
{
  char regexp[]= "^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})[T|\\s+](\\d{2}):(\\d{2}):(\\d{2})$";
  char matched[]= "1999/9/14T23:29:05";
  tf= TemporalGenerator::TemporalFormatGen::make_temporal_format(regexp, 1, 2, 3, 4, 5, 6, 0, 0);
  temporal= new DateTime();

  
  result= tf->matches(matched, sizeof(matched) - sizeof(char), temporal);
  ASSERT_TRUE(result);
  
  EXPECT_EQ(1999, temporal->years());
  EXPECT_EQ(9, temporal->months());
  EXPECT_EQ(14, temporal->days());
  EXPECT_EQ(23, temporal->hours());
  EXPECT_EQ(29, temporal->minutes());
  EXPECT_EQ(5, temporal->seconds());
}

TEST_F(TemporalFormatTest, matches_FormatWithMicroSecondIndexSet_shouldAddTrailingZeros)
{
  tf= TemporalGenerator::TemporalFormatGen::make_temporal_format("^(\\d{1,6})$", 0, 0, 0, 0, 0, 0, 1, 0);
  char matched[]= "560";
  temporal= new Time();
  
  tf->matches(matched, sizeof(matched) - sizeof(char), temporal);
  
  ASSERT_EQ(560000, temporal->useconds());
}

TEST_F(TemporalFormatTest, matches_FormatWithNanoSecondIndexSet_shouldAddTrailingZeros)
{
  tf= TemporalGenerator::TemporalFormatGen::make_temporal_format("^(\\d{1,9})$", 0, 0, 0, 0, 0, 0, 0, 1);
  char matched[]= "4321";
  temporal= new Time();
  
  tf->matches(matched, sizeof(matched) - sizeof(char), temporal);
  
  ASSERT_EQ(432100000, temporal->nseconds());
}

namespace drizzled
{
extern std::vector<TemporalFormat *> known_datetime_formats;
extern std::vector<TemporalFormat *> known_date_formats;
extern std::vector<TemporalFormat *> known_time_formats;
extern std::vector<TemporalFormat *> all_temporal_formats;
}

TEST(TemporalFormatInitTest, init_temporal_formats_vectorsWithKnownFormats_shouldHaveExpectedLengths)
{
  init_temporal_formats();

  EXPECT_EQ(13, known_datetime_formats.size());	
  EXPECT_EQ(8, known_date_formats.size());
  EXPECT_EQ(6, known_time_formats.size());
  EXPECT_EQ(19, all_temporal_formats.size());
}

TEST(TemporalFormatDeinitTest, deinit_temporal_formats_vectorsWithKnownFormats_shouldHaveZeroLengths)
{
  init_temporal_formats();
  deinit_temporal_formats();
  
  EXPECT_EQ(0, known_datetime_formats.size());
  EXPECT_EQ(0, known_date_formats.size());
  EXPECT_EQ(0, known_time_formats.size());
  EXPECT_EQ(0, all_temporal_formats.size());
}
