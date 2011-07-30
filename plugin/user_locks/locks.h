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

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

#include <plugin/user_locks/lock.h>

#include <string>

#include <drizzled/session.h>

#pragma once

namespace user_locks {

namespace locks {
  enum return_t {
    SUCCESS,
    NOT_FOUND,
    NOT_OWNED_BY
  };
} /* locks user_locks */

const size_t LARGEST_LOCK_NAME= 64;

class Locks
{
public:
  typedef boost::unordered_map<user_locks::Key, user_locks::Lock::shared_ptr> LockMap;

  static Locks &getInstance(void)
  {
    static Locks instance;
    return instance;
  }

  void waitCreate(int64_t wait_for= 2); // Default is to wait 2 seconds before returning

  bool lock(drizzled::session_id_t id_arg, const user_locks::Key &arg, int64_t wait_for= 0);
  bool lock(drizzled::session_id_t id_arg, const user_locks::Keys &arg);
  locks::return_t release(const user_locks::Key &arg, drizzled::session_id_t &id_arg, bool and_wait= false);
  bool isFree(const user_locks::Key &arg);
  bool isUsed(const user_locks::Key &arg, drizzled::session_id_t &id_arg);
  void Copy(LockMap &lock_map);

private:
  boost::mutex mutex;
  boost::condition_variable create_cond; // Signal next 
  boost::condition_variable release_cond;
  LockMap lock_map; 
};


} /* namespace user_locks */

