/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Brian Aker
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

#include <drizzled/server_includes.h>
#include <drizzled/authentication.h>
#include <drizzled/gettext.h>
#include <drizzled/errmsg_print.h>

static bool are_plugins_loaded= false;

static bool authenticate_by(Session *session, plugin_ref plugin, void* p_data)
{
  const char *password= (const char *)p_data;
  authentication_st *auth= plugin_data(plugin, authentication_st *);

  (void)p_data;

  if (auth && auth->authenticate)
  {
    if (auth->authenticate(session, password))
      return true;
  }

  return false;
}

bool authenticate_user(Session *session, const char *password)
{
  /* If we never loaded any auth plugins, just return true */
  if (are_plugins_loaded != true)
    return true;

  return plugin_foreach(session, authenticate_by, DRIZZLE_AUTH_PLUGIN, (void *)password);
}


int authentication_initializer(st_plugin_int *plugin)
{
  authentication_st *authen;

  authen= new authentication_st;

  if (authen == NULL)
    return 1;

  memset(authen, 0, sizeof(authentication_st));

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init(authen))
    {
      sql_print_error(_("Plugin '%s' init function returned error."),
                      plugin->name.str);
      goto err;
    }
  }

  plugin->data= (void *)authen;
  are_plugins_loaded= true;

  return(0);
err:
  delete authen;
  return(1);
}

int authentication_finalizer(st_plugin_int *plugin)
{
  authentication_st *authen= (authentication_st *)plugin->data;

  assert(authen);
  if (authen && plugin->plugin->deinit)
    plugin->plugin->deinit(authen);

  delete authen;

  return(0);
}
