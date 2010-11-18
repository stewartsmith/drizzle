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
#include <boost/foreach.hpp>

#ifndef PLUGIN_USER_LOCKS_BARRIER_H
#define PLUGIN_USER_LOCKS_BARRIER_H

/*
  Barrier was designed with the following concepts.

  1) A barrier can be set with an initial limit which can be used such that if the limit is met, it releases all waiters.
  2) A barrier can be released at any time, even if the limit is not met by an outside caller.
  3) An observer can register itself to the barrier, it will wait until some predicate action releases it.
  4) Observers are always released by limit, or in the case where the barrier is released or destroyed.
  5) Observers should be held by copy, not by reference in order to allow for correct deletion.

  @todo while we do pass an owner type to a barrier currently, we make no usage of it, and instead we currently protect
  poor use, namely the owner of a barrier calling wait() via the layer above. It may be a good idea to change this.
*/

namespace user_locks {
namespace barriers {

// Barrier starts in a blocking posistion
class Barrier {
  struct observer_st {
    typedef boost::shared_ptr<observer_st> shared_ptr;
    typedef std::list <shared_ptr> list;

    bool woken;
    int64_t waiting_for;
    int64_t generation;
    boost::mutex _mutex;
    boost::condition_variable_any cond;

    observer_st(int64_t wait_until_arg, int64_t generation_arg) :
      woken(false),
      waiting_for(wait_until_arg),
      generation(generation_arg)
    { 
      _mutex.lock();
    }

    void sleep()
    {
      while (not woken)
      {
        cond.wait(_mutex);
      }
      _mutex.unlock();
    }

    void wake()
    {
      {
        boost::mutex::scoped_lock mutex;
        woken= true;
      }
      cond.notify_all();
    }


    ~observer_st()
    {
    }
  };

public:
  typedef boost::shared_ptr<Barrier> shared_ptr;

  Barrier(drizzled::session_id_t owner_arg) :
    owner(owner_arg),
    limit(0),
    current_wait(0),
    generation(0)
  { }

  Barrier(drizzled::session_id_t owner_arg, int64_t limit_arg) :
    owner(owner_arg),
    limit(limit_arg),
    current_wait(limit),
    generation(0)
  {
  }

  // Signal all of the observers to start
  void signal()
  {
    {
      boost::mutex::scoped_lock scopedBarrier(sleeper_mutex);
      generation++;
      current_wait= limit;
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

    --current_wait;
    if (limit)
    {
      if (not current_wait)
      {
        wakeAll();

        return;
      }

    }
    checkObservers();

    while (my_generation == generation)
    {
      sleep_threshhold.wait(sleeper_mutex);
    }
  }

  // A call to either signal or a release will cause wait_for() to continue
  void wait_until(int64_t wait_until_arg)
  {
    observer_st::shared_ptr observer;
    {
      boost::mutex::scoped_lock scopedLock(sleeper_mutex);

      if (wait_until_arg <= count())
        return;

      observer.reset(new observer_st(wait_until_arg, generation));
      observers.push_back(observer);
    }
    observer->sleep();
  }

  void wait(int64_t generation_arg)
  {
    boost::mutex::scoped_lock scopedLock(sleeper_mutex);
    int64_t my_generation= generation;

    // If the generation is newer  then we just return immediatly
    if (my_generation > generation_arg)
      return;

    --current_wait;

    if (limit)
    {
      if (not current_wait)
      {
        wakeAll();
        return;
      }

    }

    while (my_generation == generation)
    {
      sleep_threshhold.wait(sleeper_mutex);
    }
  }

  int64_t getGeneration()
  {
    boost::mutex::scoped_lock scopedLock(sleeper_mutex);
    return generation;
  }

  int64_t sizeObservers()
  {
    boost::mutex::scoped_lock scopedLock(sleeper_mutex);
    return static_cast<int64_t>(observers.size());
  }

  int64_t sizeWaiters()
  {
    boost::mutex::scoped_lock scopedLock(sleeper_mutex);
    return count();
  }

  int64_t getLimit() const
  {
    return limit;
  }

private:
  void wakeAll()
  {
    generation++;
    current_wait= limit;
    sleep_threshhold.notify_all();

    BOOST_FOREACH(observer_st::list::const_reference iter, observers)
    {
      iter->wake();
    }
    observers.clear();
  }

  struct isReady : public std::unary_function<observer_st::list::const_reference, bool>
  {
    const int64_t count;

    isReady(int64_t arg) :
      count(arg)
    { }

    result_type operator() (argument_type observer)
    {
      if (observer->waiting_for <= count)
      {
        observer->wake();
        return true;
      }

      return false;
    }
  };

  void checkObservers()
  {
    observers.remove_if(isReady(count()));
  }

  int64_t count() const
  {
    if (limit)
    {
      return limit - current_wait;
    }
    return std::abs(current_wait);
  }


  drizzled::session_id_t owner;

  const int64_t limit;
  int64_t current_wait;
  int64_t generation;

  observer_st::list observers;

  boost::mutex sleeper_mutex;
  boost::condition_variable_any sleep_threshhold;

};

} // namespace barriers
} // namespace user_locks

#endif /* PLUGIN_USER_LOCKS_BARRIER_H */
