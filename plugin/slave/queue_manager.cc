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

#include "config.h"
#include "plugin/slave/queue_manager.h"
#include <boost/thread.hpp>
#include "drizzled/message/transaction.pb.h"

using namespace drizzled;

namespace slave
{

void QueueManager::processQueue(void)
{
  boost::posix_time::seconds duration(checkInterval);

  while (1)
  {
    {
      boost::this_thread::disable_interruption di;
      /* process the queue here */
    }
    
    /*
     * Interruptable only when not doing work (aka, sleeping)
     */

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

bool QueueManager::executeMessage(const message::Transaction &transaction)
{
  (void)transaction;
  return true;
}

} /* namespace slave */