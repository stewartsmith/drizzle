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
#include <drizzled/plugin_scheduling.h>
#include <drizzled/connect.h>

Scheduler *thread_scheduler= NULL;

static bool scheduler_inited= false; /* We must insist that only one of these plugins get loaded at a time */


extern char *opt_scheduler;

Scheduler *get_thread_scheduler()
{
  return thread_scheduler;
}

int scheduling_initializer(st_plugin_int *plugin)
{
  if (memcmp(plugin->plugin->name, opt_scheduler, strlen(opt_scheduler)))
    return 0;

  if (scheduler_inited)
  {
    fprintf(stderr, "You cannot load more then one scheduler plugin\n");
    exit(1);
  }

  assert(plugin->plugin->init); /* Find poorly designed plugins */

  if (plugin->plugin->init((void *)&thread_scheduler))
  {
    /* 
      TRANSLATORS> The leading word "scheduling" is the name
      of the plugin api, and so should not be translated. 
    */
    errmsg_printf(ERRMSG_LVL_ERROR, _("scheduling plugin '%s' init() failed"),
	                plugin->name.str);
      return 1;
  }

  scheduler_inited= true;
  /* We populate so we can find which plugin was initialized later on */
  plugin->data= (void *)thread_scheduler;
  plugin->state= PLUGIN_IS_READY;

  return 0;

}

int scheduling_finalizer(st_plugin_int *plugin)
{
  /* We know which one we initialized since its data pointer is filled */
  if (plugin->plugin->deinit && plugin->data)
  {
    if (plugin->plugin->deinit((void *)thread_scheduler))
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
