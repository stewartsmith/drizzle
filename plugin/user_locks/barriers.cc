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
#include "plugin/user_locks/module.h"

#include <boost/thread/locks.hpp>

#include <string>

namespace user_locks {

bool Barriers::create(const user_locks::Key &arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  return barrier_map.insert(std::make_pair(arg, new client::Wakeup())).second;
}

boost::tribool Barriers::release(const user_locks::Key &arg)
{
  size_t elements= 0;
  boost::unique_lock<boost::mutex> scope(mutex);
  Map::iterator iter= barrier_map.find(arg);

  // Nothing is found
  if ( iter == barrier_map.end())
    return boost::indeterminate;

  (*iter).second->start(); // We tell anyone left to start running
  elements= barrier_map.erase(arg);

  if (elements)
    return true;

  return false;
}

client::Wakeup::shared_ptr Barriers::find(const user_locks::Key &arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  Map::iterator iter= barrier_map.find(arg);

  if (iter != barrier_map.end())
    return (*iter).second;

  return client::Wakeup::shared_ptr();
}

void Barriers::Copy(Map &arg)
{
  arg= barrier_map;
}

} /* namespace user_locks */
