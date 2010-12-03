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

#ifndef DRIZZLED_CATALOG_CACHE_H
#define DRIZZLED_CATALOG_CACHE_H

#include <boost/bind.hpp>

#include "drizzled/catalog.h"
#include "drizzled/plugin/catalog.h"
#include "drizzled/identifier/catalog.h"

#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>

namespace drizzled {

namespace generator {
namespace catalog {
class Cache;
class Instance;
} //namespace catalog
} //namespace generator

namespace catalog {

class Cache
{
public:
  static inline Cache &singleton()
  {
    static Cache open_cache;

    return open_cache;
  }

  size_t size() const
  {
    return cache.size();
  }

  void rehash(size_t arg)
  {
    cache.rehash(arg);
  }

  Instance::shared_ptr find(const identifier::Catalog &identifier, catalog::error_t &error);
  bool exist(const identifier::Catalog &identifier);
  bool erase(const identifier::Catalog &identifier, catalog::error_t &error);
  bool insert(const identifier::Catalog &identifier, Instance::shared_ptr instance, catalog::error_t &error);
  bool lock(const identifier::Catalog &identifier, catalog::error_t &error);
  bool unlock(const identifier::Catalog &identifier, catalog::error_t &error);

protected:
  friend class drizzled::generator::catalog::Cache;
  friend class drizzled::plugin::Catalog;

  void copy(catalog::Instance::vector &vector);

private:
  typedef boost::unordered_map< identifier::Catalog, catalog::Instance::shared_ptr> unordered_map;

  unordered_map cache;
  boost::mutex _mutex;
};

} /* namespace catalog */
} /* namespace drizzled */

#endif /* DRIZZLED_CATALOG_CACHE_H */
