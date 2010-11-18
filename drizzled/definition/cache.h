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

#ifndef DRIZZLED_DEFINITION_CACHE_H
#define DRIZZLED_DEFINITION_CACHE_H

#include "drizzled/definition/table.h"

namespace drizzled {

namespace definition {

typedef boost::unordered_map< TableIdentifier::Key, TableShare::shared_ptr> CacheMap;

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

  size_t size() const
  {
    return cache.size();
  }

  void rehash(size_t arg)
  {
    cache.rehash(arg);
  }

  TableShare::shared_ptr find(const TableIdentifier &identifier);
  void erase(const TableIdentifier &identifier);
  bool insert(const TableIdentifier &identifier, TableShare::shared_ptr share);
};

} /* namespace definition */
} /* namespace drizzled */

#endif /* DRIZZLED_DEFINITION_CACHE_H */
