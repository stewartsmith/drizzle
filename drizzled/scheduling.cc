/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Mark Atwood
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

#include <drizzled/server_includes.h>
#include <drizzled/scheduling.h>
#include <drizzled/gettext.h>
#include <drizzled/connect.h>
#include "drizzled/plugin_registry.h"
#include "drizzled/registry.h"

using namespace std;

SchedulerFactory *scheduler_factory= NULL;
drizzled::Registry<SchedulerFactory *> all_schedulers;

bool add_scheduler_factory(SchedulerFactory *factory)
{
  if (all_schedulers.count(factory->getName()) != 0)
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Attempted to register a scheduler %s, but a scheduler "
                    "has already been registered with that name.\n"),
                    factory->getName().c_str());
    return true;
  }
  all_schedulers.add(factory);
  return false;
}


bool remove_scheduler_factory(SchedulerFactory *factory)
{
  scheduler_factory= NULL;
  all_schedulers.remove(factory);
  return false;
}


bool set_scheduler_factory(const string& name)
{
   
  SchedulerFactory *factory= all_schedulers.find(name);
  if (factory == NULL)
  {
    errmsg_printf(ERRMSG_LVL_WARN,
                  _("Attempted to configure %s as the scheduler, which did "
                    "not exist.\n"), name.c_str());
    return true;
  }
  scheduler_factory= factory;

  return false;
}

Scheduler &get_thread_scheduler()
{
  assert(scheduler_factory != NULL);
  Scheduler *sched= (*scheduler_factory)();
  if (sched == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Scheduler initialization failed.\n"));
    exit(1);
  }
  return *sched;
}

