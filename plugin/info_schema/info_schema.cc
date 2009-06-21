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
 * @file Implementation of the I_S tables.
 */

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/show.h>

#include "info_schema_methods.h"
#include "info_schema_columns.h"

using namespace std;

/*
 * List of vectors of columns for various I_S tables.
 */
static vector<const ColumnInfo *> processlist_columns;

/*
 * List of methods for various I_S tables.
 */
static InfoSchemaMethods *processlist_methods= NULL;

/*
 * List of I_S tables.
 */
static InfoSchemaTable *processlist_table= NULL;

/**
 * Populate the vectors of columns for each I_S table.
 *
 * @return false on success; true on failure.
 */
bool initTableColumns()
{
  createProcessListColumns(processlist_columns);

  return false;
}

/**
 * Clear the vectors of columns for each I_S table.
 */
void cleanupTableColumns()
{
  clearColumns(processlist_columns);
}

/**
 * Initialize the methods for each I_S table.
 *
 * @return false on success; true on failure
 */
bool initTableMethods()
{
  processlist_methods= new ProcessListISMethods();

  return false;
}

/**
 * Delete memory allocated for the I_S table methods.
 */
void cleanupTableMethods()
{
  delete processlist_methods;
}

/**
 * Initialize the I_S tables.
 *
 * @return false on success; true on failure
 */
bool initTables()
{

  processlist_table= new InfoSchemaTable("PROCESSLIST",
                                         processlist_columns,
                                         -1, -1, false, false, 0,
                                         processlist_methods);

  return false;
}

/**
 * Delete memory allocated for the I_S tables.
 */
void cleanupTables()
{
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
