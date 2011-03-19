/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <drizzled/kill.h>
#include <drizzled/session.h>
#include <drizzled/statement/kill.h>
#include <drizzled/sql_lex.h>

namespace drizzled {
namespace statement {

Kill::Kill(Session *in_session, Item *item, bool is_query_kill) :
  Statement(in_session)
  {
    if (is_query_kill)
    {
      lex().type= ONLY_KILL_QUERY;
    }

    lex().value_list.clear();
    lex().value_list.push_front(item);
    set_command(SQLCOM_KILL);
  }

} // namespace statement

bool statement::Kill::execute()
{
  Item *it= &lex().value_list.front();

  if ((not it->fixed && it->fix_fields(lex().session, &it)) || it->check_cols(1))
  {
    my_message(ER_SET_CONSTANTS_ONLY, 
               ER(ER_SET_CONSTANTS_ONLY),
               MYF(0));
    return true;
  }

  if (drizzled::kill(*session().user(), static_cast<session_id_t>(it->val_int()), lex().type & ONLY_KILL_QUERY))
  {
    session().my_ok();
  }
  else
  {
    my_error(ER_NO_SUCH_THREAD, MYF(0), it->val_int());
  }

  return false;
}

} /* namespace drizzled */

