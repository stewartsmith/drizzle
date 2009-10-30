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
#include <drizzled/server_includes.h>
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
#include "drizzled/plugin/registry.h"
#include <drizzled/plugin/info_schema_table.h>
#include <drizzled/message/schema.pb.h>
#include <drizzled/plugin/client.h>
#include <mysys/cached_directory.h>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;
using namespace drizzled;

inline const char *
str_or_nil(const char *str)
{
  return str ? str : "<nil>";
}

static void store_key_options(String *packet, Table *table, KEY *key_info);



int wild_case_compare(const CHARSET_INFO * const cs, const char *str,const char *wildstr)
{
  register int flag;
  while (*wildstr)
  {
    while (*wildstr && *wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == wild_prefix && wildstr[1])
        wildstr++;
      if (my_toupper(cs, *wildstr++) != my_toupper(cs, *str++))
        return (1);
    }
    if (! *wildstr )
      return (*str != 0);
    if (*wildstr++ == wild_one)
    {
      if (! *str++)
        return (1);	/* One char; skip */
    }
    else
    {						/* Found '*' */
      if (! *wildstr)
        return (0);		/* '*' as last char: OK */
      flag=(*wildstr != wild_many && *wildstr != wild_one);
      do
      {
        if (flag)
        {
          char cmp;
          if ((cmp= *wildstr) == wild_prefix && wildstr[1])
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
 * @brief
 *   Find subdirectories (schemas) in a given directory (datadir).
 *
 * @param[in]  session    Thread Cursor
 * @param[out] files      Put found entries in this list
 * @param[in]  path       Path to database
 * @param[in]  wild       Filter for found entries
 *
 * @retval false   Success
 * @retval true    Error
 */
static bool find_schemas(Session *session, vector<LEX_STRING*> &files,
                         const char *path, const char *wild)
{
  if (wild && (wild[0] == '\0'))
    wild= 0;

  CachedDirectory directory(path);

  if (directory.fail())
  {
    my_errno= directory.getError();
    my_error(ER_CANT_READ_DIR, MYF(0), path, my_errno);

    return true;
  }

  CachedDirectory::Entries entries= directory.getEntries();
  CachedDirectory::Entries::iterator entry_iter= entries.begin();

  while (entry_iter != entries.end())
  {
    uint32_t file_name_len;
    char uname[NAME_LEN + 1];                   /* Unencoded name */
    struct stat entry_stat;
    CachedDirectory::Entry *entry= *entry_iter;

    if ((entry->filename == ".") || (entry->filename == ".."))
    {
      ++entry_iter;
      continue;
    }

    if (stat(entry->filename.c_str(), &entry_stat))
    {
      my_errno= errno;
      my_error(ER_CANT_GET_STAT, MYF(0), entry->filename.c_str(), my_errno);
      return(true);
    }

    if (! S_ISDIR(entry_stat.st_mode))
    {
      ++entry_iter;
      continue;
    }

    file_name_len= filename_to_tablename(entry->filename.c_str(), uname,
                                         sizeof(uname));
    if (wild && wild_compare(uname, wild, 0))
    {
      ++entry_iter;
      continue;
    }

    LEX_STRING *file_name= 0;
    file_name= session->make_lex_string(file_name, uname, file_name_len, true);
    if (file_name == NULL)
      return(true);

    files.push_back(file_name);
    ++entry_iter;
  }

  return false;
}


bool drizzled_show_create(Session *session, TableList *table_list)
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

  if (store_create_info(table_list, &buffer, NULL))
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
    if (table_list->schema_table)
      session->client->store(table_list->schema_table->getTableName().c_str());
    else
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

  If HA_LEX_CREATE_IF_NOT_EXISTS flag is set in @c create_info->options, then
  the resulting CREATE statement contains "IF NOT EXISTS" clause. Other flags
  in @c create_options are ignored.

  @param  session           The current thread instance.
  @param  dbname        The name of the database.
  @param  buffer        A String instance where the statement is stored.
  @param  create_info   If not NULL, the options member influences the resulting
                        CRATE statement.

  @returns true if errors are detected, false otherwise.
*/

static bool store_db_create_info(const char *dbname, String *buffer, bool if_not_exists)
{
  message::Schema schema;

  if (!my_strcasecmp(system_charset_info, dbname,
                     INFORMATION_SCHEMA_NAME.c_str()))
  {
    dbname= INFORMATION_SCHEMA_NAME.c_str();
  }
  else
  {
    int r= get_database_metadata(dbname, &schema);
    if(r < 0)
      return true;
  }

  buffer->length(0);
  buffer->free();
  buffer->set_charset(system_charset_info);
  buffer->append(STRING_WITH_LEN("CREATE DATABASE "));

  if (if_not_exists)
    buffer->append(STRING_WITH_LEN("IF NOT EXISTS "));

  buffer->append_identifier(dbname, strlen(dbname));

  if (schema.has_collation() && strcmp(schema.collation().c_str(),
                                       default_charset_info->name))
  {
    buffer->append(" COLLATE = ");
    buffer->append(schema.collation().c_str());
  }

  return false;
}

bool mysqld_show_create_db(Session *session, char *dbname, bool if_not_exists)
{
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);

  if (store_db_create_info(dbname, &buffer, if_not_exists))
  {
    /*
      This assumes that the only reason for which store_db_create_info()
      can fail is incorrect database name (which is the case now).
    */
    my_error(ER_BAD_DB_ERROR, MYF(0), dbname);
    return true;
  }

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Database",NAME_CHAR_LEN));
  field_list.push_back(new Item_empty_string("Create Database",1024));

  if (session->client->sendFields(&field_list))
    return true;

  session->client->store(dbname, strlen(dbname));
  session->client->store(buffer.ptr(), buffer.length());

  if (session->client->flush())
    return true;
  session->my_eof();
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
    create_info_arg   Pointer to create information that can be used
                      to tailor the format of the statement.  Can be
                      NULL, in which case only SQL_MODE is considered
                      when building the statement.

  NOTE
    Currently always return 0, but might return error code in the
    future.

  RETURN
    0       OK
 */

int store_create_info(TableList *table_list, String *packet, HA_CREATE_INFO *create_info_arg)
{
  List<Item> field_list;
  char tmp[MAX_FIELD_WIDTH], *for_str, def_value_buf[MAX_FIELD_WIDTH];
  const char *alias;
  string buff;
  String type(tmp, sizeof(tmp), system_charset_info);
  String def_value(def_value_buf, sizeof(def_value_buf), system_charset_info);
  Field **ptr,*field;
  uint32_t primary_key;
  KEY *key_info;
  Table *table= table_list->table;
  Cursor *file= table->file;
  TableShare *share= table->s;
  HA_CREATE_INFO create_info;
  bool show_table_options= false;
  my_bitmap_map *old_map;

  table->restoreRecordAsDefault(); // Get empty record

  if (share->tmp_table)
    packet->append(STRING_WITH_LEN("CREATE TEMPORARY TABLE "));
  else
    packet->append(STRING_WITH_LEN("CREATE TABLE "));
  if (create_info_arg &&
      (create_info_arg->options & HA_LEX_CREATE_IF_NOT_EXISTS))
    packet->append(STRING_WITH_LEN("IF NOT EXISTS "));
  if (table_list->schema_table)
    alias= table_list->schema_table->getTableName().c_str();
  else
    alias= share->table_name.str;

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
      if (field->charset() != share->table_charset)
      {
        packet->append(STRING_WITH_LEN(" CHARACTER SET "));
        packet->append(field->charset()->csname);
      }

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
  create_info.row_type= share->row_type;
  file->update_create_info(&create_info);
  primary_key= share->primary_key;

  for (uint32_t i=0 ; i < share->keys ; i++,key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
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

  if ((for_str= file->get_foreign_key_create_info()))
  {
    packet->append(for_str, strlen(for_str));
    file->free_foreign_key_create_info(for_str);
  }

  packet->append(STRING_WITH_LEN("\n)"));
  {
    show_table_options= true;
    /*
      Get possible table space definitions and append them
      to the CREATE TABLE statement
    */

    /*
      IF   check_create_info
      THEN add ENGINE only if it was used when creating the table
    */
    if (!create_info_arg ||
        (create_info_arg->used_fields & HA_CREATE_USED_ENGINE))
    {
      packet->append(STRING_WITH_LEN(" ENGINE="));
      packet->append(file->engine->getName().c_str());
    }

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=1"));
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=0"));
    if (create_info.row_type != ROW_TYPE_DEFAULT)
    {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint32_t) create_info.row_type]);
    }
    if (table->s->hasKeyBlockSize())
    {
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      buff= to_string(table->s->getKeyBlockSize());
      packet->append(buff.c_str(), buff.length());
    }
    if (share->block_size)
    {
      packet->append(STRING_WITH_LEN(" BLOCK_SIZE="));
      buff= to_string(share->block_size);
      packet->append(buff.c_str(), buff.length());
    }
    table->file->append_create_info(packet);
    if (share->hasComment() && share->getCommentLength())
    {
      packet->append(STRING_WITH_LEN(" COMMENT="));
      append_unescaped(packet, share->getComment(),
                       share->getCommentLength());
    }
  }
  table->restore_column_map(old_map);
  return(0);
}

static void store_key_options(String *packet, Table *table, KEY *key_info)
{
  char *end, buff[32];

  if (key_info->algorithm == HA_KEY_ALG_BTREE)
    packet->append(STRING_WITH_LEN(" USING BTREE"));

  if (key_info->algorithm == HA_KEY_ALG_HASH)
    packet->append(STRING_WITH_LEN(" USING HASH"));

  if ((key_info->flags & HA_USES_BLOCK_SIZE) &&
      table->s->getKeyBlockSize() != key_info->block_size)
  {
    packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
    end= int64_t10_to_str(key_info->block_size, buff, 10);
    packet->append(buff, (uint32_t) (end - buff));
  }

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

class thread_info :public ilink {
public:
  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint32_t) size);
  }
  static void operator delete(void *, size_t)
  { TRASH(ptr, size); }

  my_thread_id thread_id;
  time_t start_time;
  uint32_t   command;
  const char *user,*host,*db,*proc_info,*state_info;
  char *query;
};

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class I_List<thread_info>;
#endif

void mysqld_list_processes(Session *session,const char *user, bool)
{
  Item *field;
  List<Item> field_list;
  I_List<thread_info> thread_infos;

  field_list.push_back(new Item_int("Id", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("User",16));
  field_list.push_back(new Item_empty_string("Host",LIST_PROCESS_HOST_LEN));
  field_list.push_back(field=new Item_empty_string("db",NAME_CHAR_LEN));
  field->maybe_null= true;
  field_list.push_back(new Item_empty_string("Command",16));
  field_list.push_back(new Item_return_int("Time",7, DRIZZLE_TYPE_LONG));
  field_list.push_back(field=new Item_empty_string("State",30));
  field->maybe_null= true;
  field_list.push_back(field=new Item_empty_string("Info", PROCESS_LIST_WIDTH));
  field->maybe_null= true;
  if (session->client->sendFields(&field_list))
    return;

  pthread_mutex_lock(&LOCK_thread_count); // For unlink from list
  if (!session->killed)
  {
    Session *tmp;
    for( vector<Session*>::iterator it= session_list.begin(); it != session_list.end(); ++it )
    {
      tmp= *it;
      Security_context *tmp_sctx= &tmp->security_ctx;
      struct st_my_thread_var *mysys_var;
      if (tmp->client->isConnected() && (!user || (tmp_sctx->user.c_str() && !strcmp(tmp_sctx->user.c_str(), user))))
      {
        thread_info *session_info= new thread_info;

        session_info->thread_id=tmp->thread_id;
        session_info->user= session->strdup(tmp_sctx->user.c_str() ? tmp_sctx->user.c_str() : "unauthenticated user");
        session_info->host= session->strdup(tmp_sctx->ip.c_str());
        if ((session_info->db=tmp->db))             // Safe test
          session_info->db=session->strdup(session_info->db);
        session_info->command=(int) tmp->command;
        if ((mysys_var= tmp->mysys_var))
          pthread_mutex_lock(&mysys_var->mutex);

        if (tmp->killed == Session::KILL_CONNECTION)
          session_info->proc_info= (char*) "Killed";
        else
          session_info->proc_info= command_name[session_info->command].str;

        session_info->state_info= (char*) (tmp->client->isWriting() ?
                                           "Writing to net" :
                                           tmp->client->isReading() ?
                                           (session_info->command == COM_SLEEP ?
                                            NULL : "Reading from net") :
                                       tmp->get_proc_info() ? tmp->get_proc_info() :
                                       tmp->mysys_var &&
                                       tmp->mysys_var->current_cond ?
                                       "Waiting on cond" : NULL);
        if (mysys_var)
          pthread_mutex_unlock(&mysys_var->mutex);

        session_info->start_time= tmp->start_time;
        session_info->query= NULL;
        if (tmp->process_list_info[0])
          session_info->query= session->strdup(tmp->process_list_info);
        thread_infos.append(session_info);
      }
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);

  thread_info *session_info;
  time_t now= time(NULL);
  while ((session_info=thread_infos.get()))
  {
    session->client->store((uint64_t) session_info->thread_id);
    session->client->store(session_info->user);
    session->client->store(session_info->host);
    session->client->store(session_info->db);
    session->client->store(session_info->proc_info);

    if (session_info->start_time)
      session->client->store((uint32_t) (now - session_info->start_time));
    else
      session->client->store();

    session->client->store(session_info->state_info);
    session->client->store(session_info->query);

    if (session->client->flush())
      break;
  }
  session->my_eof();
  return;
}

/*****************************************************************************
  Status functions
*****************************************************************************/

static vector<SHOW_VAR *> all_status_vars;
static bool status_vars_inited= 0;
static int show_var_cmp(const void *var1, const void *var2)
{
  return strcmp(((SHOW_VAR*)var1)->name, ((SHOW_VAR*)var2)->name);
}

class show_var_cmp_functor
{
  public:
  show_var_cmp_functor() { }
  inline bool operator()(const SHOW_VAR *var1, const SHOW_VAR *var2) const
  {
    int val= strcmp(var1->name, var2->name);
    return (val < 0);
  }
};

class show_var_remove_if
{
  public:
  show_var_remove_if() { }
  inline bool operator()(const SHOW_VAR *curr) const
  {
    return (curr->type == SHOW_UNDEF);
  }
};

SHOW_VAR *getFrontOfStatusVars()
{
  return all_status_vars.front();
}

/*
  Adds an array of SHOW_VAR entries to the output of SHOW STATUS

  SYNOPSIS
    add_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to add to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    The handling of all_status_vars[] is completely internal, it's allocated
    automatically when something is added to it, and deleted completely when
    the last entry is removed.

    As a special optimization, if add_status_vars() is called before
    init_status_vars(), it assumes "startup mode" - neither concurrent access
    to the array nor SHOW STATUS are possible (thus it skips locks and qsort)
*/
int add_status_vars(SHOW_VAR *list)
{
  int res= 0;
  if (status_vars_inited)
    pthread_mutex_lock(&LOCK_status);
  while (list->name)
    all_status_vars.insert(all_status_vars.begin(), list++);
  if (status_vars_inited)
    sort(all_status_vars.begin(), all_status_vars.end(),
         show_var_cmp_functor());
  if (status_vars_inited)
    pthread_mutex_unlock(&LOCK_status);
  return res;
}

/*
  Make all_status_vars[] usable for SHOW STATUS

  NOTE
    See add_status_vars(). Before init_status_vars() call, add_status_vars()
    works in a special fast "startup" mode. Thus init_status_vars()
    should be called as late as possible but before enabling multi-threading.
*/
void init_status_vars()
{
  status_vars_inited= 1;
  sort(all_status_vars.begin(), all_status_vars.end(),
       show_var_cmp_functor());
}

void reset_status_vars()
{
  vector<SHOW_VAR *>::iterator p= all_status_vars.begin();
  while (p != all_status_vars.end())
  {
    /* Note that SHOW_LONG_NOFLUSH variables are not reset */
    if ((*p)->type == SHOW_LONG)
      (*p)->value= 0;
    ++p;
  }
}

/*
  catch-all cleanup function, cleans up everything no matter what

  DESCRIPTION
    This function is not strictly required if all add_to_status/
    remove_status_vars are properly paired, but it's a safety measure that
    deletes everything from the all_status_vars vector even if some
    remove_status_vars were forgotten
*/
void free_status_vars()
{
  all_status_vars.clear();
}

/*
  Removes an array of SHOW_VAR entries from the output of SHOW STATUS

  SYNOPSIS
    remove_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to remove to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    there's lots of room for optimizing this, especially in non-sorted mode,
    but nobody cares - it may be called only in case of failed plugin
    initialization in the mysqld startup.
*/

void remove_status_vars(SHOW_VAR *list)
{
  if (status_vars_inited)
  {
    pthread_mutex_lock(&LOCK_status);
    SHOW_VAR *all= all_status_vars.front();
    int a= 0, b= all_status_vars.size(), c= (a+b)/2;

    for (; list->name; list++)
    {
      int res= 0;
      for (a= 0, b= all_status_vars.size(); b-a > 1; c= (a+b)/2)
      {
        res= show_var_cmp(list, all+c);
        if (res < 0)
          b= c;
        else if (res > 0)
          a= c;
        else
          break;
      }
      if (res == 0)
        all[c].type= SHOW_UNDEF;
    }
    /* removes all the SHOW_UNDEF elements from the vector */
    all_status_vars.erase(std::remove_if(all_status_vars.begin(),
                            all_status_vars.end(),show_var_remove_if()),
                            all_status_vars.end());
    pthread_mutex_unlock(&LOCK_status);
  }
  else
  {
    SHOW_VAR *all= all_status_vars.front();
    uint32_t i;
    for (; list->name; list++)
    {
      for (i= 0; i < all_status_vars.size(); i++)
      {
        if (show_var_cmp(list, all+i))
          continue;
        all[i].type= SHOW_UNDEF;
        break;
      }
    }
    /* removes all the SHOW_UNDEF elements from the vector */
    all_status_vars.erase(std::remove_if(all_status_vars.begin(),
                            all_status_vars.end(),show_var_remove_if()),
                            all_status_vars.end());
  }
}

/* collect status for all running threads */

void calc_sum_of_all_status(STATUS_VAR *to)
{
  /* Ensure that thread id not killed during loop */
  pthread_mutex_lock(&LOCK_thread_count); // For unlink from list

  /* Get global values as base */
  *to= global_status_var;

  /* Add to this status from existing threads */
  for( vector<Session*>::iterator it= session_list.begin(); it != session_list.end(); ++it )
  {
    add_to_status(to, &((*it)->status_var));
  }

  pthread_mutex_unlock(&LOCK_thread_count);
  return;
}

/*
  Store record to I_S table, convert HEAP table
  to MyISAM if necessary

  SYNOPSIS
    schema_table_store_record()
    session                   thread Cursor
    table                 Information schema table to be updated

  RETURN
    0	                  success
    1	                  error
*/

bool schema_table_store_record(Session *session, Table *table)
{
  int error;
  if ((error= table->file->ha_write_row(table->record[0])))
  {
    Tmp_Table_Param *param= table->pos_in_table_list->schema_table_param;

    if (create_myisam_from_heap(session, table, param->start_recinfo,
                                &param->recinfo, error, 0))
      return true;
  }
  return false;
}


static int make_table_list(Session *session, Select_Lex *sel,
                           LEX_STRING *db_name, LEX_STRING *table_name)
{
  Table_ident *table_ident;
  table_ident= new Table_ident(*db_name, *table_name);
  sel->init_query();
  if (! sel->add_table_to_list(session, table_ident, 0, 0, TL_READ))
    return 1;
  return 0;
}


/**
  @brief    Get lookup value from the part of 'WHERE' condition

  @details This function gets lookup value from
           the part of 'WHERE' condition if it's possible and
           fill appropriate lookup_field_vals struct field
           with this value.

  @param[in]      session                   thread Cursor
  @param[in]      item_func             part of WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

static bool get_lookup_value(Session *session, Item_func *item_func,
                             TableList *table,
                             LOOKUP_FIELD_VALUES *lookup_field_vals)
{
  plugin::InfoSchemaTable *schema_table= table->schema_table;
  const char *field_name1= schema_table->getFirstColumnIndex() >= 0 ?
    schema_table->getColumnName(schema_table->getFirstColumnIndex()).c_str() : "";
  const char *field_name2= schema_table->getSecondColumnIndex() >= 0 ?
    schema_table->getColumnName(schema_table->getSecondColumnIndex()).c_str() : "";

  if (item_func->functype() == Item_func::EQ_FUNC ||
      item_func->functype() == Item_func::EQUAL_FUNC)
  {
    int idx_field, idx_val;
    char tmp[MAX_FIELD_WIDTH];
    String *tmp_str, str_buff(tmp, sizeof(tmp), system_charset_info);
    Item_field *item_field;
    const CHARSET_INFO * const cs= system_charset_info;

    if (item_func->arguments()[0]->type() == Item::FIELD_ITEM &&
        item_func->arguments()[1]->const_item())
    {
      idx_field= 0;
      idx_val= 1;
    }
    else if (item_func->arguments()[1]->type() == Item::FIELD_ITEM &&
             item_func->arguments()[0]->const_item())
    {
      idx_field= 1;
      idx_val= 0;
    }
    else
      return 0;

    item_field= (Item_field*) item_func->arguments()[idx_field];
    if (table->table != item_field->field->table)
      return 0;
    tmp_str= item_func->arguments()[idx_val]->val_str(&str_buff);

    /* impossible value */
    if (!tmp_str)
      return 1;

    /* Lookup value is database name */
    if (!cs->coll->strnncollsp(cs, (unsigned char *) field_name1, strlen(field_name1),
                               (unsigned char *) item_field->field_name,
                               strlen(item_field->field_name), 0))
    {
      session->make_lex_string(&lookup_field_vals->db_value, tmp_str->ptr(),
                           tmp_str->length(), false);
    }
    /* Lookup value is table name */
    else if (!cs->coll->strnncollsp(cs, (unsigned char *) field_name2,
                                    strlen(field_name2),
                                    (unsigned char *) item_field->field_name,
                                    strlen(item_field->field_name), 0))
    {
      session->make_lex_string(&lookup_field_vals->table_value, tmp_str->ptr(),
                           tmp_str->length(), false);
    }
  }
  return 0;
}


/**
  @brief    Calculates lookup values from 'WHERE' condition

  @details This function calculates lookup value(database name, table name)
           from 'WHERE' condition if it's possible and
           fill lookup_field_vals struct fields with these values.

  @param[in]      session                   thread Cursor
  @param[in]      cond                  WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool calc_lookup_values_from_cond(Session *session, COND *cond, TableList *table,
                                  LOOKUP_FIELD_VALUES *lookup_field_vals)
{
  if (!cond)
    return 0;

  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item= li++))
      {
        if (item->type() == Item::FUNC_ITEM)
        {
          if (get_lookup_value(session, (Item_func*)item, table, lookup_field_vals))
            return 1;
        }
        else
        {
          if (calc_lookup_values_from_cond(session, item, table, lookup_field_vals))
            return 1;
        }
      }
    }
    return 0;
  }
  else if (cond->type() == Item::FUNC_ITEM &&
           get_lookup_value(session, (Item_func*) cond, table, lookup_field_vals))
    return 1;
  return 0;
}


static bool uses_only_table_name_fields(Item *item, TableList *table)
{
  if (item->type() == Item::FUNC_ITEM)
  {
    Item_func *item_func= (Item_func*)item;
    for (uint32_t i=0; i<item_func->argument_count(); i++)
    {
      if (!uses_only_table_name_fields(item_func->arguments()[i], table))
        return 0;
    }
  }
  else if (item->type() == Item::FIELD_ITEM)
  {
    Item_field *item_field= (Item_field*)item;
    const CHARSET_INFO * const cs= system_charset_info;
    plugin::InfoSchemaTable *schema_table= table->schema_table;
    const char *field_name1= schema_table->getFirstColumnIndex() >= 0 ?
      schema_table->getColumnName(schema_table->getFirstColumnIndex()).c_str() : "";
    const char *field_name2= schema_table->getSecondColumnIndex() >= 0 ?
      schema_table->getColumnName(schema_table->getSecondColumnIndex()).c_str() : "";
    if (table->table != item_field->field->table ||
        (cs->coll->strnncollsp(cs, (unsigned char *) field_name1, strlen(field_name1),
                               (unsigned char *) item_field->field_name,
                               strlen(item_field->field_name), 0) &&
         cs->coll->strnncollsp(cs, (unsigned char *) field_name2, strlen(field_name2),
                               (unsigned char *) item_field->field_name,
                               strlen(item_field->field_name), 0)))
      return 0;
  }
  else if (item->type() == Item::REF_ITEM)
    return uses_only_table_name_fields(item->real_item(), table);

  if (item->type() == Item::SUBSELECT_ITEM && !item->const_item())
    return 0;

  return 1;
}


static COND * make_cond_for_info_schema(COND *cond, TableList *table)
{
  if (!cond)
    return (COND*) 0;
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (COND*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_info_schema(item, table);
	if (fix)
	  new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (COND*) 0;
      case 1:
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
	return new_cond;
      }
    }
    else
    {						// Or list
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (COND*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_info_schema(item, table);
	if (!fix)
	  return (COND*) 0;
	new_cond->argument_list()->push_back(fix);
      }
      new_cond->quick_fix_field();
      new_cond->top_level_item();
      return new_cond;
    }
  }

  if (!uses_only_table_name_fields(cond, table))
    return (COND*) 0;
  return cond;
}


/**
  @brief   Calculate lookup values(database name, table name)

  @details This function calculates lookup values(database name, table name)
           from 'WHERE' condition or wild values (for 'SHOW' commands only)
           from LEX struct and fill lookup_field_vals struct field
           with these values.

  @param[in]      session                   thread Cursor
  @param[in]      cond                  WHERE condition
  @param[in]      tables                I_S table
  @param[in, out] lookup_field_values   Struct which holds lookup values

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

bool get_lookup_field_values(Session *session, COND *cond, TableList *tables,
                             LOOKUP_FIELD_VALUES *lookup_field_values)
{
  LEX *lex= session->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NULL;
  memset(lookup_field_values, 0, sizeof(LOOKUP_FIELD_VALUES));
  switch (lex->sql_command) {
  case SQLCOM_SHOW_DATABASES:
    if (wild)
    {
      lookup_field_values->db_value.str= (char*) wild;
      lookup_field_values->db_value.length= strlen(wild);
      lookup_field_values->wild_db_value= 1;
    }
    return 0;
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TABLE_STATUS:
    lookup_field_values->db_value.str= lex->select_lex.db;
    lookup_field_values->db_value.length=strlen(lex->select_lex.db);
    if (wild)
    {
      lookup_field_values->table_value.str= (char*)wild;
      lookup_field_values->table_value.length= strlen(wild);
      lookup_field_values->wild_table_value= 1;
    }
    return 0;
  default:
    /*
      The "default" is for queries over I_S.
      All previous cases handle SHOW commands.
    */
    return calc_lookup_values_from_cond(session, cond, tables, lookup_field_values);
  }
}


/**
 * Function used for sorting with std::sort within make_db_list.
 *
 * @returns true if a < b, false otherwise
 */

static bool lex_string_sort(const LEX_STRING *a, const LEX_STRING *b)
{
  return (strcmp(a->str, b->str) < 0);
}


/**
 * @brief
 *   Create db names list. Information schema name always is first in list
 *
 * @param[in]  session          Thread Cursor
 * @param[out] files            List of db names
 * @param[in]  wild             Wild string
 * @param[in]  idx_field_vals   idx_field_vals->db_name contains db name or
 *                              wild string
 * @param[out] with_i_schema    Returns 1 if we added 'IS' name to list
 *                              otherwise returns 0
 *
 * @retval 0   Success
 * @retval 1   Error
 */
int make_db_list(Session *session, vector<LEX_STRING*> &files,
                 LOOKUP_FIELD_VALUES *lookup_field_vals,
                 bool *with_i_schema)
{
  LEX_STRING *i_s_name_copy= 0;
  i_s_name_copy= session->make_lex_string(i_s_name_copy,
                                      INFORMATION_SCHEMA_NAME.c_str(),
                                      INFORMATION_SCHEMA_NAME.length(), true);
  *with_i_schema= 0;
  if (lookup_field_vals->wild_db_value)
  {
    /*
      This part of code is only for SHOW DATABASES command.
      idx_field_vals->db_value can be 0 when we don't use
      LIKE clause (see also get_index_field_values() function)
    */
    if (!lookup_field_vals->db_value.str ||
        !wild_case_compare(system_charset_info,
                           INFORMATION_SCHEMA_NAME.c_str(),
                           lookup_field_vals->db_value.str))
    {
      *with_i_schema= 1;
      files.push_back(i_s_name_copy);
    }

    if (find_schemas(session, files, drizzle_data_home,
                     lookup_field_vals->db_value.str) == true)
    {
      return 1;
    }

    sort(files.begin()+1, files.end(), lex_string_sort);
    return 0;
  }


  /*
    If we have db lookup vaule we just add it to list and
    exit from the function
  */
  if (lookup_field_vals->db_value.str)
  {
    if (!my_strcasecmp(system_charset_info, INFORMATION_SCHEMA_NAME.c_str(),
                       lookup_field_vals->db_value.str))
    {
      *with_i_schema= 1;
      files.push_back(i_s_name_copy);
      return 0;
    }

    files.push_back(&lookup_field_vals->db_value);
    return 0;
  }

  /*
    Create list of existing databases. It is used in case
    of select from information schema table
  */
  files.push_back(i_s_name_copy);

  *with_i_schema= 1;

  if (find_schemas(session, files, drizzle_data_home, NULL) == true)
  {
    return 1;
  }

  sort(files.begin()+1, files.end(), lex_string_sort);
  return 0;
}


/**
  @brief          Create table names list

  @details        The function creates the list of table names in
                  database

  @param[in]      session                   thread Cursor
  @param[in]      table_names           List of table names in database
  @param[in]      lex                   pointer to LEX struct
  @param[in]      lookup_field_vals     pointer to LOOKUP_FIELD_VALUE struct
  @param[in]      with_i_schema         true means that we add I_S tables to list
  @param[in]      db_name               database name

  @return         Operation status
    @retval       0           ok
    @retval       1           fatal error
    @retval       2           Not fatal error; Safe to ignore this file list
*/

static int
make_table_name_list(Session *session, vector<LEX_STRING*> &table_names,
                     LOOKUP_FIELD_VALUES *lookup_field_vals,
                     bool with_i_schema, LEX_STRING *db_name)
{
  char path[FN_REFLEN];
  set<string> set_of_names;

  build_table_filename(path, sizeof(path), db_name->str, "", false);

  if (!lookup_field_vals->wild_table_value &&
      lookup_field_vals->table_value.str)
  {
    if (with_i_schema)
    {
      if (plugin::InfoSchemaTable::getTable(lookup_field_vals->table_value.str))
      {
        table_names.push_back(&lookup_field_vals->table_value);
      }
    }
    else
    {
      table_names.push_back(&lookup_field_vals->table_value);
    }
    return 0;
  }

  string db(db_name->str);
  plugin::StorageEngine::getTableNames(db, set_of_names);

  /*  
    New I_S engine will make this go away, so ignore lack of foreach() usage.

    Notice how bad this design is... sure we created a set... but then we
    are just pushing to another set. --
    Also... callback design won't work, so we need to rewrite this to
    feed (which means new I_S). For the moment we will not optimize this.

  */
  for (set<string>::iterator it= set_of_names.begin(); it != set_of_names.end(); it++)
  {
    LEX_STRING *file_name= NULL;
    
    file_name= session->make_lex_string(file_name, (*it).c_str(),
                                        (*it).length(), true);
    const char* wild= lookup_field_vals->table_value.str;
    if (wild && wild_compare((*it).c_str(), wild, 0))
      continue;

    table_names.push_back(file_name);
  }

  return 0;
}


/**
  @brief          Fill I_S table for SHOW COLUMNS|INDEX commands

  @param[in]      session                      thread Cursor
  @param[in]      tables                   TableList for I_S table
  @param[in]      schema_table             pointer to I_S structure
  @param[in]      open_tables_state_backup pointer to Open_tables_state object
                                           which is used to save|restore original
                                           status of variables related to
                                           open tables state

  @return         Operation status
    @retval       0           success
    @retval       1           error
*/

static int
fill_schema_show_cols_or_idxs(Session *session, TableList *tables,
                              plugin::InfoSchemaTable *schema_table,
                              Open_tables_state *open_tables_state_backup)
{
  LEX *lex= session->lex;
  bool res;
  LEX_STRING tmp_lex_string, tmp_lex_string1, *db_name, *table_name;
  enum_sql_command save_sql_command= lex->sql_command;
  TableList *show_table_list= (TableList*) tables->schema_select_lex->
    table_list.first;
  Table *table= tables->table;
  int error= 1;

  lex->all_selects_list= tables->schema_select_lex;
  /*
    Restore session->temporary_tables to be able to process
    temporary tables(only for 'show index' & 'show columns').
    This should be changed when processing of temporary tables for
    I_S tables will be done.
  */
  session->temporary_tables= open_tables_state_backup->temporary_tables;
  /*
    Let us set fake sql_command so views won't try to merge
    themselves into main statement. If we don't do this,
    SELECT * from information_schema.xxxx will cause problems.
    SQLCOM_SHOW_FIELDS is used because it satisfies 'only_view_structure()'
  */
  lex->sql_command= SQLCOM_SHOW_FIELDS;
  res= session->openTables(show_table_list, DRIZZLE_LOCK_IGNORE_FLUSH);
  lex->sql_command= save_sql_command;
  /*
    get_all_tables() returns 1 on failure and 0 on success thus
    return only these and not the result code of ::process_table()

    We should use show_table_list->alias instead of
    show_table_list->table_name because table_name
    could be changed during opening of I_S tables. It's safe
    to use alias because alias contains original table name
    in this case(this part of code is used only for
    'show columns' & 'show statistics' commands).
  */
   table_name= session->make_lex_string(&tmp_lex_string1, show_table_list->alias,
                                    strlen(show_table_list->alias), false);
   db_name= session->make_lex_string(&tmp_lex_string, show_table_list->db,
                                 show_table_list->db_length, false);


   table->setWriteSet();
   error= test(schema_table->processTable(session, show_table_list,
                                          table, res, db_name,
                                          table_name));
   session->temporary_tables= 0;
   session->close_tables_for_reopen(&show_table_list);

   return(error);
}


/**
  @brief          Fill I_S table for SHOW Table NAMES commands

  @param[in]      session                      thread Cursor
  @param[in]      table                    Table struct for I_S table
  @param[in]      db_name                  database name
  @param[in]      table_name               table name
  @param[in]      with_i_schema            I_S table if true

  @return         Operation status
    @retval       0           success
    @retval       1           error
*/

static int fill_schema_table_names(Session *session, Table *table,
                                   LEX_STRING *db_name, LEX_STRING *table_name,
                                   bool with_i_schema)
{
  if (with_i_schema)
  {
    table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"),
                           system_charset_info);
  }
  else
  {
    char path[FN_REFLEN];
    (void) build_table_filename(path, sizeof(path), db_name->str,
                                table_name->str, false);

      table->field[3]->store(STRING_WITH_LEN("BASE Table"),
                             system_charset_info);

    if (session->is_error() && session->main_da.sql_errno() == ER_NO_SUCH_TABLE)
    {
      session->clear_error();
      return 0;
    }
  }
  if (schema_table_store_record(session, table))
    return 1;
  return 0;
}


/**
  @brief          Get open table method

  @details        The function calculates the method which will be used
                  for table opening:
                  SKIP_OPEN_TABLE - do not open table
                  OPEN_FRM_ONLY   - open FRM file only
                  OPEN_FULL_TABLE - open FRM, data, index files
  @param[in]      tables               I_S table table_list
  @param[in]      schema_table         I_S table struct

  @return         return a set of flags
    @retval       SKIP_OPEN_TABLE | OPEN_FRM_ONLY | OPEN_FULL_TABLE
*/

static uint32_t get_table_open_method(TableList *tables,
                                      plugin::InfoSchemaTable *schema_table)
{
  /*
    determine which method will be used for table opening
  */
  if (schema_table->getRequestedObject() & OPTIMIZE_I_S_TABLE)
  {
    Field **ptr, *field;
    int table_open_method= 0, field_indx= 0;
    for (ptr= tables->table->field; (field= *ptr) ; ptr++)
    {
      if (field->isReadSet())
        table_open_method|= schema_table->getColumnOpenMethod(field_indx);
      field_indx++;
    }
    return table_open_method;
  }
  /* I_S tables which use get_all_tables but can not be optimized */
  return (uint32_t) OPEN_FULL_TABLE;
}


/**
  @brief          Fill I_S table with data from FRM file only

  @param[in]      session                      thread Cursor
  @param[in]      table                    Table struct for I_S table
  @param[in]      schema_table             I_S table struct
  @param[in]      db_name                  database name
  @param[in]      table_name               table name

  @return         Operation status
    @retval       0           Table is processed and we can continue
                              with new table
    @retval       1           It's view and we have to use
                              open_tables function for this table
*/

static int fill_schema_table_from_frm(Session *session,TableList *tables,
                                      plugin::InfoSchemaTable *schema_table,
                                      LEX_STRING *db_name,
                                      LEX_STRING *table_name)
{
  Table *table= tables->table;
  TableShare *share;
  Table tbl;
  TableList table_list;
  uint32_t res= 0;
  int error;
  char key[MAX_DBKEY_LENGTH];
  uint32_t key_length;

  memset(&tbl, 0, sizeof(Table));

  table_list.table_name= table_name->str;
  table_list.db= db_name->str;

  key_length= table_list.create_table_def_key(key);
  pthread_mutex_lock(&LOCK_open); /* Locking to get table share when filling schema table from FRM */
  share= TableShare::getShare(session, &table_list, key, key_length, 0, &error);
  if (!share)
  {
    res= 0;
    goto err;
  }

  {
    tbl.s= share;
    table_list.table= &tbl;
    res= schema_table->processTable(session, &table_list, table,
                                    res, db_name, table_name);
  }
  /* For the moment we just set everything to read */
  table->setReadSet();

  TableShare::release(share);

err:
  pthread_mutex_unlock(&LOCK_open);
  session->clear_error();
  return res;
}



/**
  @brief          Fill I_S tables whose data are retrieved
                  from frm files and storage engine

  @details        The information schema tables are internally represented as
                  temporary tables that are filled at query execution time.
                  Those I_S tables whose data are retrieved
                  from frm files and storage engine are filled by the function
                  plugin::InfoSchemaMethods::fillTable().

  @param[in]      session                      thread Cursor
  @param[in]      tables                   I_S table
  @param[in]      cond                     'WHERE' condition

  @return         Operation status
    @retval       0                        success
    @retval       1                        error
*/
int plugin::InfoSchemaMethods::fillTable(Session *session, TableList *tables, COND *cond)
{
  LEX *lex= session->lex;
  Table *table= tables->table;
  Select_Lex *old_all_select_lex= lex->all_selects_list;
  enum_sql_command save_sql_command= lex->sql_command;
  Select_Lex *lsel= tables->schema_select_lex;
  plugin::InfoSchemaTable *schema_table= tables->schema_table;
  Select_Lex sel;
  LOOKUP_FIELD_VALUES lookup_field_vals;
  bool with_i_schema;
  vector<LEX_STRING*> db_names, table_names;
  COND *partial_cond= 0;
  uint32_t derived_tables= lex->derived_tables;
  int error= 1;
  Open_tables_state open_tables_state_backup;
  Query_tables_list query_tables_list_backup;
  uint32_t table_open_method;
  bool old_value= session->no_warnings_for_error;

  /*
    We should not introduce deadlocks even if we already have some
    tables open and locked, since we won't lock tables which we will
    open and will ignore possible name-locks for these tables.
  */
  session->reset_n_backup_open_tables_state(&open_tables_state_backup);

  tables->table_open_method= table_open_method=
    get_table_open_method(tables, schema_table);
  /*
    this branch processes SHOW FIELDS, SHOW INDEXES commands.
    see sql_parse.cc, prepare_schema_table() function where
    this values are initialized
  */
  if (lsel && lsel->table_list.first)
  {
    error= fill_schema_show_cols_or_idxs(session, tables, schema_table,
                                         &open_tables_state_backup);
    goto err;
  }

  if (get_lookup_field_values(session, cond, tables, &lookup_field_vals))
  {
    error= 0;
    goto err;
  }

  if (!lookup_field_vals.wild_db_value && !lookup_field_vals.wild_table_value)
  {
    /*
      if lookup value is empty string then
      it's impossible table name or db name
    */
    if ((lookup_field_vals.db_value.str && !lookup_field_vals.db_value.str[0]) ||
        (lookup_field_vals.table_value.str && !lookup_field_vals.table_value.str[0]))
    {
      error= 0;
      goto err;
    }
  }

  if (lookup_field_vals.db_value.length &&
      !lookup_field_vals.wild_db_value)
    tables->has_db_lookup_value= true;

  if (lookup_field_vals.table_value.length &&
      !lookup_field_vals.wild_table_value)
    tables->has_table_lookup_value= true;

  if (tables->has_db_lookup_value && tables->has_table_lookup_value)
    partial_cond= 0;
  else
    partial_cond= make_cond_for_info_schema(cond, tables);

  if (lex->describe)
  {
    /* EXPLAIN SELECT */
    error= 0;
    goto err;
  }

  table->setWriteSet();
  if (make_db_list(session, db_names, &lookup_field_vals, &with_i_schema))
    goto err;

  for (vector<LEX_STRING*>::iterator db_name= db_names.begin(); db_name != db_names.end(); ++db_name )
  {
    session->no_warnings_for_error= 1;
    table_names.clear();
    int res= make_table_name_list(session, table_names,
                                  &lookup_field_vals,
                                  with_i_schema, *db_name);

    if (res == 2)   /* Not fatal error, continue */
      continue;

    if (res)
      goto err;

    
    for (vector<LEX_STRING*>::iterator table_name= table_names.begin(); table_name != table_names.end(); ++table_name)
    {
      table->restoreRecordAsDefault();
      table->field[schema_table->getFirstColumnIndex()]->
        store((*db_name)->str, (*db_name)->length, system_charset_info);
      table->field[schema_table->getSecondColumnIndex()]->
        store((*table_name)->str, (*table_name)->length, system_charset_info);

      if (!partial_cond || partial_cond->val_int())
      {
        /*
          If table is I_S.tables and open_table_method is 0 (eg SKIP_OPEN)
          we can skip table opening and we don't have lookup value for
          table name or lookup value is wild string(table name list is
          already created by make_table_name_list() function).
        */
        if (! table_open_method &&
            schema_table->getTableName().compare("TABLES") == 0 &&
            (! lookup_field_vals.table_value.length ||
             lookup_field_vals.wild_table_value))
        {
          if (schema_table_store_record(session, table))
            goto err;      /* Out of space in temporary table */
          continue;
        }

        /* SHOW Table NAMES command */
        if (schema_table->getTableName().compare("TABLE_NAMES") == 0)
        {
          if (fill_schema_table_names(session, tables->table, *db_name,
                                      *table_name, with_i_schema))
            continue;
        }
        else
        {
          if (!(table_open_method & ~OPEN_FRM_ONLY) &&
              !with_i_schema)
          {
            if (!fill_schema_table_from_frm(session, tables, schema_table, *db_name,
                                            *table_name))
              continue;
          }

          LEX_STRING tmp_lex_string, orig_db_name;
          /*
            Set the parent lex of 'sel' because it is needed by
            sel.init_query() which is called inside make_table_list.
          */
          session->no_warnings_for_error= 1;
          sel.parent_lex= lex;
          /* db_name can be changed in make_table_list() func */
          if (!session->make_lex_string(&orig_db_name, (*db_name)->str,
                                        (*db_name)->length, false))
            goto err;

          if (make_table_list(session, &sel, *db_name, *table_name))
            goto err;

          TableList *show_table_list= (TableList*) sel.table_list.first;
          lex->all_selects_list= &sel;
          lex->derived_tables= 0;
          lex->sql_command= SQLCOM_SHOW_FIELDS;
          show_table_list->i_s_requested_object=
            schema_table->getRequestedObject();
          res= session->openTables(show_table_list, DRIZZLE_LOCK_IGNORE_FLUSH);
          lex->sql_command= save_sql_command;
          /*
            XXX->  show_table_list has a flag i_is_requested,
            and when it's set, openTables()
            can return an error without setting an error message
            in Session, which is a hack. This is why we have to
            check for res, then for session->is_error() only then
            for session->main_da.sql_errno().
          */
          if (res && session->is_error() &&
              session->main_da.sql_errno() == ER_NO_SUCH_TABLE)
          {
            /*
              Hide error for not existing table.
              This error can occur for example when we use
              where condition with db name and table name and this
              table does not exist.
            */
            res= 0;
            session->clear_error();
          }
          else
          {
            /*
              We should use show_table_list->alias instead of
              show_table_list->table_name because table_name
              could be changed during opening of I_S tables. It's safe
              to use alias because alias contains original table name
              in this case.
            */
            session->make_lex_string(&tmp_lex_string, show_table_list->alias,
                                     strlen(show_table_list->alias), false);
            res= schema_table->processTable(session, show_table_list, table,
                                            res, &orig_db_name,
                                            &tmp_lex_string);
            session->close_tables_for_reopen(&show_table_list);
          }
          assert(!lex->query_tables_own_last);
          if (res)
            goto err;
        }
      }
    }
    /*
      If we have information schema its always the first table and only
      the first table. Reset for other tables.
    */
    with_i_schema= 0;
  }

  error= 0;

err:
  session->restore_backup_open_tables_state(&open_tables_state_backup);
  lex->derived_tables= derived_tables;
  lex->all_selects_list= old_all_select_lex;
  lex->sql_command= save_sql_command;
  session->no_warnings_for_error= old_value;
  return(error);
}


/**
  @brief    Store field characteristics into appropriate I_S table columns

  @param[in]      table             I_S table
  @param[in]      field             processed field
  @param[in]      cs                I_S table charset
  @param[in]      offset            offset from beginning of table
                                    to DATE_TYPE column in I_S table

  @return         void
*/

static void store_column_type(Table *table, Field *field,
                              const CHARSET_INFO * const cs,
                              uint32_t offset)
{
  bool is_blob;
  int decimals, field_length;
  const char *tmp_buff;
  char column_type_buff[MAX_FIELD_WIDTH];
  String column_type(column_type_buff, sizeof(column_type_buff), cs);

  field->sql_type(column_type);
  /* DTD_IDENTIFIER column */
  table->field[offset + 7]->store(column_type.ptr(), column_type.length(), cs);
  table->field[offset + 7]->set_notnull();
  tmp_buff= strchr(column_type.ptr(), '(');
  /* DATA_TYPE column */
  table->field[offset]->store(column_type.ptr(),
                         (tmp_buff ? tmp_buff - column_type.ptr() :
                          column_type.length()), cs);
  is_blob= (field->type() == DRIZZLE_TYPE_BLOB);
  if (field->has_charset() || is_blob ||
      field->real_type() == DRIZZLE_TYPE_VARCHAR)  // For varbinary type
  {
    uint32_t octet_max_length= field->max_display_length();
    if (is_blob && octet_max_length != (uint32_t) 4294967295U)
      octet_max_length /= field->charset()->mbmaxlen;
    int64_t char_max_len= is_blob ?
      (int64_t) octet_max_length / field->charset()->mbminlen :
      (int64_t) octet_max_length / field->charset()->mbmaxlen;
    /* CHARACTER_MAXIMUM_LENGTH column*/
    table->field[offset + 1]->store(char_max_len, true);
    table->field[offset + 1]->set_notnull();
    /* CHARACTER_OCTET_LENGTH column */
    table->field[offset + 2]->store((int64_t) octet_max_length, true);
    table->field[offset + 2]->set_notnull();
  }

  /*
    Calculate field_length and decimals.
    They are set to -1 if they should not be set (we should return NULL)
  */

  decimals= field->decimals();
  switch (field->type()) {
  case DRIZZLE_TYPE_NEWDECIMAL:
    field_length= ((Field_new_decimal*) field)->precision;
    break;
  case DRIZZLE_TYPE_LONG:
  case DRIZZLE_TYPE_LONGLONG:
    field_length= field->max_display_length() - 1;
    break;
  case DRIZZLE_TYPE_DOUBLE:
    field_length= field->field_length;
    if (decimals == NOT_FIXED_DEC)
      decimals= -1;                           // return NULL
    break;
  default:
    field_length= decimals= -1;
    break;
  }

  /* NUMERIC_PRECISION column */
  if (field_length >= 0)
  {
    table->field[offset + 3]->store((int64_t) field_length, true);
    table->field[offset + 3]->set_notnull();
  }
  /* NUMERIC_SCALE column */
  if (decimals >= 0)
  {
    table->field[offset + 4]->store((int64_t) decimals, true);
    table->field[offset + 4]->set_notnull();
  }
  if (field->has_charset())
  {
    /* CHARACTER_SET_NAME column*/
    tmp_buff= field->charset()->csname;
    table->field[offset + 5]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[offset + 5]->set_notnull();
    /* COLLATION_NAME column */
    tmp_buff= field->charset()->name;
    table->field[offset + 6]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[offset + 6]->set_notnull();
  }
}


int plugin::InfoSchemaMethods::processTable(Session *session, TableList *tables,
				    Table *table, bool res,
				    LEX_STRING *db_name,
				    LEX_STRING *table_name) const
{
  LEX *lex= session->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NULL;
  const CHARSET_INFO * const cs= system_charset_info;
  Table *show_table;
  TableShare *show_table_share;
  Field **ptr, *field, *timestamp_field;
  int count;

  if (res)
  {
    if (lex->sql_command != SQLCOM_SHOW_FIELDS)
    {
      /*
        I.e. we are in SELECT FROM INFORMATION_SCHEMA.COLUMS
        rather than in SHOW COLUMNS
      */
      if (session->is_error())
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                     session->main_da.sql_errno(), session->main_da.message());
      session->clear_error();
      res= 0;
    }
    return(res);
  }

  show_table= tables->table;
  show_table_share= show_table->s;
  count= 0;

  if (tables->schema_table)
  {
    ptr= show_table->field;
    timestamp_field= show_table->timestamp_field;
  }
  else
  {
    ptr= show_table_share->field;
    timestamp_field= show_table_share->timestamp_field;
  }

  /* For the moment we just set everything to read */
  if (!show_table->read_set)
  {
    show_table->def_read_set.setAll();
    show_table->read_set= &show_table->def_read_set;
  }
  show_table->use_all_columns();               // Required for default

  for (; (field= *ptr) ; ptr++)
  {
    unsigned char *pos;
    char tmp[MAX_FIELD_WIDTH];
    String type(tmp,sizeof(tmp), system_charset_info);
    char *end;

    /* to satisfy 'field->val_str' ASSERTs */
    field->table= show_table;
    show_table->in_use= session;

    if (wild && wild[0] &&
        wild_case_compare(system_charset_info, field->field_name,wild))
      continue;

    count++;
    /* Get default row, with all NULL fields set to NULL */
    table->restoreRecordAsDefault();

    table->field[1]->store(db_name->str, db_name->length, cs);
    table->field[2]->store(table_name->str, table_name->length, cs);
    table->field[3]->store(field->field_name, strlen(field->field_name),
                           cs);
    table->field[4]->store((int64_t) count, true);

    if (get_field_default_value(timestamp_field, field, &type, 0))
    {
      table->field[5]->store(type.ptr(), type.length(), cs);
      table->field[5]->set_notnull();
    }
    pos=(unsigned char*) ((field->flags & NOT_NULL_FLAG) ?  "NO" : "YES");
    table->field[6]->store((const char*) pos,
                           strlen((const char*) pos), cs);
    store_column_type(table, field, cs, 7);

    pos=(unsigned char*) ((field->flags & PRI_KEY_FLAG) ? "PRI" :
                 (field->flags & UNIQUE_KEY_FLAG) ? "UNI" :
                 (field->flags & MULTIPLE_KEY_FLAG) ? "MUL":"");
    table->field[15]->store((const char*) pos,
                            strlen((const char*) pos), cs);

    end= tmp;
    if (field->unireg_check == Field::NEXT_NUMBER)
      table->field[16]->store(STRING_WITH_LEN("auto_increment"), cs);
    if (timestamp_field == field &&
        field->unireg_check != Field::TIMESTAMP_DN_FIELD)
      table->field[16]->store(STRING_WITH_LEN("on update CURRENT_TIMESTAMP"),
                              cs);
    table->field[18]->store(field->comment.str, field->comment.length, cs);
    {
      enum column_format_type column_format= (enum column_format_type)
        ((field->flags >> COLUMN_FORMAT_FLAGS) & COLUMN_FORMAT_MASK);
      pos=(unsigned char*)"Default";
      table->field[19]->store((const char*) pos,
                              strlen((const char*) pos), cs);
      pos=(unsigned char*)(column_format == COLUMN_FORMAT_TYPE_DEFAULT ? "Default" :
                   column_format == COLUMN_FORMAT_TYPE_FIXED ? "Fixed" :
                                                             "Dynamic");
      table->field[20]->store((const char*) pos,
                              strlen((const char*) pos), cs);
    }
    if (schema_table_store_record(session, table))
      return(1);
  }
  return(0);
}


Table *plugin::InfoSchemaMethods::createSchemaTable(Session *session, TableList *table_list)
  const
{
  int field_count= 0;
  Item *item;
  Table *table;
  List<Item> field_list;
  const CHARSET_INFO * const cs= system_charset_info;
  const plugin::InfoSchemaTable::Columns &columns= table_list->schema_table->getColumns();
  plugin::InfoSchemaTable::Columns::const_iterator iter= columns.begin();

  while (iter != columns.end())
  {
    const plugin::ColumnInfo *column= *iter;
    switch (column->getType()) {
    case DRIZZLE_TYPE_LONG:
    case DRIZZLE_TYPE_LONGLONG:
      if (!(item= new Item_return_int(column->getName().c_str(),
                                      column->getLength(),
                                      column->getType(),
                                      column->getValue())))
      {
        return(0);
      }
      item->unsigned_flag= (column->getFlags() & MY_I_S_UNSIGNED);
      break;
    case DRIZZLE_TYPE_DATE:
    case DRIZZLE_TYPE_TIMESTAMP:
    case DRIZZLE_TYPE_DATETIME:
      if (!(item=new Item_return_date_time(column->getName().c_str(),
                                           column->getType())))
      {
        return(0);
      }
      break;
    case DRIZZLE_TYPE_DOUBLE:
      if ((item= new Item_float(column->getName().c_str(), 0.0, NOT_FIXED_DEC,
                           column->getLength())) == NULL)
        return NULL;
      break;
    case DRIZZLE_TYPE_NEWDECIMAL:
      if (!(item= new Item_decimal((int64_t) column->getValue(), false)))
      {
        return(0);
      }
      item->unsigned_flag= (column->getFlags() & MY_I_S_UNSIGNED);
      item->decimals= column->getLength() % 10;
      item->max_length= (column->getLength()/100)%100;
      if (item->unsigned_flag == 0)
        item->max_length+= 1;
      if (item->decimals > 0)
        item->max_length+= 1;
      item->set_name(column->getName().c_str(),
                     column->getName().length(), cs);
      break;
    case DRIZZLE_TYPE_BLOB:
      if (!(item= new Item_blob(column->getName().c_str(),
                                column->getLength())))
      {
        return(0);
      }
      break;
    default:
      if (!(item= new Item_empty_string("", column->getLength(), cs)))
      {
        return(0);
      }
      item->set_name(column->getName().c_str(),
                     column->getName().length(), cs);
      break;
    }
    field_list.push_back(item);
    item->maybe_null= (column->getFlags() & MY_I_S_MAYBE_NULL);
    field_count++;
    ++iter;
  }
  Tmp_Table_Param *tmp_table_param =
    (Tmp_Table_Param*) (session->alloc(sizeof(Tmp_Table_Param)));
  tmp_table_param->init();
  tmp_table_param->table_charset= cs;
  tmp_table_param->field_count= field_count;
  tmp_table_param->schema_table= 1;
  Select_Lex *select_lex= session->lex->current_select;
  if (!(table= create_tmp_table(session, tmp_table_param,
                                field_list, (order_st*) 0, 0, 0,
                                (select_lex->options | session->options |
                                 TMP_TABLE_ALL_COLUMNS),
                                HA_POS_ERROR, table_list->alias)))
    return(0);
  my_bitmap_map* bitmaps=
    (my_bitmap_map*) session->alloc(bitmap_buffer_size(field_count));
  table->def_read_set.init((my_bitmap_map*) bitmaps, field_count);
  table->read_set= &table->def_read_set;
  table->read_set->clearAll();
  table_list->schema_table_param= tmp_table_param;
  return(table);
}


/*
  For old SHOW compatibility. It is used when
  old SHOW doesn't have generated column names
  Make list of fields for SHOW

  SYNOPSIS
    plugin::InfoSchemaMethods::oldFormat()
    session			thread Cursor
    schema_table        pointer to 'schema_tables' element

  RETURN
   1	error
   0	success
*/

int plugin::InfoSchemaMethods::oldFormat(Session *session, plugin::InfoSchemaTable *schema_table)
  const
{
  Name_resolution_context *context= &session->lex->select_lex.context;
  const plugin::InfoSchemaTable::Columns columns= schema_table->getColumns();
  plugin::InfoSchemaTable::Columns::const_iterator iter= columns.begin();

  while (iter != columns.end())
  {
    const plugin::ColumnInfo *column= *iter;
    if (column->getOldName().length() != 0)
    {
      Item_field *field= new Item_field(context,
                                        NULL, NULL,
                                        column->getName().c_str());
      if (field)
      {
        field->set_name(column->getOldName().c_str(),
                        column->getOldName().length(),
                        system_charset_info);
        if (session->add_item_to_list(field))
          return 1;
      }
    }
    ++iter;
  }
  return 0;
}


/*
  Create information_schema table

  SYNOPSIS
  mysql_schema_table()
    session                thread Cursor
    lex                pointer to LEX
    table_list         pointer to table_list

  RETURN
    true on error
*/

bool mysql_schema_table(Session *session, LEX *, TableList *table_list)
{
  Table *table;
  if (!(table= table_list->schema_table->createSchemaTable(session, table_list)))
    return true;
  table->s->tmp_table= SYSTEM_TMP_TABLE;
  /*
    This test is necessary to make
    case insensitive file systems +
    upper case table names(information schema tables) +
    views
    working correctly
  */
  if (table_list->schema_table_name)
    table->alias_name_used= my_strcasecmp(table_alias_charset,
                                          table_list->schema_table_name,
                                          table_list->alias);
  table_list->table_name= table->s->table_name.str;
  table_list->table_name_length= table->s->table_name.length;
  table_list->table= table;
  table->next= session->derived_tables;
  session->derived_tables= table;
  table_list->select_lex->options |= OPTION_SCHEMA_TABLE;

  return false;
}


/*
  Generate select from information_schema table

  SYNOPSIS
    make_schema_select()
    session                  thread Cursor
    sel                  pointer to Select_Lex
    schema_table_name    name of 'schema_tables' element

  RETURN
    true on error
*/

bool make_schema_select(Session *session, Select_Lex *sel,
                        const string& schema_table_name)
{
  plugin::InfoSchemaTable *schema_table= plugin::InfoSchemaTable::getTable(schema_table_name.c_str());
  LEX_STRING db, table;
  /*
     We have to make non const db_name & table_name
     because of lower_case_table_names
  */
  session->make_lex_string(&db, INFORMATION_SCHEMA_NAME.c_str(),
                       INFORMATION_SCHEMA_NAME.length(), 0);
  session->make_lex_string(&table, schema_table->getTableName().c_str(),
                           schema_table->getTableName().length(), 0);
  if (schema_table->oldFormat(session, schema_table) ||   /* Handle old syntax */
      ! sel->add_table_to_list(session, new Table_ident(db, table), 0, 0, TL_READ))
  {
    return true;
  }
  return false;
}


/*
  Fill temporary schema tables before SELECT

  SYNOPSIS
    get_schema_tables_result()
    join  join which use schema tables
    executed_place place where I_S table processed

  RETURN
    false success
    true  error
*/

bool get_schema_tables_result(JOIN *join,
                              enum enum_schema_table_state executed_place)
{
  JoinTable *tmp_join_tab= join->join_tab+join->tables;
  Session *session= join->session;
  LEX *lex= session->lex;
  bool result= 0;

  session->no_warnings_for_error= 1;
  for (JoinTable *tab= join->join_tab; tab < tmp_join_tab; tab++)
  {
    if (!tab->table || !tab->table->pos_in_table_list)
      break;

    TableList *table_list= tab->table->pos_in_table_list;
    if (table_list->schema_table)
    {
      bool is_subselect= (&lex->unit != lex->current_select->master_unit() &&
                          lex->current_select->master_unit()->item);


      /* skip I_S optimizations specific to get_all_tables */
      if (session->lex->describe &&
          (table_list->schema_table->isOptimizationPossible() != true))
      {
        continue;
      }

      /*
        If schema table is already processed and
        the statement is not a subselect then
        we don't need to fill this table again.
        If schema table is already processed and
        schema_table_state != executed_place then
        table is already processed and
        we should skip second data processing.
      */
      if (table_list->schema_table_state &&
          (!is_subselect || table_list->schema_table_state != executed_place))
        continue;

      /*
        if table is used in a subselect and
        table has been processed earlier with the same
        'executed_place' value then we should refresh the table.
      */
      if (table_list->schema_table_state && is_subselect)
      {
        table_list->table->file->extra(HA_EXTRA_NO_CACHE);
        table_list->table->file->extra(HA_EXTRA_RESET_STATE);
        table_list->table->file->ha_delete_all_rows();
        table_list->table->free_io_cache();
        table_list->table->filesort_free_buffers(true);
        table_list->table->null_row= 0;
      }
      else
        table_list->table->file->stats.records= 0;

      if (table_list->schema_table->fillTable(session, table_list,
                                               tab->select_cond))
      {
        result= 1;
        join->error= 1;
        tab->read_record.file= table_list->table->file;
        table_list->schema_table_state= executed_place;
        break;
      }
      tab->read_record.file= table_list->table->file;
      table_list->schema_table_state= executed_place;
    }
  }
  session->no_warnings_for_error= 0;
  return(result);
}
