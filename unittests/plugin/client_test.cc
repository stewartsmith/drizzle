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
 
#include <config.h>

#include <drizzled/plugin/client.h>
#include <drizzled/type/time.h>
#include <gtest/gtest.h>
#include <string.h>

#include "unittests/plugin/plugin_stubs.h"

using namespace drizzled;

class ClientTest : public ::testing::Test
{
public:
  char buffer[40];
  ClientStub client;

  virtual void SetUp()
  {
    memset(buffer, '\0', sizeof(buffer));
    client.set_store_ret_val(true);
    client.set_last_call_char_ptr(buffer);
  }
};

TEST_F(ClientTest, store_drizzle_time_datetime)
{
  //TODO: is the sign intentionally ignored in the case of a datetime?
  type::Time dt(2010, 4, 5, 23, 45, 3, 777, type::DRIZZLE_TIMESTAMP_DATETIME);
  char expected[]= "2010-04-05 23:45:03.000777";

  client.store(&dt);

  ASSERT_STREQ(expected, buffer);
}

TEST_F(ClientTest, store_drizzle_time_date)
{
  //TODO: is the sign intentionally ignored in the case of a date?
  type::Time dt(2010, 4, 5, 0, 0, 0, 0, type::DRIZZLE_TIMESTAMP_DATE);
  char expected[]= "2010-04-05";
  
  client.store(&dt);
  
  ASSERT_STREQ(expected, buffer);
}

TEST_F(ClientTest, store_drizzle_time_time)
{
  type::Time dt(23, 45, 3, 777, true);
  char expected[]= "-23:45:03.000777";
  
  client.store(&dt);
  
  ASSERT_STREQ(expected, buffer);
}
