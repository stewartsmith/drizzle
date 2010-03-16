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

#include "config.h"

#include "plugin/table_cache_dictionary/dictionary.h"
#include "drizzled/pthread_globals.h"

using namespace drizzled;
using namespace std;

table_cache_dictionary::TableDefinitionCache::TableDefinitionCache() :
  plugin::TableFunction("DATA_DICTIONARY", "TABLE_DEFINITION_CACHE")
{
  add_field("TABLE_SCHEMA");
  add_field("TABLE_NAME");
  add_field("VERSION", plugin::TableFunction::NUMBER);
  add_field("TABLE_COUNT", plugin::TableFunction::NUMBER);
  add_field("IS_NAME_LOCKED", plugin::TableFunction::BOOLEAN);
}

table_cache_dictionary::TableDefinitionCache::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg),
  is_primed(false)
{
  pthread_mutex_lock(&LOCK_open); /* Optionally lock for remove tables from open_cahe if not in use */
}

table_cache_dictionary::TableDefinitionCache::Generator::~Generator()
{
  pthread_mutex_unlock(&LOCK_open); /* Optionally lock for remove tables from open_cahe if not in use */
}

bool table_cache_dictionary::TableDefinitionCache::Generator::nextCore()
{
  if (is_primed)
  {
    table_share_iterator++;
  }
  else
  {
    is_primed= true;
    table_share_iterator= TableShare::getCache().begin();
  }

  if (table_share_iterator == TableShare::getCache().end())
    return false;

  share= (*table_share_iterator).second;

  return true;
}

bool table_cache_dictionary::TableDefinitionCache::Generator::next()
{
  while (not nextCore())
  {
    if (table_share_iterator != TableShare::getCache().end())
      continue;

    return false;
  }

  return true;
}

bool table_cache_dictionary::TableDefinitionCache::Generator::populate()
{
  if (not next())
    return false;
  
  fill();

  return true;
}

void table_cache_dictionary::TableDefinitionCache::Generator::fill()
{
  /**
    For test cases use:
    --replace_column 3 # 4 # 5 #
  */

  /* TABLE_SCHEMA 1 */
  string arg;
  push(share->getSchemaName(arg));

  /* TABLE_NAME  2 */
  push(share->getTableName(arg));

  /* VERSION 3 */
  push(static_cast<int64_t>(share->getVersion()));

  /* TABLE_COUNT 4 */
  push(static_cast<uint64_t>(share->getTableCount()));

  /* IS_NAME_LOCKED */
  push(share->isNameLock());
}
