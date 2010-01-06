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
#include <drizzled/lock.h>
#include <drizzled/session.h>
#include <drizzled/statement/create_table.h>
#include <drizzled/table_identifier.h>
#include <list>

using namespace std;

namespace drizzled
{

void statement::CreateTable::processBuiltinOptions()
{
  list<drizzled::message::Table::StorageEngine::EngineOption>::iterator it= parsed_engine_options.begin();

  while (it != parsed_engine_options.end())
  {
    if (my_strcasecmp(system_charset_info, (*it).option_name().c_str(),
                      "ENGINE") == 0)
    {
      message::Table::StorageEngine *engine_message;
      engine_message= create_table_proto.mutable_engine();
      is_engine_set= true;
      engine_message->set_name((*it).option_value());
    }
    else if (my_strcasecmp(system_charset_info, (*it).option_name().c_str(),
                           "BLOCK_SIZE") == 0)
    {
      message::Table::TableOptions *tableopts;
      tableopts= create_table_proto.mutable_options();

      tableopts->set_block_size(strtoull((*it).option_value().c_str(), NULL, 10)); // FIXME
      create_info.used_fields|= HA_CREATE_USED_BLOCK_SIZE;
    }
    else if (my_strcasecmp(system_charset_info, (*it).option_name().c_str(),
                           "COMMENT") == 0)
    {
      message::Table::TableOptions *tableopts;
      tableopts= create_table_proto.mutable_options();
      tableopts->set_comment((*it).option_value());
    }
    else if (my_strcasecmp(system_charset_info, (*it).option_name().c_str(),
                           "AUTO_INCREMENT") == 0)
    {
      message::Table::TableOptions *tableopts;
      tableopts= create_table_proto.mutable_options();

      create_info.auto_increment_value=strtoull((*it).option_value().c_str(), NULL, 10); // FIXME
      create_info.used_fields|= HA_CREATE_USED_AUTO;
      tableopts->set_auto_increment_value(strtoull((*it).option_value().c_str(), NULL, 10)); // FIXME
    }
    else if (my_strcasecmp(system_charset_info, (*it).option_name().c_str(),
                           "ROW_FORMAT") == 0)
    {
      enum row_type row_type;
      const char* option_value= (*it).option_value().c_str();

      if (my_strcasecmp(system_charset_info, option_value, "DEFAULT") == 0)
        row_type= ROW_TYPE_DEFAULT;
      else if (my_strcasecmp(system_charset_info, option_value, "FIXED") == 0)
        row_type= ROW_TYPE_FIXED;
      else if (my_strcasecmp(system_charset_info, option_value, "DYNAMIC") == 0)
        row_type= ROW_TYPE_DYNAMIC;
      else if (my_strcasecmp(system_charset_info, option_value, "COMPRESSED") == 0)
        row_type= ROW_TYPE_COMPRESSED;
      else if (my_strcasecmp(system_charset_info, option_value, "REDUNDANT") == 0)
        row_type= ROW_TYPE_REDUNDANT;
      else if (my_strcasecmp(system_charset_info, option_value, "COMPACT") == 0)
        row_type= ROW_TYPE_COMPACT;
      else if (my_strcasecmp(system_charset_info, option_value, "PAGE") == 0)
        row_type= ROW_TYPE_PAGE;
      else
        abort(); // FIXME


      create_info.row_type= row_type;
      create_info.used_fields|= HA_CREATE_USED_ROW_FORMAT;
      alter_info.flags.set(ALTER_ROW_FORMAT);
    }
    else if (my_strcasecmp(system_charset_info, (*it).option_name().c_str(),
                           "KEY_BLOCK_SIZE") == 0)
    {
      message::Table::TableOptions *tableopts;
      tableopts= create_table_proto.mutable_options();

      tableopts->set_key_block_size(strtoull((*it).option_value().c_str(), NULL, 10)); // FIXME
      create_info.used_fields|= HA_CREATE_USED_KEY_BLOCK_SIZE;
    }
    else if (my_strcasecmp(system_charset_info, (*it).option_name().c_str(),
                           "COLLATE") == 0)
    {
      const CHARSET_INFO *cs= get_charset_by_name((*it).option_value().c_str());
      if (! cs)
      {
        my_error(ER_UNKNOWN_COLLATION, MYF(0), (*it).option_value().c_str());
        return ;
      }
      create_info.default_table_charset= cs;
      create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
    }

//    parsed_engine_options.erase(it);

    it++;
  }
}

bool statement::CreateTable::execute()
{
  TableList *first_table= (TableList *) session->lex->select_lex.table_list.first;
  TableList *all_tables= session->lex->query_tables;
  assert(first_table == all_tables && first_table != 0);
  Select_Lex *select_lex= &session->lex->select_lex;
  Select_Lex_Unit *unit= &session->lex->unit;
  bool need_start_waiting= false;
  bool res= false;
  bool link_to_local= false;
  bool lex_identified_temp_table= 
    create_table_proto.type() == drizzled::message::Table::TEMPORARY;

  processBuiltinOptions();

  if (is_engine_set)
  {
    create_info.db_type= 
      plugin::StorageEngine::findByName(*session, create_table_proto.engine().name());

    if (create_info.db_type == NULL)
    {
      my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), 
               create_table_proto.name().c_str());

      return true;
    }
  }
  else /* We now get the default, place it in create_info, and put the engine name in table proto */
  {
    create_info.db_type= session->getDefaultStorageEngine();
  }


  /* 
    Now we set the name in our Table proto so that it will match 
    create_info.db_type.
  */
  {
    message::Table::StorageEngine *protoengine;

    protoengine= create_table_proto.mutable_engine();
    protoengine->set_name(create_info.db_type->getName());
  }


  /* If CREATE TABLE of non-temporary table, do implicit commit */
  if (! lex_identified_temp_table)
  {
    if (! session->endActiveTransaction())
    {
      return true;
    }
  }
  /* Skip first table, which is the table we are creating */
  TableList *create_table= session->lex->unlink_first_table(&link_to_local);
  TableList *select_tables= session->lex->query_tables;


  /*
    Now that we have the engine, we can figure out the table identifier. We need the engine in order
    to determine if the table is transactional or not if it is temp.
  */
  TableIdentifier new_table_identifier(create_table->db,
                                       create_table->table_name,
                                       create_table_proto.type() != message::Table::TEMPORARY ? NO_TMP_TABLE : TEMP_TABLE);

  if (create_table_precheck(new_table_identifier))
  {
    /* put tables back for PS rexecuting */
    session->lex->link_first_table_back(create_table, link_to_local);
    return true;
  }

  /* Might have been updated in create_table_precheck */
  create_info.alias= create_table->alias;

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
  if (! (need_start_waiting= ! wait_if_global_read_lock(session, 0, 1)))
  {
    /* put tables back for PS rexecuting */
    session->lex->link_first_table_back(create_table, link_to_local);
    return true;
  }

  if (select_lex->item_list.elements)		// With select
  {
    select_result *result;

    select_lex->options|= SELECT_NO_UNLOCK;
    unit->set_limit(select_lex);

    if (! lex_identified_temp_table)
    {
      session->lex->link_first_table_back(create_table, link_to_local);
      create_table->create= true;
    }

    if (! (res= session->openTablesLock(session->lex->query_tables)))
    {
      /*
         Is table which we are changing used somewhere in other parts
         of query
       */
      if (! lex_identified_temp_table)
      {
        TableList *duplicate= NULL;
        create_table= session->lex->unlink_first_table(&link_to_local);
        if ((duplicate= unique_table(create_table, select_tables)))
        {
          my_error(ER_UPDATE_TABLE_USED, MYF(0), create_table->alias);
          /*
             Release the protection against the global read lock and wake
             everyone, who might want to set a global read lock.
           */
          start_waiting_global_read_lock(session);
          /* put tables back for PS rexecuting */
          session->lex->link_first_table_back(create_table, link_to_local);
          return true;
        }
      }

      /*
         select_create is currently not re-execution friendly and
         needs to be created for every execution of a PS/SP.
       */
      if ((result= new select_create(create_table,
                                     is_if_not_exists,
                                     &create_info,
                                     &create_table_proto,
                                     &alter_info,
                                     select_lex->item_list,
                                     session->lex->duplicates,
                                     session->lex->ignore,
                                     select_tables)))
      {
        /*
           CREATE from SELECT give its Select_Lex for SELECT,
           and item_list belong to SELECT
         */
        res= handle_select(session, session->lex, result, 0);
        delete result;
      }
    }
    else if (! lex_identified_temp_table)
    {
      create_table= session->lex->unlink_first_table(&link_to_local);
    }
  }
  else
  {
    /* regular create */
    if (is_create_table_like)
    {
      res= mysql_create_like_table(session, 
                                   create_table, 
                                   select_tables,
                                   create_table_proto,
                                   create_info.db_type, 
                                   is_if_not_exists,
                                   is_engine_set);
    }
    else
    {

      for (int32_t x= 0; x < alter_info.alter_proto.added_field_size(); x++)
      {
        message::Table::Field *field= create_table_proto.add_field();

        *field= alter_info.alter_proto.added_field(x);
      }

      res= mysql_create_table(session, 
                              new_table_identifier,
                              &create_info,
                              &create_table_proto,
                              &alter_info, 
                              false, 
                              0,
                              is_if_not_exists);
    }
    if (! res)
    {
      session->my_ok();
    }
  }

  /*
     Release the protection against the global read lock and wake
     everyone, who might want to set a global read lock.
   */
  start_waiting_global_read_lock(session);

  return res;
}

} /* namespace drizzled */

