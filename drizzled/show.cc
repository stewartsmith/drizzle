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
#include "drizzled/message/statement_transform.h"


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
  Table *table= table_list->table;

  table->restoreRecordAsDefault(); // Get empty record

  string create_sql;

  enum drizzled::message::TransformSqlError transform_err;

  (void)is_if_not_exists;

  transform_err= message::transformTableDefinitionToSql(*(table->getShare()->getTableProto()),
                                                        create_sql,
                                                        message::DRIZZLE,
                                                        false);

  packet->append(create_sql.c_str(), create_sql.length(), default_charset_info);

  return(0);
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
