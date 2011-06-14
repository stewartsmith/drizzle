/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <config.h>
#include <plugin/table_cache_dictionary/dictionary.h>

using namespace drizzled;

static table_cache_dictionary::TableCache *tables_in_cache;
static table_cache_dictionary::TableDefinitionCache *table_definitions;

static int init(drizzled::module::Context &context)
{
  table_definitions= new table_cache_dictionary::TableDefinitionCache;
  tables_in_cache= new table_cache_dictionary::TableCache;

  context.add(table_definitions);
  context.add(tables_in_cache);
  
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "Table Cache Dictionary",
  "1.0",
  "Brian Aker",
  "Data Dictionary for table and table definition cache.",
  PLUGIN_LICENSE_GPL,
  init,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
