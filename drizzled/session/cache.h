/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#pragma once

#include <list>

#include <drizzled/visibility.h>

namespace drizzled
{

class Session;

namespace session
{

class DRIZZLED_API Cache 
{
  typedef boost::shared_ptr<drizzled::Session> session_shared_ptr;
public:
  typedef std::list<session_shared_ptr> list;

  Cache() :
    _ready_to_exit(false)
  {
  }

  static inline Cache &singleton()
  {
    static Cache open_cache;

    return open_cache;
  }

  list &getCache()
  {
    return cache;
  }

  boost::mutex &mutex()
  {
    return _mutex;
  }

  boost::condition_variable &cond()
  {
    return _end;
  }

  void shutdownFirst();
  void shutdownSecond();

  void erase(session_shared_ptr&);
  size_t count();
  void insert(session_shared_ptr &arg);

  session_shared_ptr find(const session_id_t &id);

private:
  bool volatile _ready_to_exit;
  list cache;
  boost::mutex _mutex;
  boost::condition_variable _end;
};

} /* namespace session */
} /* namespace drizzled */

