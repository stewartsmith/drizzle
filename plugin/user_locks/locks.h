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
#include <boost/logic/tribool.hpp>

#include <string>

#include "drizzled/session.h"


#ifndef PLUGIN_USER_LOCKS_LOCKS_H
#define PLUGIN_USER_LOCKS_LOCKS_H

namespace user_locks {

class Locks
{
  struct lock_st {
    drizzled::session_id_t id;

    lock_st(drizzled::session_id_t id_arg) :
      id(id_arg)
    {
    }
  };

  typedef boost::shared_ptr<lock_st> lock_st_ptr;

  typedef boost::unordered_map<std::string, lock_st_ptr, drizzled::util::insensitive_hash, drizzled::util::insensitive_equal_to> LockMap;

public:
  static Locks &getInstance(void)
  {
    static Locks instance;
    return instance;
  }

  bool lock(drizzled::session_id_t id_arg, const std::string &arg);
  bool lock(drizzled::session_id_t id_arg, const std::string &arg, int64_t wait_for= 0);
  boost::tribool release(const std::string &arg, drizzled::session_id_t &id_arg);
  bool isFree(const std::string &arg);
  bool isUsed(const std::string &arg, drizzled::session_id_t &id_arg);

private:
  boost::mutex mutex;
  boost::condition_variable cond;
  LockMap lock_map; 
};


} /* namespace user_locks */

#endif /* PLUGIN_USER_LOCKS_LOCKS_H */
