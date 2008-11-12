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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/functions/str/user.h>

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
  return (Item_func_sysconst::fix_fields(session, ref) ||
          init(session->main_security_ctx.user,
               session->main_security_ctx.ip));
}


bool Item_func_current_user::fix_fields(Session *session, Item **ref)
{
  if (Item_func_sysconst::fix_fields(session, ref))
    return true;

  Security_context *ctx=
                         session->security_ctx;
  return init(ctx->user, ctx->ip);
}


