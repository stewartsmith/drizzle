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
 *   I_S plugin implementation.
 */

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/show.h>

#include "info_schema_methods.h"
#include "info_schema_columns.h"
#include "character_set.h"
#include "collation.h"
#include "collation_char_set.h"
#include "columns.h"
#include "key_column_usage.h"
#include "modules.h"
#include "open_tables.h"
#include "plugins.h"
#include "processlist.h"
#include "referential_constraints.h"
#include "schemata.h"
#include "table_constraints.h"
#include "tables.h"
#include "table_names.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for various I_S tables.
 */
static vector<const plugin::ColumnInfo *> stats_columns;
static vector<const plugin::ColumnInfo *> status_columns;

/*
 * Methods for various I_S tables.
 */
static plugin::InfoSchemaMethods *stats_methods= NULL;
static plugin::InfoSchemaMethods *status_methods= NULL;
static plugin::InfoSchemaMethods *variables_methods= NULL;

/*
 * I_S tables.
 */
static plugin::InfoSchemaTable *global_stat_table= NULL;
static plugin::InfoSchemaTable *global_var_table= NULL;
static plugin::InfoSchemaTable *sess_stat_table= NULL;
static plugin::InfoSchemaTable *sess_var_table= NULL;
static plugin::InfoSchemaTable *stats_table= NULL;
static plugin::InfoSchemaTable *status_table= NULL;
static plugin::InfoSchemaTable *var_table= NULL;

/**
 * Populate the vectors of columns for each I_S table.
 *
 * @return false on success; true on failure.
 */
static bool initTableColumns()
{
  bool retval= false;

  if ((retval= createStatsColumns(stats_columns)) == true)
  {
    return true;
  }

  if ((retval= createStatusColumns(status_columns)) == true)
  {
    return true;
  }

  return false;
}

/**
 * Clear the vectors of columns for each I_S table.
 */
static void cleanupTableColumns()
{
  clearColumns(stats_columns);
  clearColumns(status_columns);
}

/**
 * Initialize the methods for each I_S table.
 *
 * @return false on success; true on failure
 */
static bool initTableMethods()
{
  if ((stats_methods= new(nothrow) StatsISMethods()) == NULL)
  {
    return true;
  }

  if ((status_methods= new(nothrow) StatusISMethods()) == NULL)
  {
    return true;
  }

  if ((variables_methods= new(nothrow) VariablesISMethods()) == NULL)
  {
    return true;
  }

  return false;
}

/**
 * Delete memory allocated for the I_S table methods.
 */
static void cleanupTableMethods()
{
  delete stats_methods;
  delete status_methods;
  delete variables_methods;
}

/**
 * Initialize the I_S tables.
 *
 * @return false on success; true on failure
 */
static bool initTables()
{

  global_stat_table= new(nothrow) plugin::InfoSchemaTable("GLOBAL_STATUS",
                                                          status_columns,
                                                          -1, -1, false, false,
                                                          0, status_methods);
  if (global_stat_table == NULL)
  {
    return true;
  }

  global_var_table= new(nothrow) plugin::InfoSchemaTable("GLOBAL_VARIABLES",
                                                         status_columns,
                                                         -1, -1, false, false,
                                                         0, variables_methods);
  if (global_var_table == NULL)
  {
    return true;
  }

  sess_stat_table= new(nothrow) plugin::InfoSchemaTable("SESSION_STATUS",
                                                        status_columns,
                                                        -1, -1, false, false,
                                                        0, status_methods);
  if (sess_stat_table == NULL)
  {
    return true;
  }

  sess_var_table= new(nothrow) plugin::InfoSchemaTable("SESSION_VARIABLES",
                                                       status_columns,
                                                       -1, -1, false, false, 0,
                                                       variables_methods);
  if (sess_var_table == NULL)
  {
    return true;
  }

  stats_table= new(nothrow) plugin::InfoSchemaTable("STATISTICS",
                                                    stats_columns,
                                                    1, 2, false, true,
                                                    OPEN_TABLE_ONLY | OPTIMIZE_I_S_TABLE,
                                                    stats_methods);
  if (stats_table == NULL)
  {
    return true;
  }

  status_table= new(nothrow) plugin::InfoSchemaTable("STATUS",
                                                     status_columns,
                                                     -1, -1, true, false, 0,
                                                     status_methods);
  if (status_table == NULL)
  {
    return true;
  }

  var_table= new(nothrow) plugin::InfoSchemaTable("VARIABLES",
                                                  status_columns,
                                                  -1, -1, true, false, 0,
                                                  variables_methods);
  if (var_table == NULL)
  {
    return true;
  }

  return false;
}

/**
 * Delete memory allocated for the I_S tables.
 */
static void cleanupTables()
{
  delete global_stat_table;
  delete global_var_table;
  delete sess_stat_table;
  delete sess_var_table;
  delete stats_table;
  delete status_table;
  delete var_table;
}

/**
 * Initialize the I_S plugin.
 *
 * @param[in] registry the drizzled::plugin::Registry singleton
 * @return 0 on success; 1 on failure.
 */
static int infoSchemaInit(drizzled::plugin::Registry& registry)
{
  bool retval= false;

  if ((retval= initTableMethods()) == true)
  {
    return 1;
  }

  if ((retval= initTableColumns()) == true)
  {
    return 1;
  }

  if ((retval= initTables()) == true)
  {
    return 1;
  }

  registry.add(CharacterSetIS::getTable());
  registry.add(CollationIS::getTable());
  registry.add(CollationCharSetIS::getTable());
  registry.add(ColumnsIS::getTable());
  registry.add(KeyColumnUsageIS::getTable());

  registry.add(global_stat_table);
  registry.add(global_var_table);

  registry.add(OpenTablesIS::getTable());
  registry.add(ModulesIS::getTable());
  registry.add(PluginsIS::getTable());
  registry.add(ProcessListIS::getTable());
  registry.add(ReferentialConstraintsIS::getTable());
  registry.add(SchemataIS::getTable());

  registry.add(sess_stat_table);
  registry.add(sess_var_table);
  registry.add(stats_table);
  registry.add(status_table);

  registry.add(TableConstraintsIS::getTable());
  registry.add(TablesIS::getTable());
  registry.add(TableNamesIS::getTable());

  registry.add(var_table);

  return 0;
}

/**
 * Clean up the I_S plugin.
 *
 * @param[in] registry the drizzled::plugin::Registry singleton
 * @return 0 on success; 1 on failure
 */
static int infoSchemaDone(drizzled::plugin::Registry& registry)
{
  registry.remove(CharacterSetIS::getTable());
  CharacterSetIS::cleanup();

  registry.remove(CollationIS::getTable());
  CollationIS::cleanup();

  registry.remove(CollationCharSetIS::getTable());
  CollationCharSetIS::cleanup();

  registry.remove(ColumnsIS::getTable());
  ColumnsIS::cleanup();

  registry.remove(KeyColumnUsageIS::getTable());
  KeyColumnUsageIS::cleanup();

  registry.remove(global_stat_table);
  registry.remove(global_var_table);

  registry.remove(OpenTablesIS::getTable());
  OpenTablesIS::cleanup();

  registry.remove(ModulesIS::getTable());
  ModulesIS::cleanup();

  registry.remove(PluginsIS::getTable());
  PluginsIS::cleanup();

  registry.remove(ProcessListIS::getTable());
  ProcessListIS::cleanup();

  registry.remove(ReferentialConstraintsIS::getTable());
  ReferentialConstraintsIS::cleanup();

  registry.remove(SchemataIS::getTable());
  SchemataIS::cleanup();

  registry.remove(sess_stat_table);
  registry.remove(sess_var_table);
  registry.remove(stats_table);
  registry.remove(status_table);

  registry.remove(TableConstraintsIS::getTable());
  TableConstraintsIS::cleanup();

  registry.remove(TablesIS::getTable());
  TablesIS::cleanup();

  registry.remove(TableNamesIS::getTable());
  TableNamesIS::cleanup();

  registry.remove(var_table);

  cleanupTableMethods();
  cleanupTableColumns();
  cleanupTables();

  return 0;
}

drizzle_declare_plugin(info_schema)
{
  "info_schema",
  "1.0",
  "Padraig O'Sullivan",
  "I_S plugin",
  PLUGIN_LICENSE_GPL,
  infoSchemaInit,
  infoSchemaDone,
  NULL,
  NULL,
  NULL
}
drizzle_declare_plugin_end;
