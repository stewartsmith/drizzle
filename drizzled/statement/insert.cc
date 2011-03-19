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
#include <drizzled/show.h>
#include <drizzled/lock.h>
#include <drizzled/session.h>
#include <drizzled/probes.h>
#include <drizzled/statement/insert.h>
#include <drizzled/sql_lex.h>

namespace drizzled {

bool statement::Insert::execute()
{
  TableList *first_table= (TableList *) lex().select_lex.table_list.first;
  TableList *all_tables= lex().query_tables;
  assert(first_table == all_tables && first_table != 0);
  bool need_start_waiting= false;

  if (insert_precheck(&session(), all_tables))
  {
    return true;
  }

  if (! (need_start_waiting= ! session().wait_if_global_read_lock(false, true)))
  {
    return true;
  }

  DRIZZLE_INSERT_START(session().getQueryString()->c_str());

  bool res= insert_query(&session(),
                         all_tables,
                         lex().field_list,
                         lex().many_values,
                         lex().update_list,
                         lex().value_list,
                         lex().duplicates,
                         lex().ignore);
  /*
     Release the protection against the global read lock and wake
     everyone, who might want to set a global read lock.
   */
  session().startWaitingGlobalReadLock();

  return res;
}

} /* namespace drizzled */
