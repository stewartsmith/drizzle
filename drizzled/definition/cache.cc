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

#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <drizzled/session.h>
#include <drizzled/identifier/table.h>
#include <drizzled/definition/cache.h>
#include <drizzled/table/instance.h>
#include <drizzled/util/find_ptr.h>

namespace drizzled {
namespace definition {

Cache::Map Cache::cache;
boost::mutex Cache::_mutex;

table::instance::Shared::shared_ptr Cache::find(const identifier::Table::Key &key)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  if (Map::mapped_type* ptr= find_ptr(cache, key))
    return *ptr;
  return table::instance::Shared::shared_ptr();
}

void Cache::erase(const identifier::Table::Key &key)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  
  cache.erase(key);
}

bool Cache::insert(const identifier::Table::Key &key, table::instance::Shared::shared_ptr share)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  std::pair<Map::iterator, bool> ret= cache.insert(std::make_pair(key, share));

  return ret.second;
}

void Cache::CopyFrom(drizzled::table::instance::Shared::vector &vector)
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  vector.reserve(definition::Cache::size());

  std::transform(cache.begin(),
                 cache.end(),
                 std::back_inserter(vector),
                 boost::bind(&Map::value_type::second, _1) );
  assert(vector.size() == cache.size());
}

} /* namespace definition */
} /* namespace drizzled */
