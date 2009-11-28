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
#include <drizzled/lock.h>
#include <drizzled/statement/drop_table.h>
#include <drizzled/plugin/info_schema_table.h>

namespace drizzled
{


/*
 delete (drop) tables.

  SYNOPSIS
   mysql_rm_table()
   session			Thread handle
   tables		List of tables to delete
   if_exists		If 1, don't give error if one table doesn't exists

  NOTES
    Will delete all tables that can be deleted and give a compact error
    messages for tables that could not be deleted.
    If a table is in use, we will wait for all users to free the table
    before dropping it

    Wait if global_read_lock (FLUSH TABLES WITH READ LOCK) is set, but
    not if under LOCK TABLES.

  RETURN
    false OK.  In this case ok packet is sent to user
    true  Error

*/

static bool mysql_rm_table(Session *session, TableList *tables, bool if_exists, bool drop_temporary)
{
  bool error, need_start_waiting= false;

  /**
   * @todo this is a result of retaining the behavior that was here before. This should be removed
   * and the correct error handling should be done in doDropTable for the I_S engine.
   */
  plugin::InfoSchemaTable *sch_table= plugin::InfoSchemaTable::getTable(tables->table_name);
  if (sch_table)
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), "", "", INFORMATION_SCHEMA_NAME.c_str());
    return true;
  }

  /* mark for close and remove all cached entries */

  if (! drop_temporary)
  {
    if (!(need_start_waiting= !wait_if_global_read_lock(session, false, true)))
      return true;
  }

  /*
    Acquire LOCK_open after wait_if_global_read_lock(). If we would hold
    LOCK_open during wait_if_global_read_lock(), other threads could not
    close their tables. This would make a pretty deadlock.
  */
  error= mysql_rm_table_part2(session, tables, if_exists, drop_temporary);

  if (need_start_waiting)
    start_waiting_global_read_lock(session);

  if (error)
    return true;

  session->my_ok();

  return false;
}

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
  /* DDL and binlog write order protected by LOCK_open */
  bool res= mysql_rm_table(session,
                           first_table,
                           drop_if_exists,
                           drop_temporary);
  return res;
}

} /* namespace drizzled */
