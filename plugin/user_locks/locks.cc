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

bool Locks::lock(drizzled::session_id_t id_arg, const user_locks::Key &arg, int64_t wait_for)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  boost::system_time timeout= boost::get_system_time() + boost::posix_time::seconds(wait_for);
  
  LockMap::iterator iter;
  while ((iter= lock_map.find(arg)) != lock_map.end())
  {
    if (id_arg == iter->second->id)
    {
      // We own the lock, so we just exit.
      return true;
    }
    try {
      if (wait_for)
      {
        bool success= release_cond.timed_wait(scope, timeout);

        if (not success)
          return false;
      }
      else
      {
        release_cond.wait(scope);
      }
    }
    catch(boost::thread_interrupted const& error)
    {
      // Currently nothing is done here.
      throw error;
    }
  }

  if (iter == lock_map.end())
  {
    create_cond.notify_all();
    return lock_map.insert(std::make_pair(arg, new Lock(id_arg))).second;
  }

  return false;
}

// Currently we just let timeouts occur, and the caller will need to know
// what it is looking for/whether to go back into this.
void Locks::waitCreate(int64_t wait_for)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  boost::system_time timeout= boost::get_system_time() + boost::posix_time::seconds(wait_for);

  try {
    create_cond.timed_wait(scope, timeout);
  }
  catch(boost::thread_interrupted const& error)
  {
    // Currently nothing is done here.
    throw error;
  }
}

bool Locks::lock(drizzled::session_id_t id_arg, const user_locks::Keys &arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  user_locks::Keys created;
  bool error= false;

  for (user_locks::Keys::const_iterator iter= arg.begin(); iter != arg.end(); iter++)
  {
    LockMap::iterator record= lock_map.find(*iter);

    if (record != lock_map.end()) // Found, so check ownership of the lock
    {
      if (id_arg != (*record).second->id)
      {
        // So it turns out the locks exist, and we can't grab them all
        error= true;
        break;
      }
    }
    else
    {
      lock_map.insert(std::make_pair(*iter, new Lock(id_arg)));
      created.insert(*iter);
    }
  }

  if (error)
  {
    for (user_locks::Keys::const_iterator iter= created.begin(); iter != created.end(); iter++)
    {
      lock_map.erase(*iter);
    }

    return false;
  }

  create_cond.notify_all();

  return true;
}

bool Locks::isUsed(const user_locks::Key &arg, drizzled::session_id_t &id_arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  
  LockMap::iterator iter= lock_map.find(arg);
  
  if ( iter == lock_map.end())
    return false;

  id_arg= iter->second->id;

  return true;
}

bool Locks::isFree(const user_locks::Key &arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);

  LockMap::iterator iter= lock_map.find(arg);
  
  return iter != lock_map.end();
}

void Locks::Copy(LockMap &lock_map_arg)
{
  boost::unique_lock<boost::mutex> scope(mutex);
  lock_map_arg= lock_map;
}

locks::return_t Locks::release(const user_locks::Key &arg, drizzled::session_id_t &id_arg, bool and_wait)
{
  size_t elements= 0;
  boost::unique_lock<boost::mutex> scope(mutex);
  LockMap::iterator iter= lock_map.find(arg);

  // Nothing is found
  if ( iter == lock_map.end())
    return locks::NOT_FOUND;

  if (iter->second->id == id_arg)
  {
    elements= lock_map.erase(arg);
    assert(elements); // If we can't find what we just found, then we are broken

    if (elements)
    {
      release_cond.notify_one();

      if (and_wait)
      {
        bool found= false;
        while (not found)
        {
          assert(boost::this_thread::interruption_enabled());
          try {
            create_cond.wait(scope);
          }
          catch(boost::thread_interrupted const& error)
          {
            // Currently nothing is done here.
            throw error;
          }
          iter= lock_map.find(arg);

          if (iter != lock_map.end())
            found= true;
        }
      }

      return locks::SUCCESS;
    }
  }

  return locks::NOT_OWNED_BY;
}

} /* namespace user_locks */
