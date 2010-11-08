/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2010 Sun Microsystems
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

#ifndef DRIZZLED_TABLE_CACHE_H
#define DRIZZLED_TABLE_CACHE_H

#include <boost/unordered_map.hpp>

namespace drizzled {
namespace table {

class Concurrent;

typedef boost::unordered_multimap< TableIdentifier::Key, Concurrent *> CacheMap;
typedef std::pair< CacheMap::const_iterator, CacheMap::const_iterator > CacheRange;

class Cache 
{
  CacheMap cache;

public:
  static inline Cache &singleton()
  {
    static Cache open_cache;

    return open_cache;
  }

  CacheMap &getCache()
  {
    return cache;
  }

  bool areTablesUsed(Table *table, bool wait_for_name_lock);
  void removeSchema(const SchemaIdentifier &schema_identifier);
  bool removeTable(Session *session, TableIdentifier &identifier, uint32_t flags);
  void release(TableShare *share);
  bool insert(table::Concurrent *arg);
};

CacheMap &getCache(void);
void remove_table(table::Concurrent *arg);

} /* namepsace table */
} /* namepsace drizzled */

#endif /* DRIZZLED_TABLE_CACHE_H */
