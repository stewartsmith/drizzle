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
#include <drizzled/session.h>
#include <drizzled/statement/show.h>
#include <drizzled/sql_lex.h>
#include <drizzled/statistics_variables.h>

namespace drizzled {
namespace statement {

Show::Show(Session *in_session) :
  Select(in_session),
  if_exists(false)
  {
  }

} /* namespace statement */

bool statement::Show::execute()
{
  TableList *all_tables= lex().query_tables;
  session().status_var.last_query_cost= 0.0;
  bool res= execute_sqlcom_select(&session(), all_tables);

  return res;
}

} /* namespace drizzled */

