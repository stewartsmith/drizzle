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
#include <drizzled/statement/rename_table.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/transaction_services.h>
#include <drizzled/sql_lex.h>
#include <drizzled/table/cache.h>

namespace drizzled {

bool statement::RenameTable::execute()
{
  TableList *first_table= (TableList *) lex().select_lex.table_list.first;
  TableList *all_tables= lex().query_tables;
  assert(first_table == all_tables && first_table != 0);
  TableList *table;

  if (session().inTransaction())
  {
    my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
    return true;
  }

  for (table= first_table; table; table= table->next_local->next_local)
  {
    TableList old_list, new_list;
    /*
       we do not need initialize old_list and new_list because we will
       come table[0] and table->next[0] there
     */
    old_list= table[0];
    new_list= table->next_local[0];
  }

  if (renameTables(first_table))
  {
    return true;
  }

  return false;
}

bool statement::RenameTable::renameTables(TableList *table_list)
{
  bool error= true;
  TableList *ren_table= NULL;

  /*
    Avoid problems with a rename on a table that we have locked or
    if the user is trying to to do this in a transcation context
  */
  if (session().inTransaction())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION, ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    return true;
  }

  if (session().wait_if_global_read_lock(false, true))
    return true;

  {
    boost::mutex::scoped_lock scopedLock(table::Cache::mutex()); /* Rename table lock for exclusive access */

    if (not session().lock_table_names_exclusively(table_list))
    {
      error= false;
      ren_table= renameTablesInList(table_list, false);

      if (ren_table)
      {
        /* Rename didn't succeed;  rename back the tables in reverse order */
        TableList *table;

        /* Reverse the table list */
        table_list= reverseTableList(table_list);

        /* Find the last renamed table */
        for (table= table_list;
             table->next_local != ren_table;
             table= table->next_local->next_local) 
        { /* do nothing */ }

        table= table->next_local->next_local;		// Skip error table

        /* Revert to old names */
        renameTablesInList(table, true);
        error= true;
      }

      table_list->unlock_table_names();
    }
  }

  /* Lets hope this doesn't fail as the result will be messy */
  if (not error)
  {
    TransactionServices::rawStatement(session(), *session().getQueryString(), *session().schema());        
    session().my_ok();
  }

  session().startWaitingGlobalReadLock();

  return error;
}

TableList *statement::RenameTable::reverseTableList(TableList *table_list)
{
  TableList *prev= NULL;

  while (table_list)
  {
    TableList *next= table_list->next_local;
    table_list->next_local= prev;
    prev= table_list;
    table_list= next;
  }
  return prev;
}

bool statement::RenameTable::rename(TableList *ren_table,
                                    const char *new_db,
                                    const char *new_table_name,
                                    bool skip_error)
{
  bool rc= true;
  const char *new_alias, *old_alias;

  {
    old_alias= ren_table->getTableName();
    new_alias= new_table_name;
  }

  plugin::StorageEngine *engine= NULL;
  message::table::shared_ptr table_message;

  identifier::Table old_identifier(ren_table->getSchemaName(), old_alias, message::Table::STANDARD);

  if (not (table_message= plugin::StorageEngine::getTableMessage(session(), old_identifier)))
  {
    my_error(ER_TABLE_UNKNOWN, old_identifier);
    return true;
  }

  engine= plugin::StorageEngine::findByName(session(), table_message->engine().name());

  identifier::Table new_identifier(new_db, new_alias, message::Table::STANDARD);
  if (plugin::StorageEngine::doesTableExist(session(), new_identifier))
  {
    my_error(ER_TABLE_EXISTS_ERROR, new_identifier);
    return 1; // This can't be skipped
  }

  rc= rename_table(session(), engine, old_identifier, new_identifier);
  if (rc && ! skip_error)
    return true;

  return false;
}

TableList *statement::RenameTable::renameTablesInList(TableList *table_list,
                                                      bool skip_error)
{
  TableList *ren_table, *new_table;

  for (ren_table= table_list; ren_table; ren_table= new_table->next_local)
  {
    new_table= ren_table->next_local;
    if (rename(ren_table, new_table->getSchemaName(), new_table->getTableName(), skip_error))
      return ren_table;
  }
  return 0;
}

} /* namespace drizzled */
