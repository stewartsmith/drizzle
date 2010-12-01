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

#include "drizzled/plugin/catalog.h"
#include "drizzled/identifier/catalog.h"

namespace drizzled {

namespace generator {
class Catalog;
}

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

  catalog::Instance::shared_ptr find(const identifier::Catalog &identifier);
  void erase(const identifier::Catalog &identifier);
  bool insert(const identifier::Catalog &identifier, catalog::Instance::shared_ptr share);

protected:
  friend class drizzled::generator::Catalog;

  void CopyFrom(catalog::Instance::vector &vector)
  {
    boost::mutex::scoped_lock scopedLock(_mutex);

    vector.reserve(catalog::Cache::singleton().size());

    std::transform(cache.begin(),
                   cache.end(),
                   std::back_inserter(vector),
                   boost::bind(&unordered_map::value_type::second, _1) );
    assert(vector.size() == cache.size());
  }

private:
  typedef boost::unordered_map< identifier::Catalog, catalog::Instance::shared_ptr> unordered_map;

  unordered_map cache;
  boost::mutex _mutex;
};

} /* namespace catalog */
} /* namespace drizzled */

#endif /* DRIZZLED_CATALOG_CACHE_H */
