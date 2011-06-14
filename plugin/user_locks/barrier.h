/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2010 Brian Aker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "observer.h"

#pragma once

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

  ~Barrier()
  {
    wakeAll();
  }

  // Signal all of the observers to start
  void signal()
  {
    boost::mutex::scoped_lock scopedBarrier(sleeper_mutex);
    wakeAll();
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

    // If we are interrupted we remove ourself from the list, and check on
    // the observers.
    try 
    {
      while (my_generation == generation)
      {
        sleep_threshhold.wait(sleeper_mutex);
      }
    }
    catch(boost::thread_interrupted const& error)
    {
      current_wait++;
      checkObservers();
    }
  }

  // A call to either signal or a release will cause wait_for() to continue
  void wait_until(int64_t wait_until_arg)
  {
    Observer::shared_ptr observer;
    {
      boost::mutex::scoped_lock scopedLock(sleeper_mutex);

      if (wait_until_arg <= count())
        return;

      observer.reset(new Observer(wait_until_arg));
      observers.push_back(observer);
    }

    try {
      observer->sleep();
    }
    catch(boost::thread_interrupted const& error)
    {
      boost::mutex::scoped_lock scopedLock(sleeper_mutex);
      // Someone has interrupted us, we now try to remove ourself from the
      // observer chain ourself

      observers.remove(observer);
      
      throw error;
    }
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

    checkObservers();
  }

  struct isReady : public std::unary_function<Observer::list::const_reference, bool>
  {
    const int64_t count;

    isReady(int64_t arg) :
      count(arg)
    { }

    result_type operator() (argument_type observer)
    {
      if (observer->getLimit() <= count or count == 0)
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
    return std::abs(static_cast<long int>(current_wait));
  }


  drizzled::session_id_t owner;

  const int64_t limit;
  int64_t current_wait;
  int64_t generation;

  Observer::list observers;

  boost::mutex sleeper_mutex;
  boost::condition_variable_any sleep_threshhold;

};

} // namespace barriers
} // namespace user_locks

