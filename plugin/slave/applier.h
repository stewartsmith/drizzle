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

#ifndef PLUGIN_SLAVE_APPLIER_H
#define PLUGIN_SLAVE_APPLIER_H

#include "plugin/slave/queue_manager.h"
#include "drizzled/plugin/daemon.h"
#include <boost/thread.hpp>
#include <cstdio>

namespace slave
{

class Applier : public drizzled::plugin::Daemon
{
public:

  Applier() : drizzled::plugin::Daemon("Replication Slave")
  {
    q_mgr.setCheckInterval(5);
    q_mgr.setSchema("test");
    q_mgr.setTable("t1");
    thread= boost::thread(&QueueManager::processQueue, &q_mgr);
  }
  
  ~Applier()
  {
    printf("Slave services shutting down\n");
    thread.interrupt();
  }

private:
  QueueManager q_mgr;

  /** Thread that will process the work queue */
  boost::thread thread;
};
  
} /* namespace slave */

#endif /* PLUGIN_SLAVE_APPLIER_H */