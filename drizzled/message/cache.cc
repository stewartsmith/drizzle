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
#include <drizzled/pthread_globals.h>
#include <drizzled/message/cache.h>
#include <drizzled/util/find_ptr.h>

namespace drizzled {
namespace message {

table::shared_ptr Cache::find(const identifier::Table &identifier)
{
  boost::mutex::scoped_lock scoped_lock(_access);
  if (Map::mapped_type* ptr= find_ptr(cache, identifier.getKey()))
    return *ptr;
  return table::shared_ptr();
}

void Cache::erase(const identifier::Table &identifier)
{
  boost::mutex::scoped_lock scoped_lock(_access);
  cache.erase(identifier.getKey());
}

bool Cache::insert(const identifier::Table &identifier, table::shared_ptr share)
{
  boost::mutex::scoped_lock scoped_lock(_access);
  return cache.insert(std::make_pair(identifier.getKey(), share)).second;
}

bool Cache::insert(const identifier::Table &identifier, drizzled::message::Table &message)
{
  return insert(identifier, table::shared_ptr(new message::Table(message)));
}

} // namespace message
} // namespace drizzled
