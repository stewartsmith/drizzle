/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include "drizzled/server_includes.h"
#include "drizzled/slot/authentication.h"
#include "drizzled/errmsg_print.h"
#include "drizzled/plugin/registry.h"
#include "drizzled/plugin/authentication.h"
#include "drizzled/gettext.h"

#include <vector>

using namespace drizzled;
using namespace std;


void slot::Authentication::add(plugin::Authentication *auth)
{
  if (auth != NULL)
    all_authentication.push_back(auth);
}

void slot::Authentication::remove(plugin::Authentication *auth)
{
  if (auth != NULL)
    all_authentication.erase(find(all_authentication.begin(),
                                  all_authentication.end(),
                                  auth));
}

namespace drizzled
{
namespace slot
{
namespace auth_priv
{

class AuthenticateBy : public unary_function<plugin::Authentication *, bool>
{
  Session *session;
  const char *password;
public:
  AuthenticateBy(Session *session_arg, const char *password_arg) :
    unary_function<plugin::Authentication *, bool>(),
    session(session_arg), password(password_arg) {}

  inline result_type operator()(argument_type auth)
  {
    return auth->authenticate(session, password);
  }
};
} /* namespace auth_priv */
} /* namespace slot */
} /* namespace drizzled */

bool slot::Authentication::authenticate(Session *session, const char *password)
{
  /* If we never loaded any auth plugins, just return true */
  if (are_plugins_loaded != true)
    return true;

  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::Authentication *>::iterator iter=
    find_if(all_authentication.begin(), all_authentication.end(),
            slot::auth_priv::AuthenticateBy(session, password));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_authentication.end();
}

