/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef DRIZZLED_GENERATOR_SESSION_H
#define DRIZZLED_GENERATOR_SESSION_H

#include <boost/thread/mutex.hpp>
#include "drizzled/session/cache.h"
#include "drizzled/identifier/user.h"

namespace drizzled {
namespace generator {

class Session
{
  session::Cache::list local_list;
  session::Cache::list::const_iterator iter;
  identifier::User::const_reference user;

public:

  Session(identifier::User::const_reference arg) :
    user(arg)
  {
    boost::mutex::scoped_lock scopedLock(session::Cache::singleton().mutex());
    local_list= session::Cache::singleton().getCache();
    iter= local_list.begin();
  }

  ~Session()
  {
  }

  operator drizzled::Session::pointer()
  {
    while (iter != local_list.end())
    {
      drizzled::Session::pointer ret= (*iter).get();
      iter++;

      if (not ret->isViewable(user))
      {
        continue;
      }

      return ret;
    }

    return drizzled::Session::pointer();
  }
};

} /* namespace generator */
} /* namespace drizzled */

#endif /* DRIZZLED_GENERATOR_SESSION_H */
