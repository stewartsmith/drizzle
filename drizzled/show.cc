/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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


/* Function with list databases, tables or fields */
#include "config.h"
#include <drizzled/sql_select.h>
#include <drizzled/show.h>
#include <drizzled/gettext.h>
#include <drizzled/util/convert.h>
#include <drizzled/error.h>
#include <drizzled/tztime.h>
#include <drizzled/data_home.h>
#include <drizzled/item/blob.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/return_int.h>
#include <drizzled/item/empty_string.h>
#include <drizzled/item/return_date_time.h>
#include <drizzled/sql_base.h>
#include <drizzled/db.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/field/decimal.h>
#include <drizzled/lock.h>
#include <drizzled/item/return_date_time.h>
#include <drizzled/item/empty_string.h>
#include "drizzled/session_list.h"
#include <drizzled/message/schema.pb.h>
#include <drizzled/plugin/client.h>
#include <drizzled/cached_directory.h>
#include "drizzled/sql_table.h"
#include "drizzled/global_charset_info.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/internal/m_string.h"
#include "drizzled/internal/my_sys.h"

#include <sys/stat.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;

namespace drizzled
{

inline const char *
str_or_nil(const char *str)
{
  return str ? str : "<nil>";
}

static void store_key_options(String *packet, Table *table, KeyInfo *key_info);


int wild_case_compare(const CHARSET_INFO * const cs, const char *str, const char *wildstr)
{
  register int flag;

  while (*wildstr)
  {
    while (*wildstr && *wildstr != internal::wild_many && *wildstr != internal::wild_one)
    {
      if (*wildstr == internal::wild_prefix && wildstr[1])
        wildstr++;
      if (my_toupper(cs, *wildstr++) != my_toupper(cs, *str++))
        return (1);
    }
    if (! *wildstr )
      return (*str != 0);
    if (*wildstr++ == internal::wild_one)
    {
      if (! *str++)
        return (1);	/* One char; skip */
    }
    else
    {						/* Found '*' */
      if (! *wildstr)
        return (0);		/* '*' as last char: OK */
      flag=(*wildstr != internal::wild_many && *wildstr != internal::wild_one);
      do
      {
        if (flag)
        {
          char cmp;
          if ((cmp= *wildstr) == internal::wild_prefix && wildstr[1])
            cmp= wildstr[1];
          cmp= my_toupper(cs, cmp);
          while (*str && my_toupper(cs, *str) != cmp)
            str++;
          if (! *str)
            return (1);
        }
        if (wild_case_compare(cs, str, wildstr) == 0)
          return (0);
      } while (*str++);
      return (1);
    }
  }

  return (*str != '\0');
}


bool drizzled_show_create(Session *session, TableList *table_list, bool is_if_not_exists)
{
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);

  /* Only one table for now, but VIEW can involve several tables */
  if (session->openTables(table_list))
  {
    if (session->is_error())
      return true;

    /*
      Clear all messages with 'error' level status and
      issue a warning with 'warning' level status in
      case of invalid view and last error is ER_VIEW_INVALID
    */
    drizzle_reset_errors(session, true);
    session->clear_error();
  }

  buffer.length(0);

  if (store_create_info(table_list, &buffer, is_if_not_exists))
    return true;

  List<Item> field_list;
  {
    field_list.push_back(new Item_empty_string("Table",NAME_CHAR_LEN));
    // 1024 is for not to confuse old clients
    field_list.push_back(new Item_empty_string("Create Table",
                                               max(buffer.length(),(uint32_t)1024)));
  }

  if (session->client->sendFields(&field_list))
    return true;
  {
    session->client->store(table_list->table->alias);
  }

  session->client->store(buffer.ptr(), buffer.length());

  if (session->client->flush())
    return true;

  session->my_eof();
  return false;
}

/**
  Get a CREATE statement for a given database.

  The database is identified by its name, passed as @c dbname parameter.
  The name should be encoded using the system character set (UTF8 currently).

  Resulting statement is stored in the string pointed by @c buffer. The string
  is emptied first and its character set is set to the system character set.

  If is_if_not_exists is set, then
  the resulting CREATE statement contains "IF NOT EXISTS" clause. Other flags
  in @c create_options are ignored.

  @param  session           The current thread instance.
  @param  dbname        The name of the database.
  @param  buffer        A String instance where the statement is stored.
  @param  create_info   If not NULL, the options member influences the resulting
                        CRATE statement.

  @returns true if errors are detected, false otherwise.
*/

static bool store_db_create_info(SchemaIdentifier &schema_identifier, string &buffer, bool if_not_exists)
{
  message::Schema schema;

  bool found= plugin::StorageEngine::getSchemaDefinition(schema_identifier, schema);
  if (not found)
    return false;

  buffer.append("CREATE DATABASE ");

  if (if_not_exists)
    buffer.append("IF NOT EXISTS ");

  buffer.append("`");
  buffer.append(schema.name());
  buffer.append("`");

  if (schema.has_collation())
  {
    buffer.append(" COLLATE = ");
    buffer.append(schema.collation());
  }

  return true;
}

bool mysqld_show_create_db(Session &session, SchemaIdentifier &schema_identifier, bool if_not_exists)
{
  message::Schema schema_message;
  string buffer;

  if (not plugin::StorageEngine::getSchemaDefinition(schema_identifier, schema_message))
  {
    /*
      This assumes that the only reason for which store_db_create_info()
      can fail is incorrect database name (which is the case now).
    */
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_identifier.getSQLPath().c_str());
    return true;
  }

  if (not store_db_create_info(schema_identifier, buffer, if_not_exists))
  {
    /*
      This assumes that the only reason for which store_db_create_info()
      can fail is incorrect database name (which is the case now).
    */
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_identifier.getSQLPath().c_str());
    return true;
  }

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Database",NAME_CHAR_LEN));
  field_list.push_back(new Item_empty_string("Create Database",1024));

  if (session.client->sendFields(&field_list))
    return true;

  session.client->store(schema_message.name());
  session.client->store(buffer);

  if (session.client->flush())
    return true;

  session.my_eof();

  return false;
}

/*
  Get the quote character for displaying an identifier.

  SYNOPSIS
    get_quote_char_for_identifier()

  IMPLEMENTATION
    Force quoting in the following cases:
      - name is empty (for one, it is possible when we use this function for
        quoting user and host names for DEFINER clause);
      - name is a keyword;
      - name includes a special character;
    Otherwise identifier is quoted only if the option OPTION_QUOTE_SHOW_CREATE
    is set.

  RETURN
    EOF	  No quote character is needed
    #	  Quote character
*/

int get_quote_char_for_identifier()
{
  return '`';
}


#define LIST_PROCESS_HOST_LEN 64

static bool get_field_default_value(Field *timestamp_field,
                                    Field *field, String *def_value,
                                    bool quoted)
{
  bool has_default;
  bool has_now_default;

  /*
     We are using CURRENT_TIMESTAMP instead of NOW because it is
     more standard
  */
  has_now_default= (timestamp_field == field &&
                    field->unireg_check != Field::TIMESTAMP_UN_FIELD);

  has_default= (field->type() != DRIZZLE_TYPE_BLOB &&
                !(field->flags & NO_DEFAULT_VALUE_FLAG) &&
                field->unireg_check != Field::NEXT_NUMBER);

  def_value->length(0);
  if (has_default)
  {
    if (has_now_default)
      def_value->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
    else if (!field->is_null())
    {                                             // Not null by default
      char tmp[MAX_FIELD_WIDTH];
      String type(tmp, sizeof(tmp), field->charset());
      field->val_str(&type);
      if (type.length())
      {
        String def_val;
        uint32_t dummy_errors;
        /* convert to system_charset_info == utf8 */
        def_val.copy(type.ptr(), type.length(), field->charset(),
                     system_charset_info, &dummy_errors);
        if (quoted)
          append_unescaped(def_value, def_val.ptr(), def_val.length());
        else
          def_value->append(def_val.ptr(), def_val.length());
      }
      else if (quoted)
        def_value->append(STRING_WITH_LEN("''"));
    }
    else if (field->maybe_null() && quoted)
      def_value->append(STRING_WITH_LEN("NULL"));    // Null as default
    else
      return false;
  }
  return has_default;
}

/*
  Build a CREATE TABLE statement for a table.

  SYNOPSIS
    store_create_info()
    table_list        A list containing one table to write statement
                      for.
    packet            Pointer to a string where statement will be
                      written.

  NOTE
    Currently always return 0, but might return error code in the
    future.

  RETURN
    0       OK
 */

int store_create_info(TableList *table_list, String *packet, bool is_if_not_exists)
{
  List<Item> field_list;
  char tmp[MAX_FIELD_WIDTH], *for_str, def_value_buf[MAX_FIELD_WIDTH];
  const char *alias;
  string buff;
  String type(tmp, sizeof(tmp), system_charset_info);
  String def_value(def_value_buf, sizeof(def_value_buf), system_charset_info);
  Field **ptr,*field;
  uint32_t primary_key;
  KeyInfo *key_info;
  Table *table= table_list->table;
  Cursor *cursor= table->cursor;
  HA_CREATE_INFO create_info;
  my_bitmap_map *old_map;

  table->restoreRecordAsDefault(); // Get empty record

  if (table->getShare()->tmp_table)
    packet->append(STRING_WITH_LEN("CREATE TEMPORARY TABLE "));
  else
    packet->append(STRING_WITH_LEN("CREATE TABLE "));
  if (is_if_not_exists)
    packet->append(STRING_WITH_LEN("IF NOT EXISTS "));
  alias= table->getShare()->getTableName();

  packet->append_identifier(alias, strlen(alias));
  packet->append(STRING_WITH_LEN(" (\n"));
  /*
    We need this to get default values from the table
    We have to restore the read_set if we are called from insert in case
    of row based replication.
  */
  old_map= table->use_all_columns(table->read_set);

  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    uint32_t flags = field->flags;

    if (ptr != table->field)
      packet->append(STRING_WITH_LEN(",\n"));

    packet->append(STRING_WITH_LEN("  "));
    packet->append_identifier(field->field_name, strlen(field->field_name));
    packet->append(' ');
    // check for surprises from the previous call to Field::sql_type()
    if (type.ptr() != tmp)
      type.set(tmp, sizeof(tmp), system_charset_info);
    else
      type.set_charset(system_charset_info);

    field->sql_type(type);
    packet->append(type.ptr(), type.length(), system_charset_info);

    if (field->has_charset())
    {
      /*
        For string types dump collation name only if
        collation is not primary for the given charset
      */
      if (!(field->charset()->state & MY_CS_PRIMARY))
      {
        packet->append(STRING_WITH_LEN(" COLLATE "));
        packet->append(field->charset()->name);
      }
    }

    if (flags & NOT_NULL_FLAG)
      packet->append(STRING_WITH_LEN(" NOT NULL"));
    else if (field->type() == DRIZZLE_TYPE_TIMESTAMP)
    {
      /*
        TIMESTAMP field require explicit NULL flag, because unlike
        all other fields they are treated as NOT NULL by default.
      */
      packet->append(STRING_WITH_LEN(" NULL"));
    }
    {
      /*
        Add field flags about FIELD FORMAT (FIXED or DYNAMIC)
        and about STORAGE (DISK or MEMORY).
      */
      enum column_format_type column_format= (enum column_format_type)
        ((flags >> COLUMN_FORMAT_FLAGS) & COLUMN_FORMAT_MASK);
      if (column_format)
      {
        packet->append(STRING_WITH_LEN(" /*!"));
        packet->append(STRING_WITH_LEN(" COLUMN_FORMAT"));
        if (column_format == COLUMN_FORMAT_TYPE_FIXED)
          packet->append(STRING_WITH_LEN(" FIXED */"));
        else
          packet->append(STRING_WITH_LEN(" DYNAMIC */"));
      }
    }
    if (get_field_default_value(table->timestamp_field, field, &def_value, 1))
    {
      packet->append(STRING_WITH_LEN(" DEFAULT "));
      packet->append(def_value.ptr(), def_value.length(), system_charset_info);
    }

    if (table->timestamp_field == field && field->unireg_check != Field::TIMESTAMP_DN_FIELD)
      packet->append(STRING_WITH_LEN(" ON UPDATE CURRENT_TIMESTAMP"));

    if (field->unireg_check == Field::NEXT_NUMBER)
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT"));

    if (field->comment.length)
    {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, field->comment.str, field->comment.length);
    }
  }

  key_info= table->key_info;
  memset(&create_info, 0, sizeof(create_info));
  /* Allow update_create_info to update row type */
  create_info.row_type= table->getShare()->row_type;
  cursor->update_create_info(&create_info);
  primary_key= table->getShare()->primary_key;

  for (uint32_t i=0 ; i < table->getShare()->keys ; i++,key_info++)
  {
    KeyPartInfo *key_part= key_info->key_part;
    bool found_primary=0;
    packet->append(STRING_WITH_LEN(",\n  "));

    if (i == primary_key && is_primary_key(key_info))
    {
      found_primary=1;
      /*
        No space at end, because a space will be added after where the
        identifier would go, but that is not added for primary key.
      */
      packet->append(STRING_WITH_LEN("PRIMARY KEY"));
    }
    else if (key_info->flags & HA_NOSAME)
      packet->append(STRING_WITH_LEN("UNIQUE KEY "));
    else
      packet->append(STRING_WITH_LEN("KEY "));

    if (!found_primary)
     packet->append_identifier(key_info->name, strlen(key_info->name));

    packet->append(STRING_WITH_LEN(" ("));

    for (uint32_t j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (j)
        packet->append(',');

      if (key_part->field)
        packet->append_identifier(key_part->field->field_name,
                                  strlen(key_part->field->field_name));
      if (key_part->field &&
          (key_part->length !=
           table->field[key_part->fieldnr-1]->key_length()))
      {
        buff.assign("(");
        buff.append(to_string((int32_t) key_part->length /
                              key_part->field->charset()->mbmaxlen));
        buff.append(")");
        packet->append(buff.c_str(), buff.length());
      }
    }
    packet->append(')');
    store_key_options(packet, table, key_info);
  }

  /*
    Get possible foreign key definitions stored in InnoDB and append them
    to the CREATE TABLE statement
  */

  if ((for_str= cursor->get_foreign_key_create_info()))
  {
    packet->append(for_str, strlen(for_str));
    cursor->free_foreign_key_create_info(for_str);
  }

  packet->append(STRING_WITH_LEN("\n)"));
  {
    /*
      Get possible table space definitions and append them
      to the CREATE TABLE statement
    */

    /* 
      We should always store engine since we will now be 
      making sure engines accept options (aka... no
      dangling arguments for engines.
    */
    packet->append(STRING_WITH_LEN(" ENGINE="));
    packet->append(cursor->getEngine()->getName().c_str());

    size_t num_engine_options= table->getShare()->getTableProto()->engine().options_size();
    for (size_t x= 0; x < num_engine_options; ++x)
    {
      const message::Engine::Option &option= table->getShare()->getTableProto()->engine().options(x);
      packet->append(" ");
      packet->append(option.name().c_str());
      packet->append("=");
      append_unescaped(packet, option.state().c_str(), option.state().length());
    }

#if 0
    if (create_info.row_type != ROW_TYPE_DEFAULT)
    {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint32_t) create_info.row_type]);
    }
#endif
    if (table->getShare()->block_size)
    {
      packet->append(STRING_WITH_LEN(" BLOCK_SIZE="));
      buff= to_string(table->getShare()->block_size);
      packet->append(buff.c_str(), buff.length());
    }
    table->cursor->append_create_info(packet);
    if (table->getMutableShare()->hasComment() && table->getMutableShare()->getCommentLength())
    {
      packet->append(STRING_WITH_LEN(" COMMENT="));
      append_unescaped(packet, table->getMutableShare()->getComment(),
                       table->getMutableShare()->getCommentLength());
    }
  }
  table->restore_column_map(old_map);
  return(0);
}

static void store_key_options(String *packet, Table *, KeyInfo *key_info)
{
  if (key_info->algorithm == HA_KEY_ALG_BTREE)
    packet->append(STRING_WITH_LEN(" USING BTREE"));

  if (key_info->algorithm == HA_KEY_ALG_HASH)
    packet->append(STRING_WITH_LEN(" USING HASH"));

  assert(test(key_info->flags & HA_USES_COMMENT) ==
              (key_info->comment.length > 0));
  if (key_info->flags & HA_USES_COMMENT)
  {
    packet->append(STRING_WITH_LEN(" COMMENT "));
    append_unescaped(packet, key_info->comment.str,
                     key_info->comment.length);
  }
}


/****************************************************************************
  Return info about all processes
  returns for each thread: thread id, user, host, db, command, info
****************************************************************************/

class thread_info
{
  thread_info();
public:
  uint64_t thread_id;
  time_t start_time;
  uint32_t   command;
  string user;
  string host;
  string db;
  string proc_info;
  string state_info;
  string query;
  thread_info(uint64_t thread_id_arg,
              time_t start_time_arg,
              uint32_t command_arg,
              const string &user_arg,
              const string &host_arg,
              const string &db_arg,
              const string &proc_info_arg,
              const string &state_info_arg,
              const string &query_arg)
    : thread_id(thread_id_arg), start_time(start_time_arg), command(command_arg),
      user(user_arg), host(host_arg), db(db_arg), proc_info(proc_info_arg),
      state_info(state_info_arg), query(query_arg)
  {}
};

} /* namespace drizzled */
