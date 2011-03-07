/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <config.h>
#include <plugin/schema_dictionary/dictionary.h>

using namespace drizzled;

static int init(drizzled::module::Context &context)
{
  context.add(new ColumnsTool());
  context.add(new IndexPartsTool());
  context.add(new IndexesTool());
  context.add(new ForeignKeysTool());
  context.add(new SchemasTool());
  context.add(new TableConstraintsTool());
  context.add(new TablesTool());
  
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "schema_dictionary",
  "1.0",
  "Brian Aker",
  "Data Dictionary for schema, table, column, indexes, etc",
  PLUGIN_LICENSE_GPL,
  init,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
