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

using namespace std;

/*
 * Vectors of columns for various I_S tables.
 */
static vector<const ColumnInfo *> char_set_columns;
static vector<const ColumnInfo *> collation_columns;
static vector<const ColumnInfo *> coll_char_columns;
static vector<const ColumnInfo *> key_col_usage_columns;
static vector<const ColumnInfo *> processlist_columns;
static vector<const ColumnInfo *> ref_constraint_columns;
static vector<const ColumnInfo *> tab_constraints_columns;

/*
 * Methods for various I_S tables.
 */
static InfoSchemaMethods *char_set_methods= NULL;
static InfoSchemaMethods *collation_methods= NULL;
static InfoSchemaMethods *coll_char_methods= NULL;
static InfoSchemaMethods *key_col_usage_methods= NULL;
static InfoSchemaMethods *processlist_methods= NULL;
static InfoSchemaMethods *ref_constraint_methods= NULL;
static InfoSchemaMethods *tab_constraints_methods= NULL;

/*
 * I_S tables.
 */
static InfoSchemaTable *char_set_table= NULL;
static InfoSchemaTable *collation_table= NULL;
static InfoSchemaTable *coll_char_set_table= NULL;
static InfoSchemaTable *key_col_usage_table= NULL;
static InfoSchemaTable *processlist_table= NULL;
static InfoSchemaTable *ref_constraint_table= NULL;
static InfoSchemaTable *tab_constraints_table= NULL;

/**
 * Populate the vectors of columns for each I_S table.
 *
 * @return false on success; true on failure.
 */
bool initTableColumns()
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

  if ((retval= createKeyColUsageColumns(key_col_usage_columns)) == true)
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

  if ((retval= createTabConstraintsColumns(tab_constraints_columns)) == true)
  {
    return true;
  }

  return false;
}

/**
 * Clear the vectors of columns for each I_S table.
 */
void cleanupTableColumns()
{
  clearColumns(char_set_columns);
  clearColumns(collation_columns);
  clearColumns(coll_char_columns);
  clearColumns(key_col_usage_columns);
  clearColumns(processlist_columns);
  clearColumns(ref_constraint_columns);
  clearColumns(tab_constraints_columns);
}

/**
 * Initialize the methods for each I_S table.
 *
 * @return false on success; true on failure
 */
bool initTableMethods()
{
  if ((char_set_methods= new(std::nothrow) CharSetISMethods()) == NULL)
  {
    return true;
  }

  if ((collation_methods= new(std::nothrow) CollationISMethods()) == NULL)
  {
    return true;
  }

  if ((coll_char_methods= new(std::nothrow) CollCharISMethods()) == NULL)
  {
    return true;
  }

  if ((key_col_usage_methods= new(std::nothrow) KeyColUsageISMethods()) == NULL)
  {
    return true;
  }

  if ((processlist_methods= new(std::nothrow) ProcessListISMethods()) == NULL)
  {
    return true;
  }

  if ((ref_constraint_methods= new(std::nothrow) RefConstraintsISMethods()) == NULL)
  {
    return true;
  }

  if ((tab_constraints_methods= new(std::nothrow) TabConstraintsISMethods()) == NULL)
  {
    return true;
  }

  return false;
}

/**
 * Delete memory allocated for the I_S table methods.
 */
void cleanupTableMethods()
{
  delete char_set_methods;
  delete collation_methods;
  delete coll_char_methods;
  delete key_col_usage_methods;
  delete processlist_methods;
  delete ref_constraint_methods;
  delete tab_constraints_methods;
}

/**
 * Initialize the I_S tables.
 *
 * @return false on success; true on failure
 */
bool initTables()
{

  char_set_table= new(std::nothrow) InfoSchemaTable("CHARACTER_SETS",
                                                    char_set_columns,
                                                    -1, -1, false, false, 0,
                                                    char_set_methods);
  if (char_set_table == NULL)
  {
    return true;
  }

  collation_table= new(std::nothrow) InfoSchemaTable("COLLATIONS",
                                                     collation_columns,
                                                     -1, -1, false, false, 0,
                                                     collation_methods);
  if (collation_table == NULL)
  {
    return true;
  }

  coll_char_set_table= new(std::nothrow) InfoSchemaTable("COLLATION_CHARACTER_SET_APPLICABILITY",
                                                         coll_char_columns,
                                                         -1, -1, false, false, 0,
                                                         coll_char_methods);
  if (coll_char_set_table == NULL)
  {
    return true;
  }

  key_col_usage_table= new(std::nothrow) InfoSchemaTable("KEY_COLUMN_USAGE",
                                                         key_col_usage_columns,
                                                         4, 5, false, true,
                                                         OPEN_TABLE_ONLY,
                                                         key_col_usage_methods);
  if (key_col_usage_table == NULL)
  {
    return true;
  }

  processlist_table= new(std::nothrow) InfoSchemaTable("PROCESSLIST",
                                                       processlist_columns,
                                                       -1, -1, false, false, 0,
                                                       processlist_methods);
  if (processlist_table == NULL)
  {
    return true;
  }

  ref_constraint_table= new(std::nothrow) InfoSchemaTable("REFERENTIAL_CONSTRAINTS",
                                                          ref_constraint_columns,
                                                          1, 9, false, true,
                                                          OPEN_TABLE_ONLY,
                                                          ref_constraint_methods);
  if (ref_constraint_table == NULL)
  {
    return true;
  }

  tab_constraints_table= new(std::nothrow) InfoSchemaTable("TABLE_CONSTRAINTS",
                                                           tab_constraints_columns,
                                                           3, 4, false, true,
                                                           OPEN_TABLE_ONLY,
                                                           tab_constraints_methods);
  if (tab_constraints_table == NULL)
  {
    return true;
  }

  return false;
}

/**
 * Delete memory allocated for the I_S tables.
 */
void cleanupTables()
{
  delete char_set_table;
  delete collation_table;
  delete coll_char_set_table;
  delete key_col_usage_table;
  delete processlist_table;
  delete ref_constraint_table;
  delete tab_constraints_table;
}

/**
 * Initialize the I_S plugin.
 *
 * @param[in] registry the PluginRegistry singleton
 * @return 0 on success; 1 on failure.
 */
int infoSchemaInit(PluginRegistry& registry)
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

  registry.add(char_set_table);
  registry.add(collation_table);
  registry.add(coll_char_set_table);
  registry.add(key_col_usage_table);
  registry.add(processlist_table);
  registry.add(ref_constraint_table);
  registry.add(tab_constraints_table);

  return 0;
}

/**
 * Clean up the I_S plugin.
 *
 * @param[in] registry the PluginRegistry singleton
 * @return 0 on success; 1 on failure
 */
int infoSchemaDone(PluginRegistry& registry)
{
  registry.remove(char_set_table);
  registry.remove(collation_table);
  registry.remove(coll_char_set_table);
  registry.remove(key_col_usage_table);
  registry.remove(processlist_table);
  registry.remove(ref_constraint_table);
  registry.remove(tab_constraints_table);

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
