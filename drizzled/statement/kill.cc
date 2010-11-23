/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/session_list.h>
#include <drizzled/statement/kill.h>

namespace drizzled
{

static bool kill_one_thread(session_id_t id, bool only_kill_query)
{
  drizzled::Session::shared_ptr session= session::Cache::singleton().find(id);

  if (session and session->isViewable())
  {
    session->awake(only_kill_query ? Session::KILL_QUERY : Session::KILL_CONNECTION);
    return true;
  }

  return false;
}

bool statement::Kill::execute()
{
  Item *it= (Item *) session->lex->value_list.head();

  if ((not it->fixed && it->fix_fields(session->lex->session, &it)) || it->check_cols(1))
  {
    my_message(ER_SET_CONSTANTS_ONLY, 
               ER(ER_SET_CONSTANTS_ONLY),
               MYF(0));
    return true;
  }

  if (kill_one_thread(static_cast<session_id_t>(it->val_int()), session->lex->type & ONLY_KILL_QUERY))
  {
    session->my_ok();
  }
  else
  {
    my_error(ER_NO_SUCH_THREAD, MYF(0), it->val_int());
  }

  return false;
}

} /* namespace drizzled */

