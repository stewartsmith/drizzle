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

#include "config.h"
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/set_option.h>

namespace drizzled
{

bool statement::SetOption::execute()
{
  TableList *all_tables= session->lex->query_tables;
  List<set_var_base> *lex_var_list= &session->lex->var_list;

  if (session->openTablesLock(all_tables))
  {
    return true;
  }
  bool res= sql_set_variables(session, lex_var_list);
  if (res)
  {
    /*
     * We encountered some sort of error, but no message was sent.
     * Send something semi-generic here since we don't know which
     * assignment in the list caused the error.
     */
    if (! session->is_error())
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "SET");
    }
  }
  else
  {
    session->my_ok();
  }

  return res;
}

} /* namespace drizzled */
