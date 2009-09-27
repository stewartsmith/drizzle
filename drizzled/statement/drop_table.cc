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

#include <drizzled/server_includes.h>
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/drop_table.h>

namespace drizzled
{

bool statement::DropTable::execute()
{
  TableList *first_table= (TableList *) session->lex->select_lex.table_list.first;
  TableList *all_tables= session->lex->query_tables;
  assert(first_table == all_tables && first_table != 0);
  if (! drop_temporary)
  {
    if (! session->endActiveTransaction())
    {
      return true;
    }
  }
  else
  {
    /* So that DROP TEMPORARY TABLE gets to binlog at commit/rollback */
    session->options|= OPTION_KEEP_LOG;
  }
  /* DDL and binlog write order protected by LOCK_open */
  bool res= mysql_rm_table(session, 
                           first_table, 
                           drop_if_exists, 
                           drop_temporary);
  return res;
}

} /* namespace drizzled */
