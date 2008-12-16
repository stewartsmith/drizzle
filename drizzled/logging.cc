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
  logging_t *p;

  p= new logging_t;
  if (p == NULL) return 1;
  memset(p, 0, sizeof(logging_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      sql_print_error("logging plugin '%s' init() failed",
                      plugin->name.str);
      goto err;
    }
  }
  return 0;

err:
  delete p;
  return 1;
}

int logging_finalizer(st_plugin_int *plugin)
{
  logging_t *p = (logging_t *) plugin->data;

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      sql_print_error(_("logging plugin '%s' deinit() failed"),
		      plugin->name.str);
    }
  }

  if (p) delete p;

  return 0;
}

/* This gets called by plugin_foreach once for each loaded logging plugin */
static bool logging_pre_iterate (Session *session, plugin_ref plugin,
				 void *p __attribute__ ((__unused__)))
{
  logging_t *l= plugin_data(plugin, logging_t *);

  /* call this loaded logging plugin's logging_pre function pointer */
  if (l && l->logging_pre)
  {
    if (l->logging_pre(session))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      sql_print_error(_("logging plugin '%s' logging_pre() failed"),
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

  foreach_rv= plugin_foreach(session,
			     logging_pre_iterate,
			     DRIZZLE_LOGGER_PLUGIN,
			     NULL);
  return foreach_rv;
}

/* This gets called by plugin_foreach once for each loaded logging plugin */
static bool logging_post_iterate (Session *session, plugin_ref plugin,
				  void *p __attribute__ ((__unused__)))
{
  logging_t *l= plugin_data(plugin, logging_t *);

  if (l && l->logging_post)
  {
    if (l->logging_post(session))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      sql_print_error(_("logging plugin '%s' logging_post() failed"),
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

  foreach_rv= plugin_foreach(session,
			     logging_post_iterate,
			     DRIZZLE_LOGGER_PLUGIN,
			     NULL);
  return foreach_rv;
}
