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

#include <fcntl.h>

#include <sstream>

#include <drizzled/show.h>
#include <drizzled/lock.h>
#include <drizzled/session.h>
#include <drizzled/statement/alter_table.h>
#include <drizzled/charset.h>
#include <drizzled/gettext.h>
#include <drizzled/data_home.h>
#include <drizzled/sql_table.h>
#include <drizzled/table_proto.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/time_functions.h>
#include <drizzled/records.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/copy_field.h>
#include <drizzled/transaction_services.h>
#include <drizzled/filesort.h>
#include <drizzled/message.h>
#include <drizzled/message/alter_table.pb.h>
#include <drizzled/alter_column.h>
#include <drizzled/alter_info.h>
#include <drizzled/util/test.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/table/cache.h>

using namespace std;

namespace drizzled {

extern pid_t current_pid;

static int copy_data_between_tables(Session *session,
                                    Table *from,Table *to,
                                    List<CreateField> &create,
                                    bool ignore,
                                    uint32_t order_num,
                                    Order *order,
                                    ha_rows *copied,
                                    ha_rows *deleted,
                                    message::AlterTable &alter_table_message,
                                    bool error_if_not_empty);

static bool prepare_alter_table(Session *session,
                                Table *table,
                                HA_CREATE_INFO *create_info,
                                const message::Table &original_proto,
                                message::Table &table_message,
                                message::AlterTable &alter_table_message,
                                AlterInfo *alter_info);

static Table *open_alter_table(Session *session, Table *table, identifier::Table &identifier);

static int apply_online_alter_keys_onoff(Session *session,
                                         Table* table,
                                         const message::AlterTable::AlterKeysOnOff &op);

static int apply_online_rename_table(Session *session,
                                     Table *table,
                                     plugin::StorageEngine *original_engine,
                                     identifier::Table &original_table_identifier,
                                     identifier::Table &new_table_identifier,
                                     const message::AlterTable::RenameTable &alter_operation);

namespace statement {

AlterTable::AlterTable(Session *in_session, Table_ident *) :
  CreateTable(in_session)
{
  set_command(SQLCOM_ALTER_TABLE);
}

} // namespace statement

bool statement::AlterTable::execute()
{
  TableList *first_table= (TableList *) lex().select_lex.table_list.first;
  TableList *all_tables= lex().query_tables;
  assert(first_table == all_tables && first_table != 0);
  Select_Lex *select_lex= &lex().select_lex;
  bool need_start_waiting= false;

  is_engine_set= not createTableMessage().engine().name().empty();

  if (is_engine_set)
  {
    create_info().db_type=
      plugin::StorageEngine::findByName(session(), createTableMessage().engine().name());

    if (create_info().db_type == NULL)
    {
      my_error(createTableMessage().engine().name(), ER_UNKNOWN_STORAGE_ENGINE, MYF(0));

      return true;
    }
  }

  /* Must be set in the parser */
  assert(select_lex->db);

  /* Chicken/Egg... we need to search for the table, to know if the table exists, so we can build a full identifier from it */
  message::table::shared_ptr original_table_message;
  {
    identifier::Table identifier(first_table->getSchemaName(), first_table->getTableName());
    if (not (original_table_message= plugin::StorageEngine::getTableMessage(session(), identifier)))
    {
      my_error(ER_BAD_TABLE_ERROR, identifier);
      return true;
    }

    if (not  create_info().db_type)
    {
      create_info().db_type=
        plugin::StorageEngine::findByName(session(), original_table_message->engine().name());

      if (not create_info().db_type)
      {
        my_error(ER_BAD_TABLE_ERROR, identifier);
        return true;
      }
    }
  }

  if (not validateCreateTableOption())
    return true;

  if (session().inTransaction())
  {
    my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
    return true;
  }

  if (not (need_start_waiting= not session().wait_if_global_read_lock(0, 1)))
    return true;

  bool res;
  if (original_table_message->type() == message::Table::STANDARD )
  {
    identifier::Table identifier(first_table->getSchemaName(), first_table->getTableName());
    identifier::Table new_identifier(select_lex->db ? select_lex->db : first_table->getSchemaName(),
                                   lex().name.str ? lex().name.str : first_table->getTableName());

    res= alter_table(&session(),
                     identifier,
                     new_identifier,
                     &create_info(),
                     *original_table_message,
                     createTableMessage(),
                     first_table,
                     &alter_info,
                     select_lex->order_list.size(),
                     (Order *) select_lex->order_list.first,
                     lex().ignore);
  }
  else
  {
    identifier::Table catch22(first_table->getSchemaName(), first_table->getTableName());
    Table *table= session().open_tables.find_temporary_table(catch22);
    assert(table);
    {
      identifier::Table identifier(first_table->getSchemaName(), first_table->getTableName(), table->getMutableShare()->getPath());
      identifier::Table new_identifier(select_lex->db ? select_lex->db : first_table->getSchemaName(),
                                       lex().name.str ? lex().name.str : first_table->getTableName(),
                                       table->getMutableShare()->getPath());

      res= alter_table(&session(),
                       identifier,
                       new_identifier,
                       &create_info(),
                       *original_table_message,
                       createTableMessage(),
                       first_table,
                       &alter_info,
                       select_lex->order_list.size(),
                       (Order *) select_lex->order_list.first,
                       lex().ignore);
    }
  }

  /*
     Release the protection against the global read lock and wake
     everyone, who might want to set a global read lock.
   */
  session().startWaitingGlobalReadLock();

  return res;
}


/**
  Prepare column and key definitions for CREATE TABLE in ALTER Table.

  This function transforms parse output of ALTER Table - lists of
  columns and keys to add, drop or modify into, essentially,
  CREATE TABLE definition - a list of columns and keys of the new
  table. While doing so, it also performs some (bug not all)
  semantic checks.

  This function is invoked when we know that we're going to
  perform ALTER Table via a temporary table -- i.e. fast ALTER Table
  is not possible, perhaps because the ALTER statement contains
  instructions that require change in table data, not only in
  table definition or indexes.

  @param[in,out]  session         thread handle. Used as a memory pool
                              and source of environment information.
  @param[in]      table       the source table, open and locked
                              Used as an interface to the storage engine
                              to acquire additional information about
                              the original table.
  @param[in,out]  create_info A blob with CREATE/ALTER Table
                              parameters
  @param[in,out]  alter_info  Another blob with ALTER/CREATE parameters.
                              Originally create_info was used only in
                              CREATE TABLE and alter_info only in ALTER Table.
                              But since ALTER might end-up doing CREATE,
                              this distinction is gone and we just carry
                              around two structures.

  @return
    Fills various create_info members based on information retrieved
    from the storage engine.
    Sets create_info->varchar if the table has a VARCHAR column.
    Prepares alter_info->create_list and alter_info->key_list with
    columns and keys of the new table.
  @retval true   error, out of memory or a semantical error in ALTER
                 Table instructions
  @retval false  success
*/
static bool prepare_alter_table(Session *session,
                                Table *table,
                                HA_CREATE_INFO *create_info,
                                const message::Table &original_proto,
                                message::Table &table_message,
                                message::AlterTable &alter_table_message,
                                AlterInfo *alter_info)
{
  uint32_t used_fields= create_info->used_fields;
  vector<string> drop_keys;
  vector<string> drop_columns;
  vector<string> drop_fkeys;

  /* we use drop_(keys|columns|fkey) below to check that we can do all
     operations we've been asked to do */
  for (int operationnr= 0; operationnr < alter_table_message.operations_size();
       operationnr++)
  {
    const message::AlterTable::AlterTableOperation &operation=
      alter_table_message.operations(operationnr);

    switch (operation.operation())
    {
    case message::AlterTable::AlterTableOperation::DROP_KEY:
      drop_keys.push_back(operation.drop_name());
      break;
    case message::AlterTable::AlterTableOperation::DROP_COLUMN:
      drop_columns.push_back(operation.drop_name());
      break;
    case message::AlterTable::AlterTableOperation::DROP_FOREIGN_KEY:
      drop_fkeys.push_back(operation.drop_name());
      break;
    default:
      break;
    }
  }

  /* Let new create options override the old ones */
  message::Table::TableOptions *table_options= table_message.mutable_options();

  if (not (used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
    create_info->default_table_charset= table->getShare()->table_charset;

  if (not (used_fields & HA_CREATE_USED_AUTO) && table->found_next_number_field)
  {
    /* Table has an autoincrement, copy value to new table */
    table->cursor->info(HA_STATUS_AUTO);
    create_info->auto_increment_value= table->cursor->stats.auto_increment_value;
    if (create_info->auto_increment_value != original_proto.options().auto_increment_value())
      table_options->set_has_user_set_auto_increment_value(false);
  }

  table->restoreRecordAsDefault(); /* Empty record for DEFAULT */

  List<CreateField> new_create_list;
  List<Key> new_key_list;
  /* First collect all fields from table which isn't in drop_list */
  Field *field;
  for (Field **f_ptr= table->getFields(); (field= *f_ptr); f_ptr++)
  {
    /* Check if field should be dropped */
    vector<string>::iterator it= drop_columns.begin();
    while ((it != drop_columns.end()))
    {
      if (! my_strcasecmp(system_charset_info, field->field_name, (*it).c_str()))
      {
        /* Reset auto_increment value if it was dropped */
        if (MTYP_TYPENR(field->unireg_check) == Field::NEXT_NUMBER &&
            not (used_fields & HA_CREATE_USED_AUTO))
        {
          create_info->auto_increment_value= 0;
          create_info->used_fields|= HA_CREATE_USED_AUTO;
        }
        break;
      }
      it++;
    }

    if (it != drop_columns.end())
    {
      drop_columns.erase(it);
      continue;
    }

    /* Mark that we will read the field */
    field->setReadSet();

    CreateField *def;
    /* Check if field is changed */
    List<CreateField>::iterator def_it= alter_info->create_list.begin();
    while ((def= def_it++))
    {
      if (def->change &&
          ! my_strcasecmp(system_charset_info, field->field_name, def->change))
	      break;
    }

    if (def)
    {
      /* Field is changed */
      def->field= field;
      if (! def->after)
      {
        new_create_list.push_back(def);
        def_it.remove();
      }
    }
    else
    {
      /*
        This field was not dropped and not changed, add it to the list
        for the new table.
      */
      def= new CreateField(field, field);
      new_create_list.push_back(def);
      AlterInfo::alter_list_t::iterator alter(alter_info->alter_list.begin());

      for (; alter != alter_info->alter_list.end(); alter++)
      {
        if (not my_strcasecmp(system_charset_info,field->field_name, alter->name))
          break;
      }

      if (alter != alter_info->alter_list.end())
      {
        def->setDefaultValue(alter->def, NULL);

        alter_info->alter_list.erase(alter);
      }
    }
  }

  CreateField *def;
  List<CreateField>::iterator def_it= alter_info->create_list.begin();
  while ((def= def_it++)) /* Add new columns */
  {
    if (def->change && ! def->field)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), def->change, table->getMutableShare()->getTableName());
      return true;
    }
    /*
      If we have been given a field which has no default value, and is not null then we need to bail.
    */
    if (not (~def->flags & (NO_DEFAULT_VALUE_FLAG | NOT_NULL_FLAG)) and not def->change)
    {
      alter_info->error_if_not_empty= true;
    }
    if (! def->after)
    {
      new_create_list.push_back(def);
    }
    else if (def->after == first_keyword)
    {
      new_create_list.push_front(def);
    }
    else
    {
      CreateField *find;
      List<CreateField>::iterator find_it= new_create_list.begin();

      while ((find= find_it++)) /* Add new columns */
      {
        if (not my_strcasecmp(system_charset_info,def->after, find->field_name))
          break;
      }

      if (not find)
      {
        my_error(ER_BAD_FIELD_ERROR, MYF(0), def->after, table->getMutableShare()->getTableName());
        return true;
      }

      find_it.after(def); /* Put element after this */

      /*
        XXX: hack for Bug#28427.
        If column order has changed, force OFFLINE ALTER Table
        without querying engine capabilities.  If we ever have an
        engine that supports online ALTER Table CHANGE COLUMN
        <name> AFTER <name1> (Falcon?), this fix will effectively
        disable the capability.
        TODO: detect the situation in compare_tables, behave based
        on engine capabilities.
      */
      if (alter_table_message.build_method() == message::AlterTable::BUILD_ONLINE)
      {
        my_error(*session->getQueryString(), ER_NOT_SUPPORTED_YET);
        return true;
      }
    }
  }

  if (not alter_info->alter_list.empty())
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0), alter_info->alter_list.front().name, table->getMutableShare()->getTableName());
    return true;
  }

  if (new_create_list.is_empty())
  {
    my_message(ER_CANT_REMOVE_ALL_FIELDS, ER(ER_CANT_REMOVE_ALL_FIELDS), MYF(0));
    return true;
  }

  /*
    Collect all keys which isn't in drop list. Add only those
    for which some fields exists.
  */
  KeyInfo *key_info= table->key_info;
  for (uint32_t i= 0; i < table->getShare()->sizeKeys(); i++, key_info++)
  {
    char *key_name= key_info->name;

    vector<string>::iterator it= drop_keys.begin();
    while ((it != drop_keys.end()))
    {
      if (! my_strcasecmp(system_charset_info, key_name, (*it).c_str()))
        break;
      it++;
    }

    if (it != drop_keys.end())
    {
      drop_keys.erase(it);
      continue;
    }

    KeyPartInfo *key_part= key_info->key_part;
    List<Key_part_spec> key_parts;
    for (uint32_t j= 0; j < key_info->key_parts; j++, key_part++)
    {
      if (! key_part->field)
	      continue;	/* Wrong field (from UNIREG) */

      const char *key_part_name= key_part->field->field_name;
      CreateField *cfield;
      List<CreateField>::iterator field_it= new_create_list.begin();
      while ((cfield= field_it++))
      {
        if (cfield->change)
        {
          if (not my_strcasecmp(system_charset_info, key_part_name, cfield->change))
            break;
        }
        else if (not my_strcasecmp(system_charset_info, key_part_name, cfield->field_name))
          break;
      }

      if (not cfield)
	      continue; /* Field is removed */

      uint32_t key_part_length= key_part->length;
      if (cfield->field) /* Not new field */
      {
        /*
          If the field can't have only a part used in a key according to its
          new type, or should not be used partially according to its
          previous type, or the field length is less than the key part
          length, unset the key part length.

          We also unset the key part length if it is the same as the
          old field's length, so the whole new field will be used.

          BLOBs may have cfield->length == 0, which is why we test it before
          checking whether cfield->length < key_part_length (in chars).
         */
        if (! Field::type_can_have_key_part(cfield->field->type()) ||
            ! Field::type_can_have_key_part(cfield->sql_type) ||
            (cfield->field->field_length == key_part_length) ||
            (cfield->length &&
             (cfield->length < key_part_length / key_part->field->charset()->mbmaxlen)))
          key_part_length= 0; /* Use whole field */
      }
      key_part_length/= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(new Key_part_spec(cfield->field_name,
                                            strlen(cfield->field_name),
                                            key_part_length));
    }
    if (key_parts.size())
    {
      key_create_information_st key_create_info= default_key_create_info;
      Key *key;
      Key::Keytype key_type;

      key_create_info.algorithm= key_info->algorithm;

      if (key_info->flags & HA_USES_BLOCK_SIZE)
        key_create_info.block_size= key_info->block_size;

      if (key_info->flags & HA_USES_COMMENT)
        key_create_info.comment= key_info->comment;

      if (key_info->flags & HA_NOSAME)
      {
        if (is_primary_key_name(key_name))
          key_type= Key::PRIMARY;
        else
          key_type= Key::UNIQUE;
      }
      else
      {
        key_type= Key::MULTIPLE;
      }

      key= new Key(key_type,
                   key_name,
                   strlen(key_name),
                   &key_create_info,
                   test(key_info->flags & HA_GENERATED_KEY),
                   key_parts);
      new_key_list.push_back(key);
    }
  }

  /* Copy over existing foreign keys */
  for (int32_t j= 0; j < original_proto.fk_constraint_size(); j++)
  {
    vector<string>::iterator it= drop_fkeys.begin();
    while ((it != drop_fkeys.end()))
    {
      if (! my_strcasecmp(system_charset_info, original_proto.fk_constraint(j).name().c_str(), (*it).c_str()))
      {
        break;
      }
      it++;
    }

    if (it != drop_fkeys.end())
    {
      drop_fkeys.erase(it);
      continue;
    }

    message::Table::ForeignKeyConstraint *pfkey= table_message.add_fk_constraint();
    *pfkey= original_proto.fk_constraint(j);
  }

  {
    Key *key;
    List<Key>::iterator key_it(alter_info->key_list.begin());
    while ((key= key_it++)) /* Add new keys */
    {
      if (key->type == Key::FOREIGN_KEY)
      {
        if (((Foreign_key *)key)->validate(new_create_list))
        {
          return true;
        }

        Foreign_key *fkey= (Foreign_key*)key;
        add_foreign_key_to_table_message(&table_message,
                                         fkey->name.str,
                                         fkey->columns,
                                         fkey->ref_table,
                                         fkey->ref_columns,
                                         fkey->delete_opt,
                                         fkey->update_opt,
                                         fkey->match_opt);
      }

      if (key->type != Key::FOREIGN_KEY)
        new_key_list.push_back(key);

      if (key->name.str && is_primary_key_name(key->name.str))
      {
        my_error(ER_WRONG_NAME_FOR_INDEX,
                 MYF(0),
                 key->name.str);
        return true;
      }
    }
  }

  /* Fix names of foreign keys being added */
  for (int j= 0; j < table_message.fk_constraint_size(); j++)
  {
    if (! table_message.fk_constraint(j).has_name())
    {
      std::string name(table->getMutableShare()->getTableName());
      char number[20];

      name.append("_ibfk_");
      snprintf(number, sizeof(number), "%d", j+1);
      name.append(number);

      message::Table::ForeignKeyConstraint *pfkey= table_message.mutable_fk_constraint(j);
      pfkey->set_name(name);
    }
  }

  if (drop_keys.size())
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,
             MYF(0),
             drop_keys.front().c_str());
    return true;
  }

  if (drop_columns.size())
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,
             MYF(0),
             drop_columns.front().c_str());
    return true;
  }

  if (drop_fkeys.size())
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,
             MYF(0),
             drop_fkeys.front().c_str());
    return true;
  }

  if (not alter_info->alter_list.empty())
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,
             MYF(0),
             alter_info->alter_list.front().name);
    return true;
  }

  if (not table_message.options().has_comment()
      && table->getMutableShare()->hasComment())
  {
    table_options->set_comment(table->getMutableShare()->getComment());
  }

  if (table->getShare()->getType())
  {
    table_message.set_type(message::Table::TEMPORARY);
  }

  table_message.set_creation_timestamp(table->getShare()->getTableMessage()->creation_timestamp());
  table_message.set_version(table->getShare()->getTableMessage()->version());
  table_message.set_uuid(table->getShare()->getTableMessage()->uuid());

  alter_info->create_list.swap(new_create_list);
  alter_info->key_list.swap(new_key_list);

  size_t num_engine_options= table_message.engine().options_size();
  size_t original_num_engine_options= original_proto.engine().options_size();
  for (size_t x= 0; x < original_num_engine_options; ++x)
  {
    bool found= false;

    for (size_t y= 0; y < num_engine_options; ++y)
    {
      found= not table_message.engine().options(y).name().compare(original_proto.engine().options(x).name());

      if (found)
        break;
    }

    if (not found)
    {
      message::Engine::Option *opt= table_message.mutable_engine()->add_options();

      opt->set_name(original_proto.engine().options(x).name());
      opt->set_state(original_proto.engine().options(x).state());
    }
  }

  drizzled::message::update(table_message);

  return false;
}

/* table_list should contain just one table */
static int discard_or_import_tablespace(Session *session,
                                        TableList *table_list,
                                        bool discard)
{
  /*
    Note that DISCARD/IMPORT TABLESPACE always is the only operation in an
    ALTER Table
  */
  session->set_proc_info("discard_or_import_tablespace");

 /*
   We set this flag so that ha_innobase::open and ::external_lock() do
   not complain when we lock the table
 */
  session->setDoingTablespaceOperation(true);
  Table* table= session->openTableLock(table_list, TL_WRITE);
  if (not table)
  {
    session->setDoingTablespaceOperation(false);
    return -1;
  }

  int error;
  do {
    error= table->cursor->ha_discard_or_import_tablespace(discard);

    session->set_proc_info("end");

    if (error)
      break;

    /* The ALTER Table is always in its own transaction */
    error= TransactionServices::autocommitOrRollback(*session, false);
    if (not session->endActiveTransaction())
      error= 1;

    if (error)
      break;

    TransactionServices::rawStatement(*session,
                                      *session->getQueryString(),
                                      *session->schema());

  } while(0);

  (void) TransactionServices::autocommitOrRollback(*session, error);
  session->setDoingTablespaceOperation(false);

  if (error == 0)
  {
    session->my_ok();
    return 0;
  }

  table->print_error(error, MYF(0));

  return -1;
}

/**
  Manages enabling/disabling of indexes for ALTER Table

  SYNOPSIS
    alter_table_manage_keys()
      table                  Target table
      indexes_were_disabled  Whether the indexes of the from table
                             were disabled
      keys_onoff             ENABLE | DISABLE | LEAVE_AS_IS

  RETURN VALUES
    false  OK
    true   Error
*/
static bool alter_table_manage_keys(Session *session,
                                    Table *table, int indexes_were_disabled,
                                    const message::AlterTable &alter_table_message)
{
  int error= 0;
  if (alter_table_message.has_alter_keys_onoff()
      && alter_table_message.alter_keys_onoff().enable())
  {
    error= table->cursor->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
  }

  if ((! alter_table_message.has_alter_keys_onoff() && indexes_were_disabled)
      || (alter_table_message.has_alter_keys_onoff()
          && ! alter_table_message.alter_keys_onoff().enable()))
  {
    error= table->cursor->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
  }

  if (error == HA_ERR_WRONG_COMMAND)
  {
    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                        ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                        table->getMutableShare()->getTableName());
    error= 0;
  }
  else if (error)
  {
    table->print_error(error, MYF(0));
  }

  return(error);
}

static bool lockTableIfDifferent(Session &session,
                                 identifier::Table &original_table_identifier,
                                 identifier::Table &new_table_identifier,
                                 Table *name_lock)
{
  /* Check that we are not trying to rename to an existing table */
  if (not (original_table_identifier == new_table_identifier))
  {
    if (original_table_identifier.isTmp())
    {
      if (session.open_tables.find_temporary_table(new_table_identifier))
      {
        my_error(ER_TABLE_EXISTS_ERROR, new_table_identifier);
        return false;
      }
    }
    else
    {
      name_lock= session.lock_table_name_if_not_cached(new_table_identifier);
      if (not name_lock)
      {
        my_error(ER_TABLE_EXISTS_ERROR, new_table_identifier);
        return false;
      }

      if (plugin::StorageEngine::doesTableExist(session, new_table_identifier))
      {
        /* Table will be closed by Session::executeCommand() */
        my_error(ER_TABLE_EXISTS_ERROR, new_table_identifier);

        {
          boost::mutex::scoped_lock scopedLock(table::Cache::mutex());
          session.unlink_open_table(name_lock);
        }

        return false;
      }
    }
  }

  return true;
}

/**
  Alter table

  SYNOPSIS
    alter_table()
      session              Thread handle
      new_db           If there is a RENAME clause
      new_name         If there is a RENAME clause
      create_info      Information from the parsing phase about new
                       table properties.
      table_list       The table to change.
      alter_info       Lists of fields, keys to be changed, added
                       or dropped.
      order_num        How many ORDER BY fields has been specified.
      order            List of fields to order_st BY.
      ignore           Whether we have ALTER IGNORE Table

  DESCRIPTION
    This is a veery long function and is everything but the kitchen sink :)
    It is used to alter a table and not only by ALTER Table but also
    CREATE|DROP INDEX are mapped on this function.

    When the ALTER Table statement just does a RENAME or ENABLE|DISABLE KEYS,
    or both, then this function short cuts its operation by renaming
    the table and/or enabling/disabling the keys. In this case, the FRM is
    not changed, directly by alter_table. However, if there is a
    RENAME + change of a field, or an index, the short cut is not used.
    See how `create_list` is used to generate the new FRM regarding the
    structure of the fields. The same is done for the indices of the table.

    Important is the fact, that this function tries to do as little work as
    possible, by finding out whether a intermediate table is needed to copy
    data into and when finishing the altering to use it as the original table.
    For this reason the function compare_tables() is called, which decides
    based on all kind of data how similar are the new and the original
    tables.

  RETURN VALUES
    false  OK
    true   Error
*/

static bool internal_alter_table(Session *session,
                                 Table *table,
                                 identifier::Table &original_table_identifier,
                                 identifier::Table &new_table_identifier,
                                 HA_CREATE_INFO *create_info,
                                 const message::Table &original_proto,
                                 message::Table &create_proto,
                                 message::AlterTable &alter_table_message,
                                 TableList *table_list,
                                 AlterInfo *alter_info,
                                 uint32_t order_num,
                                 Order *order,
                                 bool ignore)
{
  int error= 0;
  char tmp_name[80];
  char old_name[32];
  ha_rows copied= 0;
  ha_rows deleted= 0;

  if (not original_table_identifier.isValid())
    return true;

  if (not new_table_identifier.isValid())
    return true;

  session->set_proc_info("init");

  table->use_all_columns();

  plugin::StorageEngine *new_engine;
  plugin::StorageEngine *original_engine;

  original_engine= table->getMutableShare()->getEngine();

  if (not create_info->db_type)
  {
    create_info->db_type= original_engine;
  }
  new_engine= create_info->db_type;


  create_proto.set_schema(new_table_identifier.getSchemaName());
  create_proto.set_type(new_table_identifier.getType());

  /**
    @todo Have a check on the table definition for FK in the future
    to remove the need for the cursor. (aka can_switch_engines())
  */
  if (new_engine != original_engine &&
      not table->cursor->can_switch_engines())
  {
    assert(0);
    my_error(ER_ROW_IS_REFERENCED, MYF(0));

    return true;
  }

  if (original_engine->check_flag(HTON_BIT_ALTER_NOT_SUPPORTED) ||
      new_engine->check_flag(HTON_BIT_ALTER_NOT_SUPPORTED))
  {
    my_error(ER_ILLEGAL_HA, new_table_identifier);

    return true;
  }

  session->set_proc_info("setup");

  /* First we try for operations that do not require a copying
     ALTER TABLE.

     In future there should be more operations, currently it's just a couple.
  */

  if ((alter_table_message.has_rename()
       || alter_table_message.has_alter_keys_onoff())
      && alter_table_message.operations_size() == 0)
  {
    /*
     * test if no other bits except ALTER_RENAME and ALTER_KEYS_ONOFF are set
     */
    bitset<32> tmp;

    tmp.set();
    tmp.reset(ALTER_RENAME);
    tmp.reset(ALTER_KEYS_ONOFF);
    tmp&= alter_info->flags;

    if((not (tmp.any()) && not table->getShare()->getType())) // no need to touch frm
    {
      if (alter_table_message.has_alter_keys_onoff())
      {
        error= apply_online_alter_keys_onoff(session, table,
                                       alter_table_message.alter_keys_onoff());

        if (error == HA_ERR_WRONG_COMMAND)
        {
          error= EE_OK;
          push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                              ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                              table->getAlias());
        }
      }

      if (alter_table_message.has_rename())
      {
        error= apply_online_rename_table(session,
                                         table,
                                         original_engine,
                                         original_table_identifier,
                                         new_table_identifier,
                                         alter_table_message.rename());

        if (error == HA_ERR_WRONG_COMMAND)
        {
          error= EE_OK;
          push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                              ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                              table->getAlias());
        }
      }

      if (not error)
      {
        TransactionServices::allocateNewTransactionId();
        TransactionServices::rawStatement(*session,
                                          *session->getQueryString(),
                                          *session->schema());
        session->my_ok();
      }
      else if (error > EE_OK) // If we have already set the error, we pass along -1
      {
        table->print_error(error, MYF(0));
      }

      table_list->table= NULL;

      return error;
    }
  }

  if (alter_table_message.build_method() == message::AlterTable::BUILD_ONLINE)
  {
    my_error(*session->getQueryString(), ER_NOT_SUPPORTED_YET);
    return true;
  }

  /* We have to do full alter table. */
  new_engine= create_info->db_type;

  if (prepare_alter_table(session, table, create_info, original_proto, create_proto, alter_table_message, alter_info))
  {
    return true;
  }

  set_table_default_charset(create_info, new_table_identifier.getSchemaName().c_str());

  snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%"PRIx64, TMP_FILE_PREFIX, (unsigned long) current_pid, session->thread_id);

  /* Create a temporary table with the new format */
  /**
    @note we make an internal temporary table unless the table is a temporary table. In last
    case we just use it as is. Neither of these tables require locks in order to  be
    filled.
  */
  identifier::Table new_table_as_temporary(original_table_identifier.getSchemaName(),
                                         tmp_name,
                                         create_proto.type() != message::Table::TEMPORARY ? message::Table::INTERNAL :
                                         message::Table::TEMPORARY);

  /*
    Create a table with a temporary name.
    We don't log the statement, it will be logged later.
  */
  create_proto.set_name(new_table_as_temporary.getTableName());
  create_proto.mutable_engine()->set_name(create_info->db_type->getName());

  error= create_table(session,
                      new_table_as_temporary,
                      create_info, create_proto, alter_info, true, 0, false);

  if (error != 0)
  {
    return true;
  }

  /* Open the table so we need to copy the data to it. */
  Table *new_table= open_alter_table(session, table, new_table_as_temporary);


  if (not new_table)
  {
    plugin::StorageEngine::dropTable(*session, new_table_as_temporary);
    return true;
  }

  /* Copy the data if necessary. */
  {
    /* We must not ignore bad input! */
    session->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;	// calc cuted fields
    session->cuted_fields= 0L;
    session->set_proc_info("copy to tmp table");
    copied= deleted= 0;

    /* We don't want update TIMESTAMP fields during ALTER Table. */
    new_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    new_table->next_number_field= new_table->found_next_number_field;
    error= copy_data_between_tables(session,
                                    table,
                                    new_table,
                                    alter_info->create_list,
                                    ignore,
                                    order_num,
                                    order,
                                    &copied,
                                    &deleted,
                                    alter_table_message,
                                    alter_info->error_if_not_empty);

    /* We must not ignore bad input! */
    assert(session->count_cuted_fields == CHECK_FIELD_ERROR_FOR_NULL);
  }

  /* Now we need to resolve what just happened with the data copy. */

  if (error)
  {

    /*
      No default value was provided for new fields.
    */
    if (alter_info->error_if_not_empty && session->row_count)
    {
      my_error(ER_INVALID_ALTER_TABLE_FOR_NOT_NULL, MYF(0));
    }

    if (original_table_identifier.isTmp())
    {
      if (new_table)
      {
        /* close_temporary_table() frees the new_table pointer. */
        session->open_tables.close_temporary_table(new_table);
      }
      else
      {
        plugin::StorageEngine::dropTable(*session, new_table_as_temporary);
      }

      return true;
    }
    else
    {
      if (new_table)
      {
        /*
          Close the intermediate table that will be the new table.
          Note that MERGE tables do not have their children attached here.
        */
        new_table->intern_close_table();
        if (new_table->hasShare())
        {
          delete new_table->getMutableShare();
        }

        delete new_table;
      }

      boost::mutex::scoped_lock scopedLock(table::Cache::mutex());

      plugin::StorageEngine::dropTable(*session, new_table_as_temporary);

      return true;
    }
  }
  // Temporary table and success
  else if (original_table_identifier.isTmp())
  {
    /* Close lock if this is a transactional table */
    if (session->open_tables.lock)
    {
      session->unlockTables(session->open_tables.lock);
      session->open_tables.lock= 0;
    }

    /* Remove link to old table and rename the new one */
    session->open_tables.close_temporary_table(table);

    /* Should pass the 'new_name' as we store table name in the cache */
    new_table->getMutableShare()->setIdentifier(new_table_identifier);

    new_table_identifier.setPath(new_table_as_temporary.getPath());

    if (rename_table(*session, new_engine, new_table_as_temporary, new_table_identifier) != 0)
    {
      return true;
    }
  }
  // Normal table success
  else
  {
    if (new_table)
    {
      /*
        Close the intermediate table that will be the new table.
        Note that MERGE tables do not have their children attached here.
      */
      new_table->intern_close_table();

      if (new_table->hasShare())
      {
        delete new_table->getMutableShare();
      }

      delete new_table;
    }

    {
      boost::mutex::scoped_lock scopedLock(table::Cache::mutex()); /* ALTER TABLE */
      /*
        Data is copied. Now we:
        1) Wait until all other threads close old version of table.
        2) Close instances of table open by this thread and replace them
        with exclusive name-locks.
        3) Rename the old table to a temp name, rename the new one to the
        old name.
        4) If we are under LOCK TABLES and don't do ALTER Table ... RENAME
        we reopen new version of table.
        5) Write statement to the binary log.
        6) If we are under LOCK TABLES and do ALTER Table ... RENAME we
        remove name-locks from list of open tables and table cache.
        7) If we are not not under LOCK TABLES we rely on close_thread_tables()
        call to remove name-locks from table cache and list of open table.
      */

      session->set_proc_info("rename result table");

      snprintf(old_name, sizeof(old_name), "%s2-%lx-%"PRIx64, TMP_FILE_PREFIX, (unsigned long) current_pid, session->thread_id);

      my_casedn_str(files_charset_info, old_name);

      wait_while_table_is_used(session, table, HA_EXTRA_PREPARE_FOR_RENAME);
      session->close_data_files_and_morph_locks(original_table_identifier);

      assert(not error);

      /*
        This leads to the storage engine (SE) not being notified for renames in
        rename_table(), because we just juggle with the FRM and nothing
        more. If we have an intermediate table, then we notify the SE that
        it should become the actual table. Later, we will recycle the old table.
        However, in case of ALTER Table RENAME there might be no intermediate
        table. This is when the old and new tables are compatible, according to
        compare_table(). Then, we need one additional call to
      */
      identifier::Table original_table_to_drop(original_table_identifier.getSchemaName(),
                                             old_name, create_proto.type() != message::Table::TEMPORARY ? message::Table::INTERNAL :
                                             message::Table::TEMPORARY);

      drizzled::error_t rename_error= EE_OK;
      if (rename_table(*session, original_engine, original_table_identifier, original_table_to_drop))
      {
        error= ER_ERROR_ON_RENAME;
        plugin::StorageEngine::dropTable(*session, new_table_as_temporary);
      }
      else
      {
        if (rename_table(*session, new_engine, new_table_as_temporary, new_table_identifier) != 0)
        {
          /* Try to get everything back. */
          rename_error= ER_ERROR_ON_RENAME;

          plugin::StorageEngine::dropTable(*session, new_table_identifier);

          plugin::StorageEngine::dropTable(*session, new_table_as_temporary);

          rename_table(*session, original_engine, original_table_to_drop, original_table_identifier);
        }
        else
        {
          plugin::StorageEngine::dropTable(*session, original_table_to_drop);
        }
      }

      if (rename_error)
      {
        /*
          An error happened while we were holding exclusive name-lock on table
          being altered. To be safe under LOCK TABLES we should remove placeholders
          from list of open tables list and table cache.
        */
        session->unlink_open_table(table);

        return true;
      }
    }

    session->set_proc_info("end");

    TransactionServices::rawStatement(*session,
                                      *session->getQueryString(),
                                      *session->schema());
    table_list->table= NULL;
  }

  /*
   * Field::store() may have called my_error().  If this is
   * the case, we must not send an ok packet, since
   * Diagnostics_area::is_set() will fail an assert.
 */
  if (session->is_error())
  {
    /* my_error() was called.  Return true (which means error...) */
    return true;
  }

  snprintf(tmp_name, sizeof(tmp_name), ER(ER_INSERT_INFO),
           (ulong) (copied + deleted), (ulong) deleted,
           (ulong) session->cuted_fields);
  session->my_ok(copied + deleted, 0, 0L, tmp_name);
  return false;
}

static int apply_online_alter_keys_onoff(Session *session,
                                         Table* table,
                                         const message::AlterTable::AlterKeysOnOff &op)
{
  int error= 0;

  if (op.enable())
  {
    /*
      wait_while_table_is_used() ensures that table being altered is
      opened only by this thread and that Table::TableShare::version
      of Table object corresponding to this table is 0.
      The latter guarantees that no DML statement will open this table
      until ALTER Table finishes (i.e. until close_thread_tables())
      while the fact that the table is still open gives us protection
      from concurrent DDL statements.
    */
    {
      boost::mutex::scoped_lock scopedLock(table::Cache::mutex()); /* DDL wait for/blocker */
      wait_while_table_is_used(session, table, HA_EXTRA_FORCE_REOPEN);
    }
    error= table->cursor->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);

    /* COND_refresh will be signaled in close_thread_tables() */
  }
  else
  {
    {
      boost::mutex::scoped_lock scopedLock(table::Cache::mutex()); /* DDL wait for/blocker */
      wait_while_table_is_used(session, table, HA_EXTRA_FORCE_REOPEN);
    }
    error= table->cursor->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);

    /* COND_refresh will be signaled in close_thread_tables() */
  }

  return error;
}

static int apply_online_rename_table(Session *session,
                                     Table *table,
                                     plugin::StorageEngine *original_engine,
                                     identifier::Table &original_table_identifier,
                                     identifier::Table &new_table_identifier,
                                     const message::AlterTable::RenameTable &alter_operation)
{
  int error= 0;

  boost::mutex::scoped_lock scopedLock(table::Cache::mutex()); /* Lock to remove all instances of table from table cache before ALTER */
  /*
    Unlike to the above case close_cached_table() below will remove ALL
    instances of Table from table cache (it will also remove table lock
    held by this thread). So to make actual table renaming and writing
    to binlog atomic we have to put them into the same critical section
    protected by table::Cache::mutex() mutex. This also removes gap for races between
    access() and rename_table() calls.
  */

  if (not (original_table_identifier == new_table_identifier))
  {
    assert(alter_operation.to_name() == new_table_identifier.getTableName());
    assert(alter_operation.to_schema() == new_table_identifier.getSchemaName());

    session->set_proc_info("rename");
    /*
      Then do a 'simple' rename of the table. First we need to close all
      instances of 'source' table.
    */
    session->close_cached_table(table);
    /*
      Then, we want check once again that target table does not exist.
      Actually the order of these two steps does not matter since
      earlier we took name-lock on the target table, so we do them
      in this particular order only to be consistent with 5.0, in which
      we don't take this name-lock and where this order really matters.
      @todo Investigate if we need this access() check at all.
    */
    if (plugin::StorageEngine::doesTableExist(*session, new_table_identifier))
    {
      my_error(ER_TABLE_EXISTS_ERROR, new_table_identifier);
      error= -1;
    }
    else
    {
      if (rename_table(*session, original_engine, original_table_identifier, new_table_identifier))
      {
        error= -1;
      }
    }
  }
  return error;
}

bool alter_table(Session *session,
                 identifier::Table &original_table_identifier,
                 identifier::Table &new_table_identifier,
                 HA_CREATE_INFO *create_info,
                 const message::Table &original_proto,
                 message::Table &create_proto,
                 TableList *table_list,
                 AlterInfo *alter_info,
                 uint32_t order_num,
                 Order *order,
                 bool ignore)
{
  bool error;
  Table *table;
  message::AlterTable *alter_table_message= session->lex().alter_table();

  alter_table_message->set_catalog(original_table_identifier.getCatalogName());
  alter_table_message->set_schema(original_table_identifier.getSchemaName());
  alter_table_message->set_name(original_table_identifier.getTableName());

  if (alter_table_message->operations_size()
      && (alter_table_message->operations(0).operation()
          == message::AlterTable::AlterTableOperation::DISCARD_TABLESPACE
          || alter_table_message->operations(0).operation()
          == message::AlterTable::AlterTableOperation::IMPORT_TABLESPACE))
  {
    bool discard= (alter_table_message->operations(0).operation() ==
                   message::AlterTable::AlterTableOperation::DISCARD_TABLESPACE);
    /* DISCARD/IMPORT TABLESPACE is always alone in an ALTER Table */
    return discard_or_import_tablespace(session, table_list, discard);
  }

  session->set_proc_info("init");

  if (not (table= session->openTableLock(table_list, TL_WRITE_ALLOW_READ)))
    return true;

  session->set_proc_info("gained write lock on table");

  /*
    Check that we are not trying to rename to an existing table,
    if one existed we get a lock, if we can't we error.
  */
  {
    Table *name_lock= NULL;

    if (not lockTableIfDifferent(*session, original_table_identifier, new_table_identifier, name_lock))
    {
      return true;
    }

    error= internal_alter_table(session,
                                table,
                                original_table_identifier,
                                new_table_identifier,
                                create_info,
                                original_proto,
                                create_proto,
                                *alter_table_message,
                                table_list,
                                alter_info,
                                order_num,
                                order,
                                ignore);

    if (name_lock)
    {
      boost::mutex::scoped_lock scopedLock(table::Cache::mutex());
      session->unlink_open_table(name_lock);
    }
  }

  return error;
}
/* alter_table */

static int
copy_data_between_tables(Session *session,
                         Table *from, Table *to,
                         List<CreateField> &create,
                         bool ignore,
                         uint32_t order_num, Order *order,
                         ha_rows *copied,
                         ha_rows *deleted,
                         message::AlterTable &alter_table_message,
                         bool error_if_not_empty)
{
  int error= 0;
  CopyField *copy,*copy_end;
  ulong found_count,delete_count;
  uint32_t length= 0;
  SortField *sortorder;
  ReadRecord info;
  TableList   tables;
  List<Item>   fields;
  List<Item>   all_fields;
  ha_rows examined_rows;
  bool auto_increment_field_copied= 0;
  uint64_t prev_insert_id;

  /*
    Turn off recovery logging since rollback of an alter table is to
    delete the new table so there is no need to log the changes to it.

    This needs to be done before external_lock
  */

  /*
   * LP Bug #552420
   *
   * Since open_temporary_table() doesn't invoke lockTables(), we
   * don't get the usual automatic call to StorageEngine::startStatement(), so
   * we manually call it here...
   */
  to->getMutableShare()->getEngine()->startStatement(session);

  copy= new CopyField[to->getShare()->sizeFields()];

  if (to->cursor->ha_external_lock(session, F_WRLCK))
    return -1;

  /* We need external lock before we can disable/enable keys */
  alter_table_manage_keys(session, to, from->cursor->indexes_are_disabled(),
                          alter_table_message);

  /* We can abort alter table for any table type */
  session->setAbortOnWarning(not ignore);

  from->cursor->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  to->cursor->ha_start_bulk_insert(from->cursor->stats.records);

  List<CreateField>::iterator it(create.begin());
  copy_end= copy;
  for (Field **ptr= to->getFields(); *ptr ; ptr++)
  {
    CreateField* def=it++;
    if (def->field)
    {
      if (*ptr == to->next_number_field)
        auto_increment_field_copied= true;

      (copy_end++)->set(*ptr,def->field,0);
    }

  }

  found_count=delete_count=0;

  do
  {
    if (order)
    {
      if (to->getShare()->hasPrimaryKey() && to->cursor->primary_key_is_clustered())
      {
        char warn_buff[DRIZZLE_ERRMSG_SIZE];
        snprintf(warn_buff, sizeof(warn_buff),
                 _("order_st BY ignored because there is a user-defined clustered "
                   "index in the table '%-.192s'"),
                 from->getMutableShare()->getTableName());
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                     warn_buff);
      }
      else
      {
        FileSort filesort(*session);
        from->sort.io_cache= new internal::io_cache_st;

        tables.table= from;
        tables.setTableName(from->getMutableShare()->getTableName());
        tables.alias= tables.getTableName();
        tables.setSchemaName(const_cast<char *>(from->getMutableShare()->getSchemaName()));
        error= 1;

        session->lex().select_lex.setup_ref_array(session, order_num);
        if (setup_order(session, session->lex().select_lex.ref_pointer_array, &tables, fields, all_fields, order))
          break;
        sortorder= make_unireg_sortorder(order, &length, NULL);
        if ((from->sort.found_records= filesort.run(from, sortorder, length, (optimizer::SqlSelect *) 0, HA_POS_ERROR, 1, examined_rows)) == HA_POS_ERROR)
          break;
      }
    }

    /* Tell handler that we have values for all columns in the to table */
    to->use_all_columns();

    error= info.init_read_record(session, from, (optimizer::SqlSelect *) 0, 1, true);
    if (error)
    {
      to->print_error(errno, MYF(0));

      break;
    }

    if (ignore)
    {
      to->cursor->extra(HA_EXTRA_IGNORE_DUP_KEY);
    }

    session->row_count= 0;
    to->restoreRecordAsDefault();        // Create empty record
    while (not (error=info.read_record(&info)))
    {
      if (session->getKilled())
      {
        session->send_kill_message();
        error= 1;
        break;
      }
      session->row_count++;
      /* Return error if source table isn't empty. */
      if (error_if_not_empty)
      {
        error= 1;
        break;
      }
      if (to->next_number_field)
      {
        if (auto_increment_field_copied)
          to->auto_increment_field_not_null= true;
        else
          to->next_number_field->reset();
      }

      for (CopyField *copy_ptr= copy; copy_ptr != copy_end ; copy_ptr++)
      {
        if (not copy->to_field->hasDefault() and copy->from_null_ptr and  *copy->from_null_ptr & copy->from_bit)
        {
          copy->to_field->set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                      ER_WARN_DATA_TRUNCATED, 1);
          copy->to_field->reset();
          error= 1;
          break;
        }

        copy_ptr->do_copy(copy_ptr);
      }

      if (error)
      {
        break;
      }

      prev_insert_id= to->cursor->next_insert_id;
      error= to->cursor->insertRecord(to->record[0]);
      to->auto_increment_field_not_null= false;

      if (error)
      {
        if (!ignore || to->cursor->is_fatal_error(error, HA_CHECK_DUP))
        {
          to->print_error(error, MYF(0));
          break;
        }
        to->cursor->restore_auto_increment(prev_insert_id);
        delete_count++;
      }
      else
      {
        found_count++;
      }
    }

    info.end_read_record();
    from->free_io_cache();
    delete [] copy;				// This is never 0

    if (to->cursor->ha_end_bulk_insert() && error <= 0)
    {
      to->print_error(errno, MYF(0));
      error= 1;
    }
    to->cursor->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

    /*
      Ensure that the new table is saved properly to disk so that we
      can do a rename
    */
    if (TransactionServices::autocommitOrRollback(*session, false))
      error= 1;

    if (not session->endActiveTransaction())
      error= 1;

  } while (0);

  session->setAbortOnWarning(false);
  from->free_io_cache();
  *copied= found_count;
  *deleted=delete_count;
  to->cursor->ha_release_auto_increment();

  if (to->cursor->ha_external_lock(session, F_UNLCK))
  {
    error=1;
  }

  return(error > 0 ? -1 : 0);
}

static Table *open_alter_table(Session *session, Table *table, identifier::Table &identifier)
{
  /* Open the table so we need to copy the data to it. */
  if (table->getShare()->getType())
  {
    TableList tbl;
    tbl.setSchemaName(const_cast<char *>(identifier.getSchemaName().c_str()));
    tbl.alias= const_cast<char *>(identifier.getTableName().c_str());
    tbl.setTableName(const_cast<char *>(identifier.getTableName().c_str()));

    /* Table is in session->temporary_tables */
    return session->openTable(&tbl, (bool*) 0, DRIZZLE_LOCK_IGNORE_FLUSH);
  }
  else
  {
    /* Open our intermediate table */
    return session->open_temporary_table(identifier, false);
  }
}

} /* namespace drizzled */
