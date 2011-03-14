/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
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

#include <drizzled/internal/my_pthread.h>

namespace slave
{

/**
 * Interface for threads interacting with the replication queue.
 *
 * This class uses the Template Method design pattern to define the set of
 * steps for a thread operating on the replication queue. An implementing
 * class need only implement the init(), process(), and/or shutdown() methods.
 * The implementing class need only pass run() method to thread initialization.
 */

class QueueThread
{
public:
  virtual ~QueueThread()
  {}
  
  void run(void);

  /**
   * Do any initialization work.
   *
   * @retval true Success
   * @retval false Failure
   */
  virtual bool init()
  {
    return true;
  }

  /**
   * Work to do at thread shutdown time.
   */
  virtual void shutdown()
  {}

  /**
   * Method that actually does the work around the queue.
   *
   * Returning 'false' from this method currently causes the thread to
   * shutdown.
   *
   * @retval true Success
   * @retval false Failure
   */
  virtual bool process()= 0;

  virtual uint32_t getSleepInterval()= 0;
};

} /* namespace slave */

