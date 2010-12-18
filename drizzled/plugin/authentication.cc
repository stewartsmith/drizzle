/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "config.h"
#include "drizzled/plugin/authentication.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"
#include "drizzled/security_context.h"

#include <vector>

namespace drizzled
{

std::vector<plugin::Authentication *> all_authentication;


bool plugin::Authentication::addPlugin(plugin::Authentication *auth)
{
  if (auth != NULL)
    all_authentication.push_back(auth);
  return false;
}

void plugin::Authentication::removePlugin(plugin::Authentication *auth)
{
  if (auth != NULL)
    all_authentication.erase(std::find(all_authentication.begin(),
                                       all_authentication.end(),
                                       auth));
}

class AuthenticateBy : public std::unary_function<plugin::Authentication *, bool>
{
  const SecurityContext &sctx;
  const std::string &password;
public:
  AuthenticateBy(const SecurityContext &sctx_arg, const std::string &password_arg) :
    std::unary_function<plugin::Authentication *, bool>(),
    sctx(sctx_arg), password(password_arg) {}

  inline result_type operator()(argument_type auth)
  {
    return auth->authenticate(sctx, password);
  }
};

bool plugin::Authentication::isAuthenticated(const SecurityContext &sctx,
                                             const std::string &password)
{
  /* If we never loaded any auth plugins, just return true */
  if (all_authentication.empty())
    return true;

  /* Use find_if instead of foreach so that we can collect return codes */
  std::vector<plugin::Authentication *>::iterator iter=
    std::find_if(all_authentication.begin(), all_authentication.end(),
                 AuthenticateBy(sctx, password));

  /* We only require one plugin to return success in order to authenticate.
   * If iter is == end() here, that means that all of the plugins returned
   * false, which means they all failed.
   */
  if (iter == all_authentication.end())
  {
    my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
             sctx.getUser().c_str(),
             sctx.getIp().c_str(),
             password.empty() ? ER(ER_NO) : ER(ER_YES));
    return false;
  }
  return true;
}

} /* namespace drizzled */
