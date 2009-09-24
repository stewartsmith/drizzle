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

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for various I_S tables.
 */
static vector<const plugin::ColumnInfo *> char_set_columns;
static vector<const plugin::ColumnInfo *> collation_columns;
static vector<const plugin::ColumnInfo *> coll_char_columns;
static vector<const plugin::ColumnInfo *> col_columns;
static vector<const plugin::ColumnInfo *> key_col_usage_columns;
static vector<const plugin::ColumnInfo *> open_tab_columns;
static vector<const plugin::ColumnInfo *> plugin_columns;
static vector<const plugin::ColumnInfo *> processlist_columns;
static vector<const plugin::ColumnInfo *> ref_constraint_columns;
static vector<const plugin::ColumnInfo *> schemata_columns;
static vector<const plugin::ColumnInfo *> stats_columns;
static vector<const plugin::ColumnInfo *> status_columns;
static vector<const plugin::ColumnInfo *> tab_constraints_columns;
static vector<const plugin::ColumnInfo *> tables_columns;
static vector<const plugin::ColumnInfo *> tab_names_columns;

/*
 * Methods for various I_S tables.
 */
static plugin::InfoSchemaMethods *char_set_methods= NULL;
static plugin::InfoSchemaMethods *collation_methods= NULL;
static plugin::InfoSchemaMethods *coll_char_methods= NULL;
static plugin::InfoSchemaMethods *columns_methods= NULL;
static plugin::InfoSchemaMethods *key_col_usage_methods= NULL;
static plugin::InfoSchemaMethods *open_tab_methods= NULL;
static plugin::InfoSchemaMethods *plugins_methods= NULL;
static plugin::InfoSchemaMethods *processlist_methods= NULL;
static plugin::InfoSchemaMethods *ref_constraint_methods= NULL;
static plugin::InfoSchemaMethods *schemata_methods= NULL;
static plugin::InfoSchemaMethods *stats_methods= NULL;
static plugin::InfoSchemaMethods *status_methods= NULL;
static plugin::InfoSchemaMethods *tab_constraints_methods= NULL;
static plugin::InfoSchemaMethods *tables_methods= NULL;
static plugin::InfoSchemaMethods *tab_names_methods= NULL;
static plugin::InfoSchemaMethods *variables_methods= NULL;

/*
 * I_S tables.
 */
static plugin::InfoSchemaTable *char_set_table= NULL;
static plugin::InfoSchemaTable *collation_table= NULL;
static plugin::InfoSchemaTable *coll_char_set_table= NULL;
static plugin::InfoSchemaTable *columns_table= NULL;
static plugin::InfoSchemaTable *key_col_usage_table= NULL;
static plugin::InfoSchemaTable *global_stat_table= NULL;
static plugin::InfoSchemaTable *global_var_table= NULL;
static plugin::InfoSchemaTable *open_tab_table= NULL;
static plugin::InfoSchemaTable *plugins_table= NULL;
static plugin::InfoSchemaTable *processlist_table= NULL;
static plugin::InfoSchemaTable *ref_constraint_table= NULL;
static plugin::InfoSchemaTable *schemata_table= NULL;
static plugin::InfoSchemaTable *sess_stat_table= NULL;
static plugin::InfoSchemaTable *sess_var_table= NULL;
static plugin::InfoSchemaTable *stats_table= NULL;
static plugin::InfoSchemaTable *status_table= NULL;
static plugin::InfoSchemaTable *tab_constraints_table= NULL;
static plugin::InfoSchemaTable *tables_table= NULL;
static plugin::InfoSchemaTable *tab_names_table= NULL;
static plugin::InfoSchemaTable *var_table= NULL;

/**
 * Populate the vectors of columns for each I_S table.
 *
 * @return false on success; true on failure.
 */
static bool initTableColumns()
{
  bool retval= false;

  if ((retval= createCharSetColumns(char_set_columns)) == true)
  {
    return true;
  }

  if ((retval= createCollationColumns(collation_columns)) == true)
  {
    return true;
  }

  if ((retval= createCollCharSetColumns(coll_char_columns)) == true)
  {
    return true;
  }

  if ((retval= createColColumns(col_columns)) == true)
  {
    return true;
  }

  if ((retval= createKeyColUsageColumns(key_col_usage_columns)) == true)
  {
    return true;
  }

  if ((retval= createOpenTabColumns(open_tab_columns)) == true)
  {
    return true;
  }

  if ((retval= createPluginsColumns(plugin_columns)) == true)
  {
    return true;
  }

  if ((retval= createProcessListColumns(processlist_columns)) == true)
  {
    return true;
  }

  if ((retval= createRefConstraintColumns(ref_constraint_columns)) == true)
  {
    return true;
  }

  if ((retval= createSchemataColumns(schemata_columns)) == true)
  {
    return true;
  }

  if ((retval= createStatsColumns(stats_columns)) == true)
  {
    return true;
  }

  if ((retval= createStatusColumns(status_columns)) == true)
  {
    return true;
  }

  if ((retval= createTabConstraintsColumns(tab_constraints_columns)) == true)
  {
    return true;
  }

  if ((retval= createTablesColumns(tables_columns)) == true)
  {
    return true;
  }

  if ((retval= createTabNamesColumns(tab_names_columns)) == true)
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
  clearColumns(char_set_columns);
  clearColumns(collation_columns);
  clearColumns(coll_char_columns);
  clearColumns(col_columns);
  clearColumns(key_col_usage_columns);
  clearColumns(open_tab_columns);
  clearColumns(plugin_columns);
  clearColumns(processlist_columns);
  clearColumns(ref_constraint_columns);
  clearColumns(schemata_columns);
  clearColumns(stats_columns);
  clearColumns(status_columns);
  clearColumns(tab_constraints_columns);
  clearColumns(tables_columns);
  clearColumns(tab_names_columns);
}

/**
 * Initialize the methods for each I_S table.
 *
 * @return false on success; true on failure
 */
static bool initTableMethods()
{
  if ((char_set_methods= new(nothrow) CharSetISMethods()) == NULL)
  {
    return true;
  }

  if ((collation_methods= new(nothrow) CollationISMethods()) == NULL)
  {
    return true;
  }

  if ((coll_char_methods= new(nothrow) CollCharISMethods()) == NULL)
  {
    return true;
  }

  if ((columns_methods= new(nothrow) ColumnsISMethods()) == NULL)
  {
    return true;
  }

  if ((key_col_usage_methods= new(nothrow) KeyColUsageISMethods()) == NULL)
  {
    return true;
  }

  if ((open_tab_methods= new(nothrow) OpenTablesISMethods()) == NULL)
  {
    return true;
  }

  if ((plugins_methods= new(nothrow) PluginsISMethods()) == NULL)
  {
    return true;
  }

  if ((processlist_methods= new(nothrow) ProcessListISMethods()) == NULL)
  {
    return true;
  }

  if ((ref_constraint_methods= new(nothrow) RefConstraintsISMethods()) == NULL)
  {
    return true;
  }

  if ((schemata_methods= new(nothrow) SchemataISMethods()) == NULL)
  {
    return true;
  }

  if ((stats_methods= new(nothrow) StatsISMethods()) == NULL)
  {
    return true;
  }

  if ((status_methods= new(nothrow) StatusISMethods()) == NULL)
  {
    return true;
  }

  if ((tab_constraints_methods= new(nothrow) TabConstraintsISMethods()) == NULL)
  {
    return true;
  }

  if ((tables_methods= new(nothrow) TablesISMethods()) == NULL)
  {
    return true;
  }

  if ((tab_names_methods= new(nothrow) TabNamesISMethods()) == NULL)
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
  delete char_set_methods;
  delete collation_methods;
  delete coll_char_methods;
  delete columns_methods;
  delete key_col_usage_methods;
  delete open_tab_methods;
  delete plugins_methods;
  delete processlist_methods;
  delete ref_constraint_methods;
  delete schemata_methods;
  delete stats_methods;
  delete status_methods;
  delete tab_constraints_methods;
  delete tables_methods;
  delete tab_names_methods;
  delete variables_methods;
}

/**
 * Initialize the I_S tables.
 *
 * @return false on success; true on failure
 */
static bool initTables()
{

  char_set_table= new(nothrow) plugin::InfoSchemaTable("CHARACTER_SETS",
                                                    char_set_columns,
                                                    -1, -1, false, false, 0,
                                                    char_set_methods);
  if (char_set_table == NULL)
  {
    return true;
  }

  collation_table= new(nothrow) plugin::InfoSchemaTable("COLLATIONS",
                                                     collation_columns,
                                                     -1, -1, false, false, 0,
                                                     collation_methods);
  if (collation_table == NULL)
  {
    return true;
  }

  coll_char_set_table= new(nothrow) plugin::InfoSchemaTable("COLLATION_CHARACTER_SET_APPLICABILITY",
                                                         coll_char_columns,
                                                         -1, -1, false, false, 0,
                                                         coll_char_methods);
  if (coll_char_set_table == NULL)
  {
    return true;
  }

  columns_table= new(nothrow) plugin::InfoSchemaTable("COLUMNS",
                                                   col_columns,
                                                   1, 2, false, true,
                                                   OPTIMIZE_I_S_TABLE,
                                                   columns_methods);
  if (columns_table == NULL)
  {
    return true;
  }

  key_col_usage_table= new(nothrow) plugin::InfoSchemaTable("KEY_COLUMN_USAGE",
                                                         key_col_usage_columns,
                                                         4, 5, false, true,
                                                         OPEN_TABLE_ONLY,
                                                         key_col_usage_methods);
  if (key_col_usage_table == NULL)
  {
    return true;
  }

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
  
  open_tab_table= new(nothrow) plugin::InfoSchemaTable("OPEN_TABLES",
                                                    open_tab_columns,
                                                    -1, -1, true, false, 0,
                                                    open_tab_methods);
  if (open_tab_table == NULL)
  {
    return true;
  }

  plugins_table= new(nothrow) plugin::InfoSchemaTable("PLUGINS",
                                                   plugin_columns,
                                                   -1, -1, false, false, 0,
                                                   plugins_methods);
  if (plugins_table == NULL)
  {
    return true;
  }

  processlist_table= new(nothrow) plugin::InfoSchemaTable("PROCESSLIST",
                                                       processlist_columns,
                                                       -1, -1, false, false, 0,
                                                       processlist_methods);
  if (processlist_table == NULL)
  {
    return true;
  }

  ref_constraint_table= new(nothrow) plugin::InfoSchemaTable("REFERENTIAL_CONSTRAINTS",
                                                          ref_constraint_columns,
                                                          1, 9, false, true,
                                                          OPEN_TABLE_ONLY,
                                                          ref_constraint_methods);
  if (ref_constraint_table == NULL)
  {
    return true;
  }

  schemata_table= new(nothrow) plugin::InfoSchemaTable("SCHEMATA",
                                                    schemata_columns,
                                                    1, -1, false, false, 0,
                                                    schemata_methods);
  if (schemata_table == NULL)
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

  tab_constraints_table= new(nothrow) plugin::InfoSchemaTable("TABLE_CONSTRAINTS",
                                                           tab_constraints_columns,
                                                           3, 4, false, true,
                                                           OPEN_TABLE_ONLY,
                                                           tab_constraints_methods);
  if (tab_constraints_table == NULL)
  {
    return true;
  }

  tables_table= new(nothrow) plugin::InfoSchemaTable("TABLES",
                                                  tables_columns,
                                                  1, 2, false, true,
                                                  OPTIMIZE_I_S_TABLE,
                                                  tables_methods);
  if (tables_table == NULL)
  {
    return true;
  }

  tab_names_table= new(nothrow) plugin::InfoSchemaTable("TABLE_NAMES",
                                                     tab_names_columns,
                                                     1, 2, true, true, 0,
                                                     tab_names_methods);
  if (tab_names_table == NULL)
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
  delete char_set_table;
  delete collation_table;
  delete coll_char_set_table;
  delete columns_table;
  delete key_col_usage_table;
  delete global_stat_table;
  delete global_var_table;
  delete open_tab_table;
  delete plugins_table;
  delete processlist_table;
  delete ref_constraint_table;
  delete schemata_table;
  delete sess_stat_table;
  delete sess_var_table;
  delete stats_table;
  delete status_table;
  delete tab_constraints_table;
  delete tables_table;
  delete tab_names_table;
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

  registry.info_schema.add(char_set_table);
  registry.info_schema.add(collation_table);
  registry.info_schema.add(coll_char_set_table);
  registry.info_schema.add(columns_table);
  registry.info_schema.add(key_col_usage_table);
  registry.info_schema.add(global_stat_table);
  registry.info_schema.add(global_var_table);
  registry.info_schema.add(open_tab_table);
  registry.info_schema.add(plugins_table);
  registry.info_schema.add(processlist_table);
  registry.info_schema.add(ref_constraint_table);
  registry.info_schema.add(schemata_table);
  registry.info_schema.add(sess_stat_table);
  registry.info_schema.add(sess_var_table);
  registry.info_schema.add(stats_table);
  registry.info_schema.add(status_table);
  registry.info_schema.add(tab_constraints_table);
  registry.info_schema.add(tables_table);
  registry.info_schema.add(tab_names_table);
  registry.info_schema.add(var_table);

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
  registry.info_schema.remove(char_set_table);
  registry.info_schema.remove(collation_table);
  registry.info_schema.remove(coll_char_set_table);
  registry.info_schema.remove(columns_table);
  registry.info_schema.remove(key_col_usage_table);
  registry.info_schema.remove(global_stat_table);
  registry.info_schema.remove(global_var_table);
  registry.info_schema.remove(open_tab_table);
  registry.info_schema.remove(plugins_table);
  registry.info_schema.remove(processlist_table);
  registry.info_schema.remove(ref_constraint_table);
  registry.info_schema.remove(schemata_table);
  registry.info_schema.remove(sess_stat_table);
  registry.info_schema.remove(sess_var_table);
  registry.info_schema.remove(stats_table);
  registry.info_schema.remove(status_table);
  registry.info_schema.remove(tab_constraints_table);
  registry.info_schema.remove(tables_table);
  registry.info_schema.remove(tab_names_table);
  registry.info_schema.remove(var_table);

  cleanupTableMethods();
  cleanupTableColumns();
  cleanupTables();

  return 0;
}

drizzle_declare_plugin(info_schema)
{
  "info_schema",
  "0.1",
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
