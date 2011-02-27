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
#include <plugin/user_locks/module.h>

#include <boost/thread/locks.hpp>

#include <string>

namespace user_locks {
namespace barriers {

bool Barriers::create(const user_locks::Key &arg, drizzled::session_id_t owner)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  return barrier_map.insert(std::make_pair(arg, new Barrier(owner))).second;
}

bool Barriers::create(const user_locks::Key &arg, drizzled::session_id_t owner, int64_t wait_count)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  return barrier_map.insert(std::make_pair(arg, new Barrier(owner, wait_count))).second;
}

/*
  @note return

  true -> release happened.
  false -> no release, we were not the owner
  indeterminate -> barrier was not found.

*/
return_t Barriers::release(const user_locks::Key &arg, drizzled::session_id_t owner)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  Map::iterator iter= barrier_map.find(arg);

  // Nothing is found
  if ( iter == barrier_map.end())
    return NOT_FOUND;

  if (not iter->second->getOwner() == owner)
    return NOT_OWNED_BY;

  iter->second->signal(); // We tell anyone left to start running
  (void)barrier_map.erase(arg);

  return SUCCESS;
}

Barrier::shared_ptr Barriers::find(const user_locks::Key &arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  Map::iterator iter= barrier_map.find(arg);

  if (iter != barrier_map.end())
    return iter->second;

  return Barrier::shared_ptr();
}

void Barriers::Copy(Map &arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  arg= barrier_map;
}

} /* namespace barriers */
} /* namespace user_locks */
