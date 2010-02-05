/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems
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

#include <plugin/data_engine/function.h>
#include <plugin/data_engine/cursor.h>

#include <string>

using namespace std;
using namespace drizzled;

Function::Function(const std::string &name_arg) :
  drizzled::plugin::StorageEngine(name_arg,
                                  HTON_ALTER_NOT_SUPPORTED |
                                  HTON_HAS_DATA_DICTIONARY |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED)
{
}


Cursor *Function::create(TableShare &table, memory::Root *mem_root)
{
  return new (mem_root) FunctionCursor(*this, table);
}

int Function::doGetTableDefinition(Session &,
                                     const char *path,
                                     const char *,
                                     const char *,
                                     const bool,
                                     message::Table *table_proto)
{
  string tab_name(path);
  transform(tab_name.begin(), tab_name.end(),
            tab_name.begin(), ::tolower);

  drizzled::plugin::TableFunction *function= getFunction(tab_name);

  if (not function)
  {
    return ENOENT;
  }

  if (table_proto)
  {
    function->define(*table_proto);
  }

  return EEXIST;
}


void Function::doGetTableNames(drizzled::CachedDirectory&, 
                               string &db, 
                               set<string> &set_of_names)
{
  drizzled::plugin::TableFunction::getNames(db, set_of_names);
}


static drizzled::plugin::StorageEngine *function_plugin= NULL;
static CharacterSetsTool *character_sets;
static CollationsTool *collations;
static ColumnsTool *columns;
static IndexPartsTool *index_parts;
static IndexesTool *indexes;
static ModulesTool *modules;
static PluginsTool *plugins;
static ProcesslistTool *processlist;
static ReferentialConstraintsTool *referential_constraints;
static SchemasTool *schemas;
static SchemaNames *schema_names;
static StatementsTool *global_statements;
static StatementsTool *session_statements;
static StatusTool *global_status;
static StatusTool *session_status;
static TableConstraintsTool *table_constraints;
static TablesTool *tables;
static TableNames *local_tables;
static VariablesTool *global_variables;
static VariablesTool *session_variables;
static TableStatus *table_status;


static int init(drizzled::plugin::Registry &registry)
{
  function_plugin= new(std::nothrow) Function("FunctionEngine");
  if (not function_plugin)
  {
    return 1;
  }

  character_sets= new(std::nothrow)CharacterSetsTool;
  collations= new(std::nothrow)CollationsTool;
  columns= new(std::nothrow)ColumnsTool;
  index_parts= new(std::nothrow)IndexPartsTool;
  indexes= new(std::nothrow)IndexesTool;
  modules= new(std::nothrow)ModulesTool;
  plugins= new(std::nothrow)PluginsTool;
  processlist= new(std::nothrow)ProcesslistTool;
  referential_constraints= new(std::nothrow)ReferentialConstraintsTool;
  schemas= new(std::nothrow)SchemasTool;
  global_statements= new(std::nothrow)StatementsTool(true);
  global_status= new(std::nothrow)StatusTool(true);
  local_tables= new(std::nothrow)TableNames;
  schema_names= new(std::nothrow)SchemaNames;
  session_statements= new(std::nothrow)StatementsTool(false);
  session_status= new(std::nothrow)StatusTool(false);
  table_constraints= new(std::nothrow)TableConstraintsTool;
  table_status= new(std::nothrow)TableStatus;
  tables= new(std::nothrow)TablesTool;
  global_variables= new(std::nothrow)VariablesTool(true);
  session_variables= new(std::nothrow)VariablesTool(false);

  registry.add(function_plugin);

  registry.add(character_sets);
  registry.add(collations);
  registry.add(columns);
  registry.add(global_statements);
  registry.add(global_status);
  registry.add(global_variables);
  registry.add(index_parts);
  registry.add(indexes);
  registry.add(local_tables);
  registry.add(modules);
  registry.add(plugins);
  registry.add(processlist);
  registry.add(referential_constraints);
  registry.add(schema_names);
  registry.add(schemas);
  registry.add(session_statements);
  registry.add(session_status);
  registry.add(session_variables);
  registry.add(table_constraints);
  registry.add(table_status);
  registry.add(tables);
  
  return 0;
}

static int finalize(drizzled::plugin::Registry &registry)
{
  registry.remove(function_plugin);
  delete function_plugin;

  delete character_sets;
  delete collations;
  delete columns;
  delete global_statements;
  delete global_status;
  delete global_variables;
  delete index_parts;
  delete indexes;
  delete local_tables;
  delete modules;
  delete plugins;
  delete processlist;
  delete referential_constraints;
  delete schema_names;
  delete schemas;
  delete session_statements;
  delete session_status;
  delete session_variables;
  delete table_constraints;
  delete table_status;
  delete tables;

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "FunctionEngine",
  "1.0",
  "Brian Aker",
  "Function Engine provides the infrastructure for Table Functions,etc.",
  PLUGIN_LICENSE_GPL,
  init,     /* Plugin Init */
  finalize,     /* Plugin Deinit */
  NULL,               /* status variables */
  NULL,               /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
