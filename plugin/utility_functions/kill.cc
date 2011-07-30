/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <config.h>

#include <drizzled/session.h>
#include <drizzled/session/cache.h>
#include <plugin/utility_functions/functions.h>

namespace drizzled {
namespace utility_functions {

int64_t Kill::val_int()
{
  int64_t session_id_for_kill= args[0]->val_int();

  if (getSession().getSessionId() == session_id_for_kill)
  {
    my_error(drizzled::ER_KILL_DENY_SELF_ERROR, MYF(0));
    return 0;
  }

  Session::shared_ptr die_session(session::Cache::find(session_id_for_kill));

  if (not die_session)
  {
    my_error(drizzled::ER_NO_SUCH_THREAD, session_id_for_kill, MYF(0));
    return 0;
  }

  if (not die_session->isViewable(*getSession().user()))
  {
    my_error(drizzled::ER_KILL_DENIED_ERROR, session_id_for_kill, MYF(0));
    return 0;
  }

  die_session->awake(arg_count == 2 ? Session::KILL_QUERY : Session::KILL_CONNECTION);

  return session_id_for_kill;
}

} /* namespace utility_functions */
} /* namespace drizzled */
