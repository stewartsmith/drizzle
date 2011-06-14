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

#include <config.h>
#include <plugin/slave/queue_thread.h>
#include <drizzled/internal/my_pthread.h>
#include <boost/thread.hpp>

using namespace drizzled;

namespace slave {

void QueueThread::run()
{
  boost::posix_time::seconds duration(getSleepInterval());

  /* thread setup needed to do things like create a Session */
  internal::my_thread_init();

  if (not init())
    return;

  while (1)
  {
    {
      /* This uninterruptable block processes the message queue */
      boost::this_thread::disable_interruption di;

      if (not process())
      {
        shutdown();
        return;
      }
    }

    /* Interruptable only when not doing work (aka, sleeping) */
    try
    {
      boost::this_thread::sleep(duration);
    }
    catch (boost::thread_interrupted &)
    {
      return;
    }
  }
}

} /* namespace slave */
