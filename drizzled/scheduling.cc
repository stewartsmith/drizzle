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
#include <drizzled/scheduler.h>

extern scheduler_functions thread_scheduler;

int scheduling_initializer(st_plugin_int *plugin)
{
  scheduling_t *p;

  p= new scheduling_t;
  if (p == NULL) return 1;
  memset(p, 0, sizeof(scheduling_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
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
  return 0;

err:
  free(p);
  return 1;
}

int scheduling_finalizer(st_plugin_int *plugin)
{
  scheduling_t *p= (scheduling_t *) plugin->data;

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      /* TRANSLATORS: The leading word "scheduling" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, _("scheduling plugin '%s' deinit() failed"),
		      plugin->name.str);
    }
  }

  if (p) free(p);

  return 0;
}
