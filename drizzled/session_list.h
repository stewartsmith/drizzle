/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_SESSION_LIST_H
#define DRIZZLED_SESSION_LIST_H

#include "drizzled/session.h"
#include <list>

namespace drizzled
{

class Session;

namespace session
{

class Cache 
{
  boost::mutex _mutex;

public:
  typedef std::list<Session::Ptr> List;

  static inline Cache &singleton()
  {
    static Cache open_cache;

    return open_cache;
  }

  List &getCache()
  {
    return cache;
  }

  boost::mutex &mutex()
  {
    return _mutex;
  }

  void erase(Session::Ptr);

private:
  List cache;
};

} /* namespace session */
} /* namespace drizzled */

#endif /* DRIZZLED_SESSION_LIST_H */
