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
#include <boost/shared_ptr.hpp>

#ifndef PLUGIN_USER_LOCKS_BARRIER_H
#define PLUGIN_USER_LOCKS_BARRIER_H

namespace user_locks {
namespace barriers {

// Barrier starts in a blocking posistion
class Barrier {
public:
  typedef boost::shared_ptr<Barrier> shared_ptr;

  Barrier(drizzled::session_id_t owner_arg) :
    sleeping(true),
    owner(owner_arg)
  { }

  // Signal all of the waiters to start
  void start()
  {
    boost::mutex::scoped_lock scopedBarrier(sleeper_mutex);
    sleeping= false;
    sleep_threshhold.notify_all();
  }

  // reset after the start of the signal so that we can reuse the wakeup
  void reset()
  {
    boost::mutex::scoped_lock scopedBarrier(sleeper_mutex);
    sleeping= true;
  }

  drizzled::session_id_t getOwner() const
  {
    return owner;
  }

  void wait() 
  {
    sleeper_mutex.lock();
    while (sleeping)
    {
      sleep_threshhold.wait(sleeper_mutex);
    }
    sleeper_mutex.unlock();
  }

private:
  bool sleeping;
  drizzled::session_id_t owner;
  boost::mutex sleeper_mutex;
  boost::condition_variable_any sleep_threshhold;
};

} // namespace barriers
} // namespace user_locks

#endif /* PLUGIN_USER_LOCKS_BARRIER_H */
