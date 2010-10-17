/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <drizzled/constrained_value.h>

using namespace drizzled;

namespace po= boost::program_options;

TEST(constrained_value, raw_usage)
{
  constrained_value<uint64_t> val(1, 1024, 1, 10);

  EXPECT_EQ(UINT64_C(1), (uint64_t)val);

  ASSERT_THROW(val= 1025 , po::validation_error);
  ASSERT_THROW(val= 0 , po::validation_error);

  val= 25;

  EXPECT_EQ(20, (uint64_t)val);
}

