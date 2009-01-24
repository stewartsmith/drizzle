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

scheduling_st thread_scheduler;
static bool scheduler_inited= false; /* We must insist that only one of these plugins get loaded at a time */
static bool has_been_seen= false; /* We are still in a single thread when we see this variable, so no lock needed. */

static void post_kill_dummy(Session *) { return; }
static bool end_thread_dummy(Session *, bool) { return false; }

int scheduling_initializer(st_plugin_int *plugin)
{
  if (has_been_seen == false)
  {
    memset(&thread_scheduler, 0, sizeof(scheduling_st));
    has_been_seen= true;
  }

  if (plugin->plugin->init)
  {

    thread_scheduler.post_kill_notification= post_kill_dummy;
    thread_scheduler.end_thread= end_thread_dummy;
    thread_scheduler.init_new_connection_thread= init_new_connection_handler_thread;

    if (plugin->plugin->init((void *)&thread_scheduler))
    {
      /* 
        TRANSLATORS> The leading word "scheduling" is the name
        of the plugin api, and so should not be translated. 
      */
      errmsg_printf(ERRMSG_LVL_ERROR, _("scheduling plugin '%s' init() failed"),
		      plugin->name.str);
      goto err;
    }
  }

  if (thread_scheduler.is_used == true)
  {
    /* We are going to assert() on any plugin that is not well written. */
    assert(thread_scheduler.max_threads);
    assert(thread_scheduler.init_new_connection_thread);
    assert(thread_scheduler.add_connection);
    assert(thread_scheduler.post_kill_notification);
    assert(thread_scheduler.end_thread);

    if (scheduler_inited)
    {
      fprintf(stderr, "You cannot load more then one scheduler plugin\n");
      exit(1);
    }

    scheduler_inited= true;
    /* We populate so we can find which plugin was initialized later on */
    plugin->data= (void *)&thread_scheduler;
  }

  return 0;

err:

  return 1;
}

int scheduling_finalizer(st_plugin_int *plugin)
{
  /* We know which one we initialized since its data pointer is filled */
  if (plugin->plugin->deinit && plugin->data)
  {
    if (plugin->plugin->deinit((void *)&thread_scheduler))
    {
      /* TRANSLATORS: The leading word "scheduling" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, _("scheduling plugin '%s' deinit() failed"),
                    plugin->name.str);
    }
  }

  return 0;
}
