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

#include <vector>

using namespace std;

static vector<Authentication *> all_authentication;

static bool are_plugins_loaded= false;

static void add_authentication(Authentication *auth)
{
  all_authentication.push_back(auth);
}

static void remove_authentication(Authentication *auth)
{
  all_authentication.erase(find(all_authentication.begin(),
                                all_authentication.end(),
                                auth));
}

class AuthenticateBy : public unary_function<Authentication *, bool>
{
  Session *session;
  const char *password;
public:
  AuthenticateBy(Session *session_arg, const char *password_arg) :
    unary_function<Authentication *, bool>(),
    session(session_arg), password(password_arg) {}

  inline result_type operator()(argument_type auth)
  {
    return auth->authenticate(session, password);
  }
};

bool authenticate_user(Session *session, const char *password)
{
  /* If we never loaded any auth plugins, just return true */
  if (are_plugins_loaded != true)
    return true;

  /* Use find_if instead of foreach so that we can collect return codes */
  vector<Authentication *>::iterator iter=
    find_if(all_authentication.begin(), all_authentication.end(),
            AuthenticateBy(session, password));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_authentication.end();
}


int authentication_initializer(st_plugin_int *plugin)
{
  Authentication *authen= NULL;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init(&authen))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Plugin '%s' init function returned error."),
                    plugin->name.str);
      return 1;
    }
  }

  if (authen == NULL)
    return 1;

  add_authentication(authen);
  plugin->data= authen;
  are_plugins_loaded= true;

  return 0;
}

int authentication_finalizer(st_plugin_int *plugin)
{
  Authentication *authen= static_cast<Authentication *>(plugin->data);
  assert(authen);

  remove_authentication(authen);

  if (authen && plugin->plugin->deinit)
    plugin->plugin->deinit(authen);

  return(0);
}
