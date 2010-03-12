/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/function/str/user.h>
#include <drizzled/session.h>

namespace drizzled
{

/**
  @todo
  make USER() replicate properly (currently it is replicated to "")
*/
bool Item_func_user::init(const char *user, const char *host)
{
  assert(fixed == 1);

  // For system threads (e.g. replication SQL thread) user may be empty
  if (user)
  {
    const CHARSET_INFO * const cs= str_value.charset();
    uint32_t res_length= (strlen(user)+strlen(host)+2) * cs->mbmaxlen;

    if (str_value.alloc(res_length))
    {
      null_value=1;
      return true;
    }

    res_length=cs->cset->snprintf(cs, (char*)str_value.ptr(), res_length,
                                  "%s@%s", user, host);
    str_value.length(res_length);
    str_value.mark_as_const();
  }
  return false;
}


bool Item_func_user::fix_fields(Session *session, Item **ref)
{
  return (Item_str_func::fix_fields(session, ref) ||
          init(session->getSecurityContext().getUser().c_str(),
               session->getSecurityContext().getIp().c_str()));
}


bool Item_func_current_user::fix_fields(Session *session, Item **ref)
{
  if (Item_str_func::fix_fields(session, ref))
    return true;

  const SecurityContext *ctx= &session->getSecurityContext();
  return init(ctx->getUser().c_str(), ctx->getIp().c_str());
}

} /* namespace drizzled */
