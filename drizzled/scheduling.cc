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


SchedulerFactory *scheduler_factory= NULL;

static bool scheduler_inited= false; /* We must insist that only one of these plugins get loaded at a time */


extern char *opt_scheduler;

bool add_scheduler_factory(SchedulerFactory *factory)
{
  if (factory->getName() != opt_scheduler)
    return true;

  if (scheduler_inited)
  {
    fprintf(stderr, "You cannot load more then one scheduler plugin\n");
    return(1);
  }
  scheduler_factory= factory;

  scheduler_inited= true;
  return false;
}

bool remove_scheduler_factory(SchedulerFactory *)
{
  scheduler_factory= NULL;
  scheduler_inited= false;
  return false;
}

Scheduler &get_thread_scheduler()
{
  assert(scheduler_factory != NULL);
  Scheduler *sched= (*scheduler_factory)();
  if (sched == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Scheduler initialization failed."));
    exit(1);
  }
  return *sched;
}

