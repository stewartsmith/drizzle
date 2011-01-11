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
#include "drizzled/catalog.h"
#include "drizzled/catalog/cache.h"

namespace drizzled {
namespace catalog {

Instance::shared_ptr Cache::find(const identifier::Catalog &identifier, catalog::error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  unordered_map::iterator iter= cache.find(identifier);
  if (iter != cache.end())
  {
    if (not (*iter).second)
      error= LOCKED;

    return (*iter).second;
  }

  error= NOT_FOUND;
  return catalog::Instance::shared_ptr();
}

bool Cache::exist(const identifier::Catalog &identifier)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  unordered_map::iterator iter= cache.find(identifier);
  if (iter != cache.end())
  {
    return true;
  }

  return false;
}

bool Cache::erase(const identifier::Catalog &identifier, catalog::error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  unordered_map::iterator iter= cache.find(identifier);
  if (iter != cache.end())
  {
    unordered_map::size_type erased= cache.erase(identifier);

    if (erased)
      return true;

    assert(0); // This should be imposssible
  }
  error= NOT_FOUND;

  return false;
}

bool Cache::unlock(const identifier::Catalog &identifier, catalog::error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  unordered_map::iterator iter= cache.find(identifier);
  if (iter != cache.end())
  {
    if (not (*iter).second)
    {
      unordered_map::size_type erased= cache.erase(identifier);

      if (erased)
        return true;

      assert(0); // This should be imposssible
    }
    error= FOUND;
  }
  else
  {
    error= NOT_FOUND;
  }

  return false;
}

bool Cache::lock(const identifier::Catalog &identifier, catalog::error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  std::pair<unordered_map::iterator, bool> ret= cache.insert(std::make_pair(identifier, catalog::Instance::shared_ptr()));

  if (ret.second == false)
  {
    if (ret.first->second)
    {
      error= FOUND;
    }
    else
    {
      error= LOCKED;
    }
  }

  return ret.second;
}

bool Cache::insert(const identifier::Catalog &identifier, catalog::Instance::shared_ptr instance, catalog::error_t &error)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  std::pair<unordered_map::iterator, bool> ret= cache.insert(std::make_pair(identifier, instance));

  if (ret.second == false)
  {
    if (ret.first->second)
    {
      error= FOUND;
    }
    else
    {
      error= LOCKED;
    }
  }

  return ret.second;
}

void Cache::copy(catalog::Instance::vector &vector)
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  vector.reserve(catalog::Cache::singleton().size());

  std::transform(cache.begin(),
                 cache.end(),
                 std::back_inserter(vector),
                 boost::bind(&unordered_map::value_type::second, _1) );
  assert(vector.size() == cache.size());
}


} /* namespace catalog */
} /* namespace drizzled */
