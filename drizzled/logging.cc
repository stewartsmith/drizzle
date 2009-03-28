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
#include <drizzled/logging.h>
#include <drizzled/gettext.h>

int logging_initializer(st_plugin_int *plugin)
{
  Logging_handler *p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init(&p))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, "logging plugin '%s' init() failed",
                    plugin->name.str);
      return 1;
    }
  }

  plugin->data= (void *)p;

  return 0;
}


int logging_finalizer(st_plugin_int *plugin)
{
  Logging_handler *p = static_cast<Logging_handler *>(plugin->data);

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, _("logging plugin '%s' deinit() failed"),
		      plugin->name.str);
    }
  }

  return 0;
}

/* This gets called by plugin_foreach once for each loaded logging plugin */
static bool logging_pre_iterate (Session *session, plugin_ref plugin, void *)
{
  Logging_handler *handler= plugin_data(plugin, Logging_handler *);

  /* call this loaded logging plugin's logging_pre function pointer */
  if (handler)
  {
    if (handler->pre(session))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("logging plugin '%s' pre() failed"),
                    (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

/* This is the logging_pre_do entry point.
   This gets called by the rest of the Drizzle server code */
bool logging_pre_do (Session *session)
{
  bool foreach_rv;

  foreach_rv= plugin_foreach(session, logging_pre_iterate, DRIZZLE_LOGGER_PLUGIN, NULL);

  return foreach_rv;
}

/* This gets called by plugin_foreach once for each loaded logging plugin */
static bool logging_post_iterate (Session *session, plugin_ref plugin, void *)
{
  Logging_handler *handler= plugin_data(plugin, Logging_handler *);

  if (handler)
  {
    if (handler->post(session))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("logging plugin '%s' post() failed"),
		                (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

/* This is the logging_pre_do entry point.
   This gets called by the rest of the Drizzle server code */
bool logging_post_do (Session *session)
{
  bool foreach_rv;

  foreach_rv= plugin_foreach(session, logging_post_iterate, DRIZZLE_LOGGER_PLUGIN, NULL);

  return foreach_rv;
}
