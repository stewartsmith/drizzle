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

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <drizzled/visibility.h>
#include <list>

namespace drizzled {
namespace session {

class DRIZZLED_API Cache 
{
  typedef boost::shared_ptr<drizzled::Session> session_ptr;
public:
  typedef std::list<session_ptr> list;

  static list &getCache()
  {
    return cache;
  }

  static boost::mutex &mutex()
  {
    return _mutex;
  }

  static boost::condition_variable &cond()
  {
    return _end;
  }

  static void shutdownFirst();
  static void shutdownSecond();

  static void erase(const session_ptr&);
  static size_t count();
  static void insert(const session_ptr&);

  static session_ptr find(const session_id_t&);

private:
  static bool volatile _ready_to_exit;
  static list cache;
  static boost::mutex _mutex;
  static boost::condition_variable _end;
};

} /* namespace session */
} /* namespace drizzled */

