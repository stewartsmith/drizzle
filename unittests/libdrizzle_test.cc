/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Andrew Hutchings
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
#include <libdrizzle/drizzle_client.h>
#include <libdrizzle/drizzle_server.h>

TEST(libdrizzle, drizzle_escape_string)
{
  const char* orig= "hello \"world\"\n";
  char out[255];
  size_t out_len;

  out_len= drizzle_escape_string(out, orig, strlen(orig));

  EXPECT_EQ(17, out_len);
  ASSERT_STREQ("hello \\\"world\\\"\\n", out);
}

TEST(libdrizzle, drizzle_hex_string)
{
  const unsigned char orig[5]= {0x34, 0x26, 0x80, 0x99, 0xFF};
  char out[255];
  size_t out_len;

  out_len= drizzle_hex_string(out, (char*) orig, 5);

  EXPECT_EQ(10, out_len);
  ASSERT_STREQ("34268099FF", out);
}

TEST(libdrizzle, drizzle_mysql_password_hash)
{
  const char* orig= "test password";
  char out[255];

  drizzle_mysql_password_hash(out, orig, strlen(orig));

  ASSERT_STREQ("3B942720DACACBBA7E3838AF03C5B6B5A6DFE0AB", out);
}
