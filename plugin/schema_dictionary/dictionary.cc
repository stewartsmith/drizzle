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
static TableConstraintsTool *table_constraints;
static TablesTool *tables;
static TableNames *local_tables;
static TableStatus *table_status;


static int init(drizzled::plugin::Registry &registry)
{
  columns= new(std::nothrow)ColumnsTool;
  index_parts= new(std::nothrow)IndexPartsTool;
  indexes= new(std::nothrow)IndexesTool;
  referential_constraints= new(std::nothrow)ReferentialConstraintsTool;
  schemas= new(std::nothrow)SchemasTool;
  local_tables= new(std::nothrow)TableNames;
  schema_names= new(std::nothrow)SchemaNames;
  table_constraints= new(std::nothrow)TableConstraintsTool;
  table_status= new(std::nothrow)TableStatus;
  tables= new(std::nothrow)TablesTool;

  registry.add(columns);
  registry.add(index_parts);
  registry.add(indexes);
  registry.add(local_tables);
  registry.add(referential_constraints);
  registry.add(schema_names);
  registry.add(schemas);
  registry.add(table_constraints);
  registry.add(table_status);
  registry.add(tables);
  
  return 0;
}

static int finalize(drizzled::plugin::Registry &registry)
{
  registry.remove(columns);
  registry.remove(index_parts);
  registry.remove(indexes);
  registry.remove(local_tables);
  registry.remove(referential_constraints);
  registry.remove(schema_names);
  registry.remove(schemas);
  registry.remove(table_constraints);
  registry.remove(table_status);
  registry.remove(tables);
  delete columns;
  delete index_parts;
  delete indexes;
  delete local_tables;
  delete referential_constraints;
  delete schema_names;
  delete schemas;
  delete table_constraints;
  delete table_status;
  delete tables;

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
  finalize,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
