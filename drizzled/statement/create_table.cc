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
#include <drizzled/lock.h>
#include <drizzled/session.h>
#include <drizzled/statement/create_table.h>
#include <drizzled/message.h>
#include <drizzled/identifier.h>

#include <iostream>

namespace drizzled
{

bool statement::CreateTable::execute()
{
  TableList *first_table= (TableList *) session->lex->select_lex.table_list.first;
  TableList *all_tables= session->lex->query_tables;
  assert(first_table == all_tables && first_table != 0);
  bool need_start_waiting= false;
  lex_identified_temp_table= createTableMessage().type() == message::Table::TEMPORARY;

  is_engine_set= not createTableMessage().engine().name().empty();

  if (is_engine_set)
  {
    create_info().db_type= 
      plugin::StorageEngine::findByName(*session, createTableMessage().engine().name());

    if (create_info().db_type == NULL)
    {
      my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), 
               createTableMessage().engine().name().c_str());

      return true;
    }
  }
  else /* We now get the default, place it in create_info, and put the engine name in table proto */
  {
    create_info().db_type= session->getDefaultStorageEngine();
  }

  if (not validateCreateTableOption())
  {
    return true;
  }

  if (not lex_identified_temp_table)
  {
    if (session->inTransaction())
    {
      my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
      return true;
    }
  }
  /* Skip first table, which is the table we are creating */
  create_table_list= session->lex->unlink_first_table(&link_to_local);

  drizzled::message::table::init(createTableMessage(), createTableMessage().name(), create_table_list->getSchemaName(), create_info().db_type->getName());

  identifier::Table new_table_identifier(create_table_list->getSchemaName(),
                                       create_table_list->getTableName(),
                                       createTableMessage().type());

  if (not check(new_table_identifier))
  {
    /* put tables back for PS rexecuting */
    session->lex->link_first_table_back(create_table_list, link_to_local);
    return true;
  }

  /* Might have been updated in create_table_precheck */
  create_info().alias= create_table_list->alias;

  /*
     The create-select command will open and read-lock the select table
     and then create, open and write-lock the new table. If a global
     read lock steps in, we get a deadlock. The write lock waits for
     the global read lock, while the global read lock waits for the
     select table to be closed. So we wait until the global readlock is
     gone before starting both steps. Note that
     wait_if_global_read_lock() sets a protection against a new global
     read lock when it succeeds. This needs to be released by
     start_waiting_global_read_lock(). We protect the normal CREATE
     TABLE in the same way. That way we avoid that a new table is
     created during a gobal read lock.
   */
  if (! (need_start_waiting= not session->wait_if_global_read_lock(0, 1)))
  {
    /* put tables back for PS rexecuting */
    session->lex->link_first_table_back(create_table_list, link_to_local);
    return true;
  }

  bool res= executeInner(new_table_identifier);

  /*
    Release the protection against the global read lock and wake
    everyone, who might want to set a global read lock.
  */
  session->startWaitingGlobalReadLock();

  return res;
}

bool statement::CreateTable::executeInner(identifier::Table::const_reference new_table_identifier)
{
  bool res= false;
  Select_Lex *select_lex= &session->lex->select_lex;
  TableList *select_tables= session->lex->query_tables;

  do 
  {
    if (select_lex->item_list.elements)		// With select
    {
      Select_Lex_Unit *unit= &session->lex->unit;
      select_result *result;

      select_lex->options|= SELECT_NO_UNLOCK;
      unit->set_limit(select_lex);

      if (not lex_identified_temp_table)
      {
        session->lex->link_first_table_back(create_table_list, link_to_local);
        create_table_list->setCreate(true);
      }

      if (not (res= session->openTablesLock(session->lex->query_tables)))
      {
        /*
          Is table which we are changing used somewhere in other parts
          of query
        */
        if (not lex_identified_temp_table)
        {
          TableList *duplicate= NULL;
          create_table_list= session->lex->unlink_first_table(&link_to_local);

          if ((duplicate= unique_table(create_table_list, select_tables)))
          {
            my_error(ER_UPDATE_TABLE_USED, MYF(0), create_table_list->alias);
            /* put tables back for PS rexecuting */
            session->lex->link_first_table_back(create_table_list, link_to_local);

            res= true;
            break;
          }
        }

        /*
          select_create is currently not re-execution friendly and
          needs to be created for every execution of a PS/SP.
        */
        if ((result= new select_create(create_table_list,
                                       session->getLex()->exists(),
                                       &create_info(),
                                       createTableMessage(),
                                       &alter_info,
                                       select_lex->item_list,
                                       session->lex->duplicates,
                                       session->lex->ignore,
                                       select_tables,
                                       new_table_identifier)))
        {
          /*
            CREATE from SELECT give its Select_Lex for SELECT,
            and item_list belong to SELECT
          */
          res= handle_select(session, session->lex, result, 0);
          delete result;
        }
      }
      else if (not lex_identified_temp_table)
      {
        create_table_list= session->lex->unlink_first_table(&link_to_local);
      }
    }
    else
    {
      /* regular create */
      if (is_create_table_like)
      {
        res= create_like_table(session, 
                               new_table_identifier,
                               identifier::Table(select_tables->getSchemaName(),
                                                 select_tables->getTableName()),
                               createTableMessage(),
                               session->getLex()->exists(),
                               is_engine_set);
      }
      else
      {

        for (int32_t x= 0; x < alter_info.alter_proto.added_field_size(); x++)
        {
          message::Table::Field *field= createTableMessage().add_field();

          *field= alter_info.alter_proto.added_field(x);
        }

        res= create_table(session, 
                          new_table_identifier,
                          &create_info(),
                          createTableMessage(),
                          &alter_info, 
                          false, 
                          0,
                          session->getLex()->exists());
      }

      if (not res)
      {
        session->my_ok();
      }
    }
  } while (0);

  return res;
}

bool statement::CreateTable::check(const identifier::Table &identifier)
{
  // Check table name for validity
  if (not identifier.isValid())
    return false;

  // See if any storage engine objects to the name of the file
  if (not plugin::StorageEngine::canCreateTable(identifier))
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), "", "", identifier.getSchemaName().c_str());

    return false;
  }

  // Make sure the schema exists, we will do this again during the actual
  // create for the table.
  if (not plugin::StorageEngine::doesSchemaExist(identifier))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), identifier.getSchemaName().c_str());

    return false;
  }

  return true;
}

bool statement::CreateTable::validateCreateTableOption()
{
  bool rc= true;
  size_t num_engine_options= createTableMessage().engine().options_size();

  assert(create_info().db_type);

  for (size_t y= 0; y < num_engine_options; ++y)
  {
    bool valid= create_info().db_type->validateCreateTableOption(createTableMessage().engine().options(y).name(),
                                                                 createTableMessage().engine().options(y).state());

    if (not valid)
    {
      my_error(ER_UNKNOWN_ENGINE_OPTION, MYF(0),
               createTableMessage().engine().options(y).name().c_str(),
               createTableMessage().engine().options(y).state().c_str());

      rc= false;
    }
  }

  return rc;
}

} /* namespace drizzled */

