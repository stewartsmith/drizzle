/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

/**
 * @file 
 *   Plugins I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
#include "plugins.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the plugins I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the plugins I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * plugins I_S table.
 */
static plugin::InfoSchemaTable *plugins_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *PluginsIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  columns->push_back(new plugin::ColumnInfo("PLUGIN_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Name",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("PLUGIN_TYPE",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("IS_ACTIVE",
                                            3,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("MODULE_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Name",
                                            SKIP_OPEN_TABLE));
  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *PluginsIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new PluginsISMethods();
  }

  if (plugins_table == NULL)
  {
    plugins_table= new plugin::InfoSchemaTable("PLUGINS",
                                               *columns,
                                               -1, -1, false, false, 0,
                                               methods);
  }

  return plugins_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void PluginsIS::cleanup()
{
  clearColumns(*columns);
  delete plugins_table;
  delete methods;
  delete columns;
}

class ShowPlugins
 : public unary_function<pair<const string, const drizzled::plugin::Plugin *>, bool>
{
  Session *session;
  Table *table;
public:
  ShowPlugins(Session *session_arg, Table *table_arg)
    : session(session_arg), table(table_arg) {}

  result_type operator() (argument_type plugin)
  {
    const CHARSET_INFO * const cs= system_charset_info;

    table->restoreRecordAsDefault();

    table->field[0]->store(plugin.first.c_str(),
                           plugin.first.size(), cs);

    table->field[1]->store(plugin.second->getTypeName().c_str(),
                           plugin.second->getTypeName().size(), cs);

    if (plugin.second->isActive())
    {
      table->field[2]->store(STRING_WITH_LEN("YES"),cs);
    }
    else
    {
      table->field[2]->store(STRING_WITH_LEN("NO"), cs);
    }

    table->field[3]->store(plugin.second->getModuleName().c_str(),
                           plugin.second->getModuleName().size(), cs);

    return schema_table_store_record(session, table);
  }
};

int PluginsISMethods::fillTable(Session *session, TableList *tables)
{
  Table *table= tables->table;

  drizzled::plugin::Registry &registry= drizzled::plugin::Registry::singleton();
  const map<string, const drizzled::plugin::Plugin *> &plugin_map=
    registry.getPluginsMap();
  map<string, const drizzled::plugin::Plugin *>::const_iterator iter=
    find_if(plugin_map.begin(), plugin_map.end(), ShowPlugins(session, table));
  if (iter != plugin_map.end())
  {
    return 1;
  }
  return (0);
}
