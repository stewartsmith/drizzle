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
static vector<const ColumnInfo *> processlist_columns;

/*
 * Methods for various I_S tables.
 */
static InfoSchemaMethods *char_set_methods= NULL;
static InfoSchemaMethods *collation_methods= NULL;
static InfoSchemaMethods *processlist_methods= NULL;

/*
 * I_S tables.
 */
static InfoSchemaTable *char_set_table= NULL;
static InfoSchemaTable *collation_table= NULL;
static InfoSchemaTable *processlist_table= NULL;

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

  if ((retval= createProcessListColumns(processlist_columns)) == true)
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
  clearColumns(processlist_columns);
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

  if ((processlist_methods= new(std::nothrow) ProcessListISMethods()) == NULL)
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
  delete processlist_methods;
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

  processlist_table= new(std::nothrow) InfoSchemaTable("PROCESSLIST",
                                                       processlist_columns,
                                                       -1, -1, false, false, 0,
                                                       processlist_methods);
  if (processlist_table == NULL)
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
  delete processlist_table;
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
  registry.add(processlist_table);

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
  registry.remove(processlist_table);

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
