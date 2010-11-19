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

#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

#include "drizzled/session.h"
#include "drizzled/identifier/table.h"
#include "drizzled/definition/cache.h"
#include "drizzled/definition/table.h"

namespace drizzled {

namespace definition {

TableShare::shared_ptr Cache::find(const TableIdentifier::Key &key)
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  Map::iterator iter= cache.find(key);
  if (iter != cache.end())
  {
    return (*iter).second;
  }

  return TableShare::shared_ptr();
}

void Cache::erase(const TableIdentifier::Key &key)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  
  cache.erase(key);
}

bool Cache::insert(const TableIdentifier::Key &key, TableShare::shared_ptr share)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  std::pair<Map::iterator, bool> ret=
    cache.insert(std::make_pair(key, share));

  return ret.second;
}

} /* namespace definition */
} /* namespace drizzled */
