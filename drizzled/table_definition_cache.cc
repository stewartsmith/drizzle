/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#include "drizzled/server_includes.h"
#include "drizzled/table_definition_cache.h"

using namespace std;

namespace drizzled
{

TableDefinitionCache::TableDefinitionCache()
{
  (void) pthread_mutex_init(&table_definitions_mutex, NULL);
}

TableDefinitionCache::~TableDefinitionCache()
{
  (void) pthread_mutex_destroy(&table_definitions_mutex);
}

bool TableDefinitionCache::fillTableDefinitionFromCache(plugin::StorageEngine::TableDefinition *to_fill,
                                                        const string &cache_key)
{
  bool found= false;
  plugin::StorageEngine::TableDefinitions::const_iterator iter;

  pthread_mutex_lock(&table_definitions_mutex);
  iter= table_definitions.find(cache_key);

  if (iter != table_definitions.end())
  {
    if (to_fill != NULL)
      to_fill->CopyFrom(((*iter).second));
    found= true;
  }
  pthread_mutex_unlock(&table_definitions_mutex);
  return found;
}

void TableDefinitionCache::addTableDefinitionToCache(const plugin::StorageEngine::TableDefinition &to_add,
                                                     const string &cache_key)
{
  pthread_mutex_lock(&table_definitions_mutex);
  if (table_definitions.find(cache_key) == table_definitions.end())
  {
    table_definitions[cache_key]= to_add;
  }
  pthread_mutex_unlock(&table_definitions_mutex);
}

void TableDefinitionCache::removeTableDefinitionFromCache(const string &cache_key)
{
  pthread_mutex_lock(&table_definitions_mutex);

  plugin::StorageEngine::TableDefinitions::iterator iter;

  iter= table_definitions.find(cache_key);
  if (iter != table_definitions.end())
  {
    table_definitions.erase(iter);
  }
  pthread_mutex_unlock(&table_definitions_mutex);
}

} /* namespace drizzled */
