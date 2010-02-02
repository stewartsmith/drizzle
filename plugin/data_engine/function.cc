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

#include <plugin/data_engine/function.h>
#include <plugin/data_engine/cursor.h>
#include <drizzled/table_function_container.h>

#include <string>

using namespace std;
using namespace drizzled;

static TableFunctionContainer table_functions;

Function::Function(const std::string &name_arg) :
  drizzled::plugin::StorageEngine(name_arg,
                                  HTON_ALTER_NOT_SUPPORTED |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED),
  global_statements(true),
  session_statements(false),
  global_status(true),
  session_status(false),
  global_variables(true),
  session_variables(false)
{
  table_functions.addTool(character_sets);
  table_functions.addTool(collations);
  table_functions.addTool(columns);
  table_functions.addTool(global_statements);
  table_functions.addTool(global_status);
  table_functions.addTool(global_variables);
  table_functions.addTool(index_parts);
  table_functions.addTool(indexes);
  table_functions.addTool(modules);
  table_functions.addTool(plugins);
  table_functions.addTool(processlist);
  table_functions.addTool(referential_constraints);
  table_functions.addTool(schemas);
  table_functions.addTool(session_statements);
  table_functions.addTool(session_status);
  table_functions.addTool(session_variables);
  table_functions.addTool(table_constraints);
  table_functions.addTool(tables);
}


Cursor *Function::create(TableShare &table, memory::Root *mem_root)
{
  return new (mem_root) FunctionCursor(*this, table);
}

Tool *Function::getTool(const char *path)
{
  return table_functions.getTool(path);
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

  Tool *tool= table_functions.getTool(tab_name);

  if (not tool)
  {
    return ENOENT;
  }

  if (table_proto)
  {
    tool->define(*table_proto);
  }

  return EEXIST;
}


void Function::doGetTableNames(drizzled::CachedDirectory&, 
                                        string &db, 
                                        set<string> &set_of_names)
{
  table_functions.getNames(db, set_of_names);
}


static plugin::StorageEngine *function_plugin= NULL;

static int init(plugin::Registry &registry)
{
  function_plugin= new(std::nothrow) Function(engine_name);
  if (! function_plugin)
  {
    return 1;
  }

  registry.add(function_plugin);
  
  return 0;
}

static int finalize(plugin::Registry &registry)
{
  registry.remove(function_plugin);
  delete function_plugin;

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "DICTIONARY",
  "1.0",
  "Brian Aker",
  "Function provides the information for data_dictionary,etc.",
  PLUGIN_LICENSE_GPL,
  init,     /* Plugin Init */
  finalize,     /* Plugin Deinit */
  NULL,               /* status variables */
  NULL,               /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
