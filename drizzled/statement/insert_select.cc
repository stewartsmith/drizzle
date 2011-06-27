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
#include <drizzled/statement/insert_select.h>
#include <drizzled/select_insert.h>
#include <drizzled/sql_lex.h>
#include <drizzled/open_tables_state.h>

namespace drizzled {

bool statement::InsertSelect::execute()
{
  TableList *first_table= (TableList *) lex().select_lex.table_list.first;
  TableList *all_tables= lex().query_tables;
  assert(first_table == all_tables && first_table != 0);
  Select_Lex *select_lex= &lex().select_lex;
  Select_Lex_Unit *unit= &lex().unit;
  select_result *sel_result= NULL;
  bool res= false;
  bool need_start_waiting= false;

  if (insert_precheck(&session(), all_tables))
  {
    return true;
  }

  /* Don't unlock tables until command is written to binary log */
  select_lex->options|= SELECT_NO_UNLOCK;

  unit->set_limit(select_lex);

  if (! (need_start_waiting= not session().wait_if_global_read_lock(false, true)))
  {
    return true;
  }

  if (! (res= session().openTablesLock(all_tables)))
  {
    DRIZZLE_INSERT_SELECT_START(session().getQueryString()->c_str());
    /* Skip first table, which is the table we are inserting in */
    TableList *second_table= first_table->next_local;
    select_lex->table_list.first= (unsigned char*) second_table;
    select_lex->context.table_list= select_lex->context.first_name_resolution_table= second_table;
    res= insert_select_prepare(&session());
    if (not res)
    {
      sel_result= new select_insert(first_table, first_table->table, &lex().field_list, &lex().update_list, &lex().value_list, 
        lex().duplicates, lex().ignore);
      res= handle_select(&session(), &lex(), sel_result, OPTION_SETUP_TABLES_DONE);

      /*
         Invalidate the table in the query cache if something changed
         after unlocking when changes become visible.
         TODO: this is a workaround. right way will be move invalidating in
         the unlock procedure.
       */
      if (first_table->lock_type == TL_WRITE_CONCURRENT_INSERT && session().open_tables.lock)
      {
        /* INSERT ... SELECT should invalidate only the very first table */
        TableList *save_table= first_table->next_local;
        first_table->next_local= 0;
        first_table->next_local= save_table;
      }
      delete sel_result;
    }
    /* revert changes for SP */
    select_lex->table_list.first= (unsigned char*) first_table;
  }

  /*
     Release the protection against the global read lock and wake
     everyone, who might want to set a global read lock.
   */
  session().startWaitingGlobalReadLock();

  return res;
}

} /* namespace drizzled */
