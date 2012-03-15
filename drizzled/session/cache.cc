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

#include <config.h>
#include <drizzled/session/cache.h>

#include <boost/foreach.hpp>
#include <drizzled/current_session.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/session.h>
#include <vector>

namespace drizzled {
namespace session {

bool volatile Cache::_ready_to_exit= false;
Cache::list Cache::cache;
boost::mutex Cache::_mutex;
boost::condition_variable Cache::_end;

Cache::session_ptr Cache::find(const session_id_t &id)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  BOOST_FOREACH(list::const_reference it, cache)
  {
    if (it->thread_id == id)
      return it;
  }
  return session_ptr();
}

void Cache::shutdownFirst()
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  _ready_to_exit= true;

  /* do the broadcast inside the lock to ensure that my_end() is not called */
  _end.notify_all();
}

  /* Wait until cleanup is done */
void Cache::shutdownSecond()
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  while (not _ready_to_exit)
  {
    _end.wait(scopedLock);
  }
}

size_t Cache::count()
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  return cache.size();
}

void Cache::insert(const session_ptr& arg)
{
  boost::mutex::scoped_lock scopedLock(_mutex);
  cache.push_back(arg);
}

void Cache::erase(const session_ptr& arg)
{
  list::iterator iter= std::find(cache.begin(), cache.end(), arg);
  assert(iter != cache.end());
  cache.erase(iter);
}

} /* namespace session */
} /* namespace drizzled */
