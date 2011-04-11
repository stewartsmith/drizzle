/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2010 Sun Microsystems, Inc.
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

#pragma once

#include <boost/unordered_map.hpp>
#include <drizzled/identifier.h>

namespace drizzled {
namespace table {

typedef boost::unordered_multimap< identifier::Table::Key, Concurrent *> CacheMap;
typedef std::pair< CacheMap::const_iterator, CacheMap::const_iterator > CacheRange;

class Cache 
{
public:
  static Cache& singleton()
  {
    static Cache open_cache;
    return open_cache;
  }

  CacheMap& getCache()
  {
    return cache;
  }

  void rehash(size_t arg)
  {
    cache.rehash(arg);
  }

  bool areTablesUsed(Table*, bool wait_for_name_lock);
  void removeSchema(const identifier::Schema&);
  bool removeTable(Session&, const identifier::Table&, uint32_t flags);
  void release(table::instance::Shared*);
  void insert(table::Concurrent*);

  boost::mutex &mutex()
  {
    return _mutex;
  }

private:
  CacheMap cache;
  boost::mutex _mutex;
};

CacheMap& getCache();
void remove_table(table::Concurrent*);

} /* namepsace table */
} /* namepsace drizzled */

