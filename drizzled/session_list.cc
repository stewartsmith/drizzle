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

#include "config.h"

#include <vector>

#include "drizzled/session_list.h"
#include "drizzled/session.h"
#include "drizzled/current_session.h"
#include "drizzled/plugin/authorization.h"

using namespace std;

namespace drizzled
{

namespace session
{

Session::shared_ptr Cache::find(const session_id_t &id)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  for (List::iterator it= cache.begin(); it != cache.end(); ++it )
  {
    if ((*it)->thread_id == id)
    {
      return *it;
    }
  }

  return Session::shared_ptr();
}

size_t Cache::count()
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  return cache.size();
}

void Cache::erase(Session::Ptr arg)
{
  for (List::iterator it= cache.begin(); it != cache.end(); it++)
  {
    if ((*it).get() == arg)
    {
      cache.erase(it);
      return;
    }
  }
}

void Cache::erase(Session::shared_ptr arg)
{
  cache.erase(remove(cache.begin(), cache.end(), arg));
}

} /* namespace session */
} /* namespace drizzled */
