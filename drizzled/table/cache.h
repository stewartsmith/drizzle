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

#include <boost/thread/mutex.hpp>
#include <boost/unordered_map.hpp>
#include <drizzled/identifier.h>

namespace drizzled {
namespace table {

typedef boost::unordered_multimap<identifier::Table::Key, Concurrent*> CacheMap;
typedef std::pair<CacheMap::const_iterator, CacheMap::const_iterator> CacheRange;

class Cache 
{
public:
  static CacheMap& getCache()
  {
    return cache;
  }

  static void rehash(size_t arg)
  {
    cache.rehash(arg);
  }

  static boost::mutex& mutex()
  {
    return _mutex;
  }

  static bool areTablesUsed(Table*, bool wait_for_name_lock);
  static void removeSchema(const identifier::Schema&);
  static bool removeTable(Session&, const identifier::Table&, uint32_t flags);
  static void release(table::instance::Shared*);
  static void insert(table::Concurrent*);
private:
  static CacheMap cache;
  static boost::mutex _mutex;
};

CacheMap& getCache();
void remove_table(table::Concurrent*);

} /* namepsace table */
} /* namepsace drizzled */

