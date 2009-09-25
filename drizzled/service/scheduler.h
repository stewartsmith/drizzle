/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_SERVICE_SCHEDULER_H
#define DRIZZLED_SERVICE_SCHEDULER_H

#include <drizzled/name_map.h>

namespace drizzled
{

namespace plugin
{
  class SchedulerFactory;
  class Scheduler;
}

namespace service
{

/**
 * Class to handle all Scheduler objects
 */
class Scheduler
{
private:

  plugin::SchedulerFactory *scheduler_factory;
  NameMap<plugin::SchedulerFactory *> all_schedulers;

public:

  Scheduler();
  ~Scheduler();

  void add(plugin::SchedulerFactory *factory);
  void remove(plugin::SchedulerFactory *factory);
  bool setFactory(const std::string& name);
  plugin::Scheduler *getScheduler();

};

} /* namespace service */
} /* namespace drizzled */

#endif /* DRIZZLED_SERVICE_SCHEDULER_H */
