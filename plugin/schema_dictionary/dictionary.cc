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

#include "config.h"
#include "plugin/schema_dictionary/dictionary.h"

using namespace drizzled;

static ColumnsTool *columns;
static IndexPartsTool *index_parts;
static IndexesTool *indexes;
static ReferentialConstraintsTool *referential_constraints;
static SchemasTool *schemas;
static SchemaNames *schema_names;
static ShowColumns *show_columns;
static ShowIndexes *show_indexes;
static TableConstraintsTool *table_constraints;
static TablesTool *tables;
static ShowTables *local_tables;
static ShowTableStatus *table_status;
static ShowTemporaryTables *show_temporary_tables;


static int init(drizzled::plugin::Context &context)
{
  columns= new(std::nothrow)ColumnsTool;
  index_parts= new(std::nothrow)IndexPartsTool;
  indexes= new(std::nothrow)IndexesTool;
  local_tables= new(std::nothrow)ShowTables;
  referential_constraints= new(std::nothrow)ReferentialConstraintsTool;
  schema_names= new(std::nothrow)SchemaNames;
  schemas= new(std::nothrow)SchemasTool;
  show_columns= new(std::nothrow)ShowColumns;
  show_indexes= new(std::nothrow)ShowIndexes;
  show_temporary_tables= new(std::nothrow)ShowTemporaryTables;
  table_constraints= new(std::nothrow)TableConstraintsTool;
  table_status= new(std::nothrow)ShowTableStatus;
  tables= new(std::nothrow)TablesTool;

  context.add(columns);
  context.add(index_parts);
  context.add(indexes);
  context.add(local_tables);
  context.add(referential_constraints);
  context.add(schema_names);
  context.add(schemas);
  context.add(show_columns);
  context.add(show_indexes);
  context.add(show_temporary_tables);
  context.add(table_constraints);
  context.add(table_status);
  context.add(tables);

  context.add(new ShowSchemas());
  
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
