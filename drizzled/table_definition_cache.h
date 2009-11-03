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

#ifndef DRIZZLED_TABLE_DEFINITION_CACHE_H
#define DRIZZLED_TABLE_DEFINITION_CACHE_H

#include "drizzled/plugin/storage_engine.h"

namespace drizzled
{

class TableDefinitionCache
{
  /* Disallow these */
  TableDefinitionCache(const TableDefinitionCache &other);
  TableDefinitionCache operator=(const TableDefinitionCache &other);
public:
  explicit TableDefinitionCache();
  ~TableDefinitionCache();
  /**
   * Fills a supplied Table Definition from a cached Table
   * Definition.  Returns true if the cached table definition
   * was found, false otherwise.
   *
   * @param[out] Table definition to fill.
   * @param[in] Cache key for table definition
   */
  bool fillTableDefinitionFromCache(plugin::StorageEngine::TableDefinition *to_fill,
                                    const std::string &cache_key);

  /**
   * Adds a new table definition to the cache
   *
   * @param Table definition to add
   * @param Cache key
   */
  void addTableDefinitionToCache(const plugin::StorageEngine::TableDefinition &to_add,
                                 const std::string &cache_key);

  /**
   * Removes a new table definition from the cache
   *
   * @param Cache key to remove
   */
  void removeTableDefinitionFromCache(const std::string &cache_key);
private:
  plugin::StorageEngine::TableDefinitions table_definitions;
  pthread_mutex_t table_definitions_mutex;
};

} /* namespace drizzled */

#endif /* DRIZZLED_TABLE_DEFINITION_CACHE_H */
