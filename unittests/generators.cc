/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <drizzled/generator.h>
#include <drizzled/plugin/listen.h>

/*
  These will not work currently because of init issues for the server.
*/
#if 0
TEST(generator_test, schema)
{
  drizzled::Session session(drizzled::plugin::Listen::getNullClient());
  drizzled::generator::Schema generator(session);

  const drizzled::message::Schema *schema_ptr;
  if ((schema_ptr= generator))
    ASSERT_TRUE(0);
}

TEST(generator_test, table)
{
  drizzled::Session session(drizzled::plugin::Listen::getNullClient());
  drizzled::SchemaIdentifier schema("foo");
  drizzled::generator::Table generator(session, schema);
  const drizzled::message::Table *table_ptr;

  if ((table_ptr= generator))
    ASSERT_TRUE(0);
}

TEST(generator_test, all_tables)
{
  drizzled::Session session(drizzled::plugin::Listen::getNullClient());
  drizzled::generator::AllTables generator(session);
  const drizzled::message::Table *table_ptr;

  if ((table_ptr= generator))
    ASSERT_TRUE(0);
}

TEST(generator_test, all_fields)
{
  drizzled::Session session(drizzled::plugin::Listen::getNullClient());
  drizzled::generator::AllFields generator(session);
  const drizzled::message::Table::Field *field_ptr;

  if ((field_ptr= generator))
    ASSERT_TRUE(0);
}
#endif
