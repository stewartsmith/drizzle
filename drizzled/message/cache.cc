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

#include <drizzled/pthread_globals.h>
#include "drizzled/message/cache.h"

namespace drizzled {

namespace message {

TablePtr Cache::find(const TableIdentifier &identifier)
{
  boost_unique_lock_t scoped_lock(_access);

  Map::iterator iter= cache.find(identifier.getKey());
  if (iter != cache.end())
  {
    return (*iter).second;
  }

  return TablePtr();
}

void Cache::erase(const TableIdentifier &identifier)
{
  boost_unique_lock_t scoped_lock(_access);
  
  cache.erase(identifier.getKey());
}

bool Cache::insert(const TableIdentifier &identifier, TablePtr share)
{
  boost_unique_lock_t scoped_lock(_access);

  std::pair<Map::iterator, bool> ret=
    cache.insert(std::make_pair(identifier.getKey(), share));

  return ret.second;
}

bool Cache::insert(const TableIdentifier &identifier, drizzled::message::Table &message)
{
  boost_unique_lock_t scoped_lock(_access);

  TablePtr share;
  share.reset(new message::Table(message));

  std::pair<Map::iterator, bool> ret=
    cache.insert(std::make_pair(identifier.getKey(), share));

  return ret.second;
}

} /* namespace definition */
} /* namespace drizzled */
