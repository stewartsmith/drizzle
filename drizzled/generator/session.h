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

#pragma once

#include <boost/thread/mutex.hpp>

#include <drizzled/identifier/user.h>
#include <drizzled/session/cache.h>

namespace drizzled {
namespace generator {

class Session
{
  session::Cache::list local_list;
  session::Cache::list::const_iterator iter;
  const identifier::User& user;

public:

  Session(const identifier::User& arg) :
    user(arg)
  {
    boost::mutex::scoped_lock scopedLock(session::Cache::mutex());
    local_list= session::Cache::getCache();
    iter= local_list.begin();
  }

  operator drizzled::Session*()
  {
    while (iter != local_list.end())
    {
      drizzled::Session* ret= iter->get();
      iter++;

      if (ret->isViewable(user))
	      return ret;
    }
    return NULL;
  }
};

} /* namespace generator */
} /* namespace drizzled */

