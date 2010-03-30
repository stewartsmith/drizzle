/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor <mordred@inaugust.com>
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

#include "config.h"

#include <string>

#include "drizzled/plugin/authorization.h"
#include "drizzled/security_context.h"


using namespace drizzled;

namespace authz
{

class Authz :
  public plugin::Authorization
{
public:
  Authz() :
    plugin::Authorization("test authz")
  { }

  /* I told you it was a silly plugin.
    If the db name is authz_no and the user is authz, then it's blocked
    If it's anything else, it's approved
  */
  virtual bool restrictSchema(const SecurityContext &user_ctx,
                              const std::string &db)
  {
    if (db == "authz_no" and user_ctx.getUser() == "authz")
      return true;
    return false;
  }

  virtual bool restrictProcess(const SecurityContext &user_ctx,
                               const SecurityContext &session_ctx)
  {
    if (user_ctx.getUser() == session_ctx.getUser())
      return false;
    if (user_ctx.getUser() == "authz")
      return true;
    return false;
  }
};

Authz *authz= NULL;

static int init(plugin::Context &context)
{
  authz= new Authz();
  context.add(authz);
  return 0;
}

} /* namespace authz */

DRIZZLE_PLUGIN(authz::init, NULL);
