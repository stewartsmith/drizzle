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

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
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
#include "statistics.h"
#include "status.h"
#include "variables.h"

#include <vector>

using namespace drizzled;
using namespace std;

/**
 * Initialize the I_S plugin.
 *
 * @param[in] registry the drizzled::plugin::Registry singleton
 * @return 0 on success; 1 on failure.
 */
static int infoSchemaInit(drizzled::plugin::Registry& registry)
{
  registry.add(CharacterSetIS::getTable());
  registry.add(CollationIS::getTable());
  registry.add(CollationCharSetIS::getTable());
  registry.add(ColumnsIS::getTable());
  registry.add(KeyColumnUsageIS::getTable());
  registry.add(GlobalStatusIS::getTable());
  registry.add(GlobalVariablesIS::getTable());
  registry.add(OpenTablesIS::getTable());
  registry.add(ModulesIS::getTable());
  registry.add(PluginsIS::getTable());
  registry.add(ProcessListIS::getTable());
  registry.add(ReferentialConstraintsIS::getTable());
  registry.add(SchemataIS::getTable());
  registry.add(SessionStatusIS::getTable());
  registry.add(SessionVariablesIS::getTable());
  registry.add(StatisticsIS::getTable());
  registry.add(StatusIS::getTable());
  registry.add(TableConstraintsIS::getTable());
  registry.add(TablesIS::getTable());
  registry.add(TableNamesIS::getTable());
  registry.add(VariablesIS::getTable());

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

  registry.remove(GlobalStatusIS::getTable());
  GlobalStatusIS::cleanup();

  registry.remove(GlobalVariablesIS::getTable());
  GlobalVariablesIS::cleanup();

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

  registry.remove(SessionStatusIS::getTable());
  SessionStatusIS::cleanup();

  registry.remove(SessionVariablesIS::getTable());
  SessionVariablesIS::cleanup();

  registry.remove(StatisticsIS::getTable());
  StatisticsIS::cleanup();

  registry.remove(StatusIS::getTable());
  StatusIS::cleanup();

  registry.remove(TableConstraintsIS::getTable());
  TableConstraintsIS::cleanup();

  registry.remove(TablesIS::getTable());
  TablesIS::cleanup();

  registry.remove(TableNamesIS::getTable());
  TableNamesIS::cleanup();

  registry.remove(VariablesIS::getTable());
  VariablesIS::cleanup();

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "info_schema",
  "1.1",
  "Padraig O'Sullivan",
  "I_S plugin",
  PLUGIN_LICENSE_GPL,
  infoSchemaInit,
  infoSchemaDone,
  NULL,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
