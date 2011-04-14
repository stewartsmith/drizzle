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

#include <drizzled/catalog/cache.h>
#include <drizzled/util/find_ptr.h>

namespace drizzled {
namespace catalog {

Cache::unordered_map Cache::cache;
boost::mutex Cache::_mutex;

Instance::shared_ptr Cache::find(const identifier::Catalog &identifier, error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  if (const unordered_map::mapped_type* ptr= find_ptr(cache, identifier))
  {
    error= *ptr ? EE_OK : ER_CATALOG_NO_LOCK;
    return *ptr;
  }
  error= ER_CATALOG_DOES_NOT_EXIST;
  return catalog::Instance::shared_ptr();
}

bool Cache::exist(const identifier::Catalog &identifier)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  return find_ptr(cache, identifier);
}

bool Cache::erase(const identifier::Catalog &identifier, error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  if (find_ptr(cache, identifier))
  {
    if (cache.erase(identifier))
      return true;
    assert(false); // This should be imposssible
  }
  error= ER_CATALOG_DOES_NOT_EXIST;
  return false;
}

bool Cache::unlock(const identifier::Catalog &identifier, error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  if (const unordered_map::mapped_type* ptr= find_ptr(cache, identifier))
  {
    if (not *ptr)
    {
      if (cache.erase(identifier))
        return true;
      assert(false); // This should be imposssible
    }
    error= EE_OK;
  }
  else
  {
    error= ER_CATALOG_DOES_NOT_EXIST;
  }
  return false;
}

bool Cache::lock(const identifier::Catalog &identifier, error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  std::pair<unordered_map::iterator, bool> ret= cache.insert(std::make_pair(identifier, catalog::Instance::shared_ptr()));
  if (not ret.second)
    error= ret.first->second ? EE_OK : ER_CATALOG_NO_LOCK;
  return ret.second;
}

bool Cache::insert(const identifier::Catalog &identifier, catalog::Instance::shared_ptr instance, error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  std::pair<unordered_map::iterator, bool> ret= cache.insert(std::make_pair(identifier, instance));
  if (not ret.second)
    error= ret.first->second ? EE_OK : ER_CATALOG_NO_LOCK;
  return ret.second;
}

void Cache::copy(catalog::Instance::vector &vector)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  vector.reserve(catalog::Cache::size());
  std::transform(cache.begin(), cache.end(), std::back_inserter(vector), boost::bind(&unordered_map::value_type::second, _1));
  assert(vector.size() == cache.size());
}


} /* namespace catalog */
} /* namespace drizzled */
