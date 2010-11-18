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
    owner(owner_arg),
    wait_count(0),
    current_wait(0),
    generation(0)
  { }

  Barrier(drizzled::session_id_t owner_arg, int64_t wait_count_arg) :
    owner(owner_arg),
    wait_count(wait_count_arg),
    current_wait(wait_count),
    generation(0)
  {
  }


  // Signal all of the waiters to start
  void signal()
  {
    {
      boost::mutex::scoped_lock scopedBarrier(sleeper_mutex);
      generation++;
      current_wait= wait_count;
    }
    sleep_threshhold.notify_all();
  }

  drizzled::session_id_t getOwner() const
  {
    return owner;
  }

  void wait()
  {
    boost::mutex::scoped_lock scopedLock(sleeper_mutex);
    int64_t my_generation= generation;

    if (wait_count)
    {
      if (not --current_wait)
      {
        generation++;
        current_wait= wait_count;
        sleep_threshhold.notify_all();

        return;
      }

    }

    while (my_generation == generation)
    {
      sleep_threshhold.wait(sleeper_mutex);
    }
  }

  void wait(int64_t generation_arg)
  {
    boost::mutex::scoped_lock scopedLock(sleeper_mutex);
    int64_t my_generation= generation;

    // If the generation is newer  then we just return immediatly
    if (my_generation > generation_arg)
      return;

    if (wait_count)
    {
      if (not --current_wait)
      {
        generation++;
        current_wait= wait_count;
        sleep_threshhold.notify_all();

        return;
      }

    }

    while (my_generation == generation)
    {
      sleep_threshhold.wait(sleeper_mutex);
    }
  }

  int64_t getGeneration() const
  {
    return generation;
  }

  int64_t getWaitCount() const
  {
    return wait_count;
  }

private:
  drizzled::session_id_t owner;

  const int64_t wait_count;
  int64_t current_wait;
  int64_t generation;

  boost::mutex sleeper_mutex;
  boost::condition_variable_any sleep_threshhold;
};

} // namespace barriers
} // namespace user_locks

#endif /* PLUGIN_USER_LOCKS_BARRIER_H */
