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

int scheduling_initializer(st_plugin_int *plugin)
{

  SchedulerFactory *factory= NULL;

  assert(plugin->plugin->init); /* Find poorly designed plugins */

  if (plugin->plugin->init((void *)&factory))
  {
    /* 
      TRANSLATORS> The leading word "scheduling" is the name
      of the plugin api, and so should not be translated. 
    */
    errmsg_printf(ERRMSG_LVL_ERROR, _("scheduling plugin '%s' init() failed"),
	                plugin->name.str);
      return 1;
  }

  Plugin_registry &registry= Plugin_registry::get_plugin_registry();
  if (factory != NULL)
    registry.registerPlugin(factory);
  
  /* We populate so we can find which plugin was initialized later on */
  plugin->data= (void *)factory;

  return 0;

}

int scheduling_finalizer(st_plugin_int *plugin)
{
  /* We know which one we initialized since its data pointer is filled */
  if (plugin->plugin->deinit && plugin->data)
  {
    if (plugin->plugin->deinit((void *)plugin->data))
    {
      /* TRANSLATORS: The leading word "scheduling" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("scheduling plugin '%s' deinit() failed"),
                    plugin->name.str);
    }
  }

  return 0;
}
