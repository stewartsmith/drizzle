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

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
#include "key_column_usage.h"
#include "referential_constraints.h"
#include "table_constraints.h"
#include "statistics.h"

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
  registry.add(ReferentialConstraintsIS::getTable());
  registry.add(TableConstraintsIS::getTable());
  registry.add(KeyColumnUsageIS::getTable());
  registry.add(StatisticsIS::getTable());
#if 0
#endif

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
  registry.remove(KeyColumnUsageIS::getTable());
  KeyColumnUsageIS::cleanup();

  registry.remove(ReferentialConstraintsIS::getTable());
  ReferentialConstraintsIS::cleanup();

  registry.remove(StatisticsIS::getTable());
  StatisticsIS::cleanup();

  registry.remove(TableConstraintsIS::getTable());
  TableConstraintsIS::cleanup();

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
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
