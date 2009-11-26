/* Copyright (C) 2000-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* drop and alter of tables */

#include <drizzled/server_includes.h>
#include <plugin/myisam/myisam.h>
#include <drizzled/show.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/data_home.h>
#include <drizzled/sql_parse.h>
#include <mysys/hash.h>
#include <drizzled/sql_lex.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/db.h>
#include <drizzled/lock.h>
#include <drizzled/unireg.h>
#include <drizzled/item/int.h>
#include <drizzled/item/empty_string.h>
#include <drizzled/replication_services.h>
#include <drizzled/table_proto.h>
#include <drizzled/plugin/client.h>
#include <drizzled/table_identifier.h>

#include "drizzled/statement/alter_table.h" /* for drizzled::create_like_schema_frm, which will die soon */

#include <algorithm>

using namespace std;
using namespace drizzled;

static const char hexchars[]= "0123456789abcdef";
bool is_primary_key(KEY *key_info)
{
  static const char * primary_key_name="PRIMARY";
  return (strcmp(key_info->name, primary_key_name)==0);
}

const char* is_primary_key_name(const char* key_name)
{
  static const char * primary_key_name="PRIMARY";
  if (strcmp(key_name, primary_key_name)==0)
    return key_name;
  else
    return NULL;
}

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static char *make_unique_key_name(const char *field_name,KEY *start,KEY *end);

static bool prepare_blob_field(Session *session, CreateField *sql_field);

void set_table_default_charset(HA_CREATE_INFO *create_info, const char *db)
{
  /*
    If the table character set was not given explicitly,
    let's fetch the database default character set and
    apply it to the table.
  */
  if (create_info->default_table_charset == NULL)
    create_info->default_table_charset= get_default_db_collation(db);
}

/*
  Translate a cursor name to a table name (WL #1324).

  SYNOPSIS
    filename_to_tablename()
      from                      The cursor name
      to                OUT     The table name
      to_length                 The size of the table name buffer.

  RETURN
    Table name length.
*/
uint32_t filename_to_tablename(const char *from, char *to, uint32_t to_length)
{
  uint32_t length= 0;

  if (!memcmp(from, TMP_FILE_PREFIX, TMP_FILE_PREFIX_LENGTH))
  {
    /* Temporary table name. */
    length= strlen(strncpy(to, from, to_length));
  }
  else
  {
    for (; *from  && length < to_length; length++, from++)
    {
      if (*from != '@')
      {
        to[length]= *from;
        continue;
      }
      /* We've found an escaped char - skip the @ */
      from++;
      to[length]= 0;
      /* There will be a two-position hex-char version of the char */
      for (int x=1; x >= 0; x--)
      {
        if (*from >= '0' && *from <= '9')
          to[length] += ((*from++ - '0') << (4 * x));
        else if (*from >= 'a' && *from <= 'f')
          to[length] += ((*from++ - 'a' + 10) << (4 * x));
      }
      /* Backup because we advanced extra in the inner loop */
      from--;
    } 
  }

  return length;
}


/*
  Translate a table name to a cursor name (WL #1324).

  SYNOPSIS
    tablename_to_filename()
      from                      The table name
      to                OUT     The cursor name
      to_length                 The size of the cursor name buffer.

  RETURN
    true if errors happen. false on success.
*/
bool tablename_to_filename(const char *from, char *to, size_t to_length)
{
  
  size_t length= 0;
  for (; *from  && length < to_length; length++, from++)
  {
    if ((*from >= '0' && *from <= '9') ||
        (*from >= 'A' && *from <= 'Z') ||
        (*from >= 'a' && *from <= 'z') ||
/* OSX defines an extra set of high-bit and multi-byte characters
   that cannot be used on the filesystem. Instead of trying to sort
   those out, we'll just escape encode all high-bit-set chars on OSX.
   It won't really hurt anything - it'll just make some filenames ugly. */
#if !defined(TARGET_OS_OSX)
        ((unsigned char)*from >= 128) ||
#endif
        (*from == '_') ||
        (*from == ' ') ||
        (*from == '-'))
    {
      to[length]= *from;
      continue;
    }
   
    if (length + 3 >= to_length)
      return true;

    /* We need to escape this char in a way that can be reversed */
    to[length++]= '@';
    to[length++]= hexchars[(*from >> 4) & 15];
    to[length]= hexchars[(*from) & 15];
  }

  if (check_if_legal_tablename(to) &&
      length + 4 < to_length)
  {
    memcpy(to + length, "@@@", 4);
    length+= 3;
  }
  return false;
}


/*
  Creates path to a cursor: drizzle_data_dir/db/table.ext

  SYNOPSIS
   build_table_filename()
     buff                       Where to write result
                                This may be the same as table_name.
     bufflen                    buff size
     db                         Database name
     table_name                 Table name
     ext                        File extension.
     flags                      FN_FROM_IS_TMP or FN_TO_IS_TMP
                                table_name is temporary, do not change.

  NOTES

    Uses database and table name, and extension to create
    a cursor name in drizzle_data_dir. Database and table
    names are converted from system_charset_info into "fscs".
    Unless flags indicate a temporary table name.
    'db' is always converted.
    'ext' is not converted.

    The conversion suppression is required for ALTER Table. This
    statement creates intermediate tables. These are regular
    (non-temporary) tables with a temporary name. Their path names must
    be derivable from the table name. So we cannot use
    build_tmptable_filename() for them.

  RETURN
    path length on success, 0 on failure
*/

size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp)
{
  char dbbuff[FN_REFLEN];
  char tbbuff[FN_REFLEN];
  bool conversion_error= false;

  memset(tbbuff, 0, sizeof(tbbuff));
  if (is_tmp) // FN_FROM_IS_TMP | FN_TO_IS_TMP
    strncpy(tbbuff, table_name, sizeof(tbbuff));
  else
  {
    conversion_error= tablename_to_filename(table_name, tbbuff, sizeof(tbbuff));
    if (conversion_error)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Table name cannot be encoded and fit within filesystem "
                      "name length restrictions."));
      return 0;
    }
  }
  memset(dbbuff, 0, sizeof(dbbuff));
  conversion_error= tablename_to_filename(db, dbbuff, sizeof(dbbuff));
  if (conversion_error)
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Schema name cannot be encoded and fit within filesystem "
                    "name length restrictions."));
    return 0;
  }
   

  int rootdir_len= strlen(FN_ROOTDIR);
  string table_path(drizzle_data_home);
  int without_rootdir= table_path.length()-rootdir_len;

  /* Don't add FN_ROOTDIR if dirzzle_data_home already includes it */
  if (without_rootdir >= 0)
  {
    const char *tmp= table_path.c_str()+without_rootdir;
    if (memcmp(tmp, FN_ROOTDIR, rootdir_len) != 0)
      table_path.append(FN_ROOTDIR);
  }

  table_path.append(dbbuff);
  table_path.append(FN_ROOTDIR);
  table_path.append(tbbuff);

  if (bufflen < table_path.length())
    return 0;

  strcpy(buff, table_path.c_str());

  return table_path.length();
}


/*
  Creates path to a cursor: drizzle_tmpdir/#sql1234_12_1.ext

  SYNOPSIS
   build_tmptable_filename()
     session                    The thread handle.
     buff                       Where to write result
     bufflen                    buff size

  NOTES

    Uses current_pid, thread_id, and tmp_table counter to create
    a cursor name in drizzle_tmpdir.

  RETURN
    path length on success, 0 on failure
*/

size_t build_tmptable_filename(char *buff, size_t bufflen)
{
  size_t length;
  ostringstream path_str, post_tmpdir_str;
  string tmp;

  Session *session= current_session;

  path_str << drizzle_tmpdir;
  post_tmpdir_str << "/" << TMP_FILE_PREFIX << current_pid;
  post_tmpdir_str << session->thread_id << session->tmp_table++;
  tmp= post_tmpdir_str.str();

  transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);

  path_str << tmp;

  if (bufflen < path_str.str().length())
    length= 0;
  else
    length= unpack_filename(buff, path_str.str().c_str());

  return length;
}

/*
  SYNOPSIS
    write_bin_log()
    session                           Thread object
    query                         Query to log
    query_length                  Length of query

  RETURN VALUES
    NONE

  DESCRIPTION
    Write the binlog if open, routine used in multiple places in this
    cursor
*/

void write_bin_log(Session *session,
                   char const *query, size_t query_length)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  replication_services.rawStatement(session, query, query_length);
}


/* Should should be refactored to go away */
void write_bin_log_drop_table(Session *session, bool if_exists, const char *db_name, const char *table_name)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  string built_query;

  if (if_exists)
    built_query.append("DROP TABLE IF EXISTS ");
  else
    built_query.append("DROP TABLE ");

  built_query.append("`");
  if (session->db.empty() || strcmp(db_name, session->db.c_str()) != 0)
  {
    built_query.append(db_name);
    built_query.append("`.`");
  }

  built_query.append(table_name);
  built_query.append("`");
  replication_services.rawStatement(session, built_query.c_str(), built_query.length());
}

/*
  Execute the drop of a normal or temporary table

  SYNOPSIS
    mysql_rm_table_part2()
    session			Thread Cursor
    tables		Tables to drop
    if_exists		If set, don't give an error if table doesn't exists.
			In this case we give an warning of level 'NOTE'
    drop_temporary	Only drop temporary tables

  TODO:
    When logging to the binary log, we should log
    tmp_tables and transactional tables as separate statements if we
    are in a transaction;  This is needed to get these tables into the
    cached binary log that is only written on COMMIT.

   The current code only writes DROP statements that only uses temporary
   tables to the cache binary log.  This should be ok on most cases, but
   not all.

 RETURN
   0	ok
   1	Error
   -1	Thread was killed
*/

int mysql_rm_table_part2(Session *session, TableList *tables, bool if_exists,
                         bool drop_temporary)
{
  TableList *table;
  char path[FN_REFLEN];
  uint32_t path_length= 0;
  String wrong_tables;
  int error= 0;
  bool foreign_key_error= false;

  pthread_mutex_lock(&LOCK_open); /* Part 2 of rm a table */

  /*
    If we have the table in the definition cache, we don't have to check the
    .frm cursor to find if the table is a normal table (not view) and what
    engine to use.
  */

  for (table= tables; table; table= table->next_local)
  {
    TableShare *share;
    table->db_type= NULL;
    if ((share= TableShare::getShare(table->db, table->table_name)))
      table->db_type= share->db_type();
  }

  if (!drop_temporary && lock_table_names_exclusively(session, tables))
  {
    pthread_mutex_unlock(&LOCK_open);
    return 1;
  }

  /* Don't give warnings for not found errors, as we already generate notes */
  session->no_warnings_for_error= 1;

  for (table= tables; table; table= table->next_local)
  {
    char *db=table->db;
    plugin::StorageEngine *table_type;

    error= session->drop_temporary_table(table);

    switch (error) {
    case  0:
      // removed temporary table
      continue;
    case -1:
      error= 1;
      goto err_with_placeholders;
    default:
      // temporary table not found
      error= 0;
    }

    table_type= table->db_type;
    if (drop_temporary == false)
    {
      Table *locked_table;
      abort_locked_tables(session, db, table->table_name);
      remove_table_from_cache(session, db, table->table_name,
                              RTFC_WAIT_OTHER_THREAD_FLAG |
                              RTFC_CHECK_KILLED_FLAG);
      /*
        If the table was used in lock tables, remember it so that
        unlock_table_names can free it
      */
      if ((locked_table= drop_locked_tables(session, db, table->table_name)))
        table->table= locked_table;

      if (session->killed)
      {
        error= -1;
        goto err_with_placeholders;
      }
      /* remove .frm cursor and engine files */
      path_length= build_table_filename(path, sizeof(path), db, table->table_name, table->internal_tmp_table);
    }
    TableIdentifier identifier(db, table->table_name, table->internal_tmp_table ? INTERNAL_TMP_TABLE : NO_TMP_TABLE);

    if (drop_temporary ||
        ((table_type == NULL
          && (plugin::StorageEngine::getTableDefinition(*session,
                                                        path,
                                                        db,
                                                        table->table_name,
                                                        table->internal_tmp_table) != EEXIST))))
    {
      // Table was not found on disk and table can't be created from engine
      if (if_exists)
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
                            table->table_name);
      else
        error= 1;
    }
    else
    {
      error= plugin::StorageEngine::dropTable(*session,
                                              identifier,
                                              true);

      if ((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE) && if_exists)
      {
	error= 0;
        session->clear_error();
      }

      if (error == HA_ERR_ROW_IS_REFERENCED)
      {
        /* the table is referenced by a foreign key constraint */
        foreign_key_error= true;
      }
    }

    if (error == 0 || (if_exists && foreign_key_error == false))
        write_bin_log_drop_table(session, if_exists, db, table->table_name);

    if (error)
    {
      if (wrong_tables.length())
        wrong_tables.append(',');
      wrong_tables.append(String(table->table_name,system_charset_info));
    }
  }
  /*
    It's safe to unlock LOCK_open: we have an exclusive lock
    on the table name.
  */
  pthread_mutex_unlock(&LOCK_open);
  error= 0;
  if (wrong_tables.length())
  {
    if (!foreign_key_error)
      my_printf_error(ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR), MYF(0),
                      wrong_tables.c_ptr());
    else
    {
      my_message(ER_ROW_IS_REFERENCED, ER(ER_ROW_IS_REFERENCED), MYF(0));
    }
    error= 1;
  }

  pthread_mutex_lock(&LOCK_open); /* final bit in rm table lock */
err_with_placeholders:
  unlock_table_names(tables, NULL);
  pthread_mutex_unlock(&LOCK_open);
  session->no_warnings_for_error= 0;

  return(error);
}


/*
  Quickly remove a table.

  SYNOPSIS
    quick_rm_table()
      base                      The plugin::StorageEngine handle.
      db                        The database name.
      table_name                The table name.
      is_tmp                    If the table is temp.

  RETURN
    0           OK
    != 0        Error
*/

bool quick_rm_table(Session& session, const char *db,
                    const char *table_name, bool is_tmp)
{
  bool error= 0;

  TableIdentifier identifier(db, table_name, is_tmp ? INTERNAL_TMP_TABLE : NO_TMP_TABLE);

  return (plugin::StorageEngine::dropTable(session, identifier, false)
          || error);
}

/*
  Sort keys in the following order:
  - PRIMARY KEY
  - UNIQUE keys where all column are NOT NULL
  - UNIQUE keys that don't contain partial segments
  - Other UNIQUE keys
  - Normal keys
  - Fulltext keys

  This will make checking for duplicated keys faster and ensure that
  PRIMARY keys are prioritized.
*/

static int sort_keys(KEY *a, KEY *b)
{
  ulong a_flags= a->flags, b_flags= b->flags;

  if (a_flags & HA_NOSAME)
  {
    if (!(b_flags & HA_NOSAME))
      return -1;
    if ((a_flags ^ b_flags) & (HA_NULL_PART_KEY))
    {
      /* Sort NOT NULL keys before other keys */
      return (a_flags & (HA_NULL_PART_KEY)) ? 1 : -1;
    }
    if (is_primary_key(a))
      return -1;
    if (is_primary_key(b))
      return 1;
    /* Sort keys don't containing partial segments before others */
    if ((a_flags ^ b_flags) & HA_KEY_HAS_PART_KEY_SEG)
      return (a_flags & HA_KEY_HAS_PART_KEY_SEG) ? 1 : -1;
  }
  else if (b_flags & HA_NOSAME)
    return 1;					// Prefer b

  /*
    Prefer original key order.	usable_key_parts contains here
    the original key position.
  */
  return ((a->usable_key_parts < b->usable_key_parts) ? -1 :
          (a->usable_key_parts > b->usable_key_parts) ? 1 :
          0);
}

/*
  Check TYPELIB (set or enum) for duplicates

  SYNOPSIS
    check_duplicates_in_interval()
    set_or_name   "SET" or "ENUM" string for warning message
    name	  name of the checked column
    typelib	  list of values for the column
    dup_val_count  returns count of duplicate elements

  DESCRIPTION
    This function prints an warning for each value in list
    which has some duplicates on its right

  RETURN VALUES
    0             ok
    1             Error
*/

static bool check_duplicates_in_interval(const char *set_or_name,
                                         const char *name, TYPELIB *typelib,
                                         const CHARSET_INFO * const cs,
                                         unsigned int *dup_val_count)
{
  TYPELIB tmp= *typelib;
  const char **cur_value= typelib->type_names;
  unsigned int *cur_length= typelib->type_lengths;
  *dup_val_count= 0;

  for ( ; tmp.count > 1; cur_value++, cur_length++)
  {
    tmp.type_names++;
    tmp.type_lengths++;
    tmp.count--;
    if (find_type2(&tmp, (const char*)*cur_value, *cur_length, cs))
    {
      my_error(ER_DUPLICATED_VALUE_IN_TYPE, MYF(0),
               name,*cur_value,set_or_name);
      return 1;
    }
  }
  return 0;
}


/*
  Check TYPELIB (set or enum) max and total lengths

  SYNOPSIS
    calculate_interval_lengths()
    cs            charset+collation pair of the interval
    typelib       list of values for the column
    max_length    length of the longest item
    tot_length    sum of the item lengths

  DESCRIPTION
    After this function call:
    - ENUM uses max_length
    - SET uses tot_length.

  RETURN VALUES
    void
*/
static void calculate_interval_lengths(const CHARSET_INFO * const cs,
                                       TYPELIB *interval,
                                       uint32_t *max_length,
                                       uint32_t *tot_length)
{
  const char **pos;
  uint32_t *len;
  *max_length= *tot_length= 0;
  for (pos= interval->type_names, len= interval->type_lengths;
       *pos ; pos++, len++)
  {
    uint32_t length= cs->cset->numchars(cs, *pos, *pos + *len);
    *tot_length+= length;
    set_if_bigger(*max_length, (uint32_t)length);
  }
}

/*
  Prepare a create_table instance for packing

  SYNOPSIS
    prepare_create_field()
    sql_field     field to prepare for packing
    blob_columns  count for BLOBs
    timestamps    count for timestamps
    table_flags   table flags

  DESCRIPTION
    This function prepares a CreateField instance.
    Fields such as pack_flag are valid after this call.

  RETURN VALUES
   0	ok
   1	Error
*/
int prepare_create_field(CreateField *sql_field,
                         uint32_t *blob_columns,
                         int *timestamps, int *timestamps_with_niladic,
                         int64_t )
{
  unsigned int dup_val_count;

  /*
    This code came from mysql_prepare_create_table.
    Indent preserved to make patching easier
  */
  assert(sql_field->charset);

  switch (sql_field->sql_type) {
  case DRIZZLE_TYPE_BLOB:
    sql_field->pack_flag= pack_length_to_packflag(sql_field->pack_length - portable_sizeof_char_ptr);
    sql_field->length= 8; // Unireg field length
    (*blob_columns)++;
    break;
  case DRIZZLE_TYPE_VARCHAR:
    sql_field->pack_flag=0;
    break;
  case DRIZZLE_TYPE_ENUM:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length);
    if (check_duplicates_in_interval("ENUM",
                                     sql_field->field_name,
                                     sql_field->interval,
                                     sql_field->charset,
                                     &dup_val_count))
      return 1;
    break;
  case DRIZZLE_TYPE_DATE:  // Rest of string types
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_NULL:
    sql_field->pack_flag=f_settype((uint32_t) sql_field->sql_type);
    break;
  case DRIZZLE_TYPE_DECIMAL:
    sql_field->pack_flag= 0;
    break;
  case DRIZZLE_TYPE_TIMESTAMP:
    /* We should replace old TIMESTAMP fields with their newer analogs */
    if (sql_field->unireg_check == Field::TIMESTAMP_OLD_FIELD)
    {
      if (!*timestamps)
      {
        sql_field->unireg_check= Field::TIMESTAMP_DNUN_FIELD;
        (*timestamps_with_niladic)++;
      }
      else
        sql_field->unireg_check= Field::NONE;
    }
    else if (sql_field->unireg_check != Field::NONE)
      (*timestamps_with_niladic)++;

    (*timestamps)++;
    /* fall-through */
  default:
    sql_field->pack_flag=(0 |
                          f_settype((uint32_t) sql_field->sql_type));
    break;
  }
  return 0;
}

int mysql_prepare_create_table(Session *session,
                               HA_CREATE_INFO *create_info,
                               message::Table *create_proto,
                               AlterInfo *alter_info,
                               bool tmp_table,
                               uint32_t *db_options,
                               Cursor *cursor,
                               KEY **key_info_buffer,
                               uint32_t *key_count,
                               int select_field_count)
{
  const char	*key_name;
  CreateField	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  ulong		record_offset= 0;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  List_iterator<CreateField> it(alter_info->create_list);
  List_iterator<CreateField> it2(alter_info->create_list);
  uint32_t total_uneven_bit_length= 0;

  select_field_pos= alter_info->create_list.elements - select_field_count;
  null_fields=blob_columns=0;
  max_key_length= cursor->max_key_length();

  for (field_no=0; (sql_field=it++) ; field_no++)
  {
    const CHARSET_INFO *save_cs;

    /*
      Initialize length from its original value (number of characters),
      which was set in the parser. This is necessary if we're
      executing a prepared statement for the second time.
    */
    sql_field->length= sql_field->char_length;
    if (!sql_field->charset)
      sql_field->charset= create_info->default_table_charset;
    /*
      table_charset is set in ALTER Table if we want change character set
      for all varchar/char columns.
      But the table charset must not affect the BLOB fields, so don't
      allow to change my_charset_bin to somethig else.
    */
    if (create_info->table_charset && sql_field->charset != &my_charset_bin)
      sql_field->charset= create_info->table_charset;

    save_cs= sql_field->charset;
    if ((sql_field->flags & BINCMP_FLAG) &&
        !(sql_field->charset= get_charset_by_csname(sql_field->charset->csname, MY_CS_BINSORT)))
    {
      char tmp[64];
      char *tmp_pos= tmp;
      strncpy(tmp_pos, save_cs->csname, sizeof(tmp)-4);
      tmp_pos+= strlen(tmp);
      strncpy(tmp_pos, STRING_WITH_LEN("_bin"));
      my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
      return(true);
    }

    /*
      Convert the default value from client character
      set into the column character set if necessary.
    */
    if (sql_field->def &&
        save_cs != sql_field->def->collation.collation &&
        (sql_field->sql_type == DRIZZLE_TYPE_ENUM))
    {
      /*
        Starting from 5.1 we work here with a copy of CreateField
        created by the caller, not with the instance that was
        originally created during parsing. It's OK to create
        a temporary item and initialize with it a member of the
        copy -- this item will be thrown away along with the copy
        at the end of execution, and thus not introduce a dangling
        pointer in the parsed tree of a prepared statement or a
        stored procedure statement.
      */
      sql_field->def= sql_field->def->safe_charset_converter(save_cs);

      if (sql_field->def == NULL)
      {
        /* Could not convert */
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        return(true);
      }
    }

    if (sql_field->sql_type == DRIZZLE_TYPE_ENUM)
    {
      uint32_t dummy;
      const CHARSET_INFO * const cs= sql_field->charset;
      TYPELIB *interval= sql_field->interval;

      /*
        Create typelib from interval_list, and if necessary
        convert strings from client character set to the
        column character set.
      */
      if (!interval)
      {
        /*
          Create the typelib in runtime memory - we will free the
          occupied memory at the same time when we free this
          sql_field -- at the end of execution.
        */
        interval= sql_field->interval= typelib(session->mem_root,
                                               sql_field->interval_list);

        List_iterator<String> int_it(sql_field->interval_list);
        String conv, *tmp;
        char comma_buf[4];
        int comma_length= cs->cset->wc_mb(cs, ',', (unsigned char*) comma_buf,
                                          (unsigned char*) comma_buf +
                                          sizeof(comma_buf));
        assert(comma_length > 0);

        for (uint32_t i= 0; (tmp= int_it++); i++)
        {
          uint32_t lengthsp;
          if (String::needs_conversion(tmp->length(), tmp->charset(),
                                       cs, &dummy))
          {
            uint32_t cnv_errs;
            conv.copy(tmp->ptr(), tmp->length(), tmp->charset(), cs, &cnv_errs);
            interval->type_names[i]= strmake_root(session->mem_root, conv.ptr(),
                                                  conv.length());
            interval->type_lengths[i]= conv.length();
          }

          // Strip trailing spaces.
          lengthsp= cs->cset->lengthsp(cs, interval->type_names[i],
                                       interval->type_lengths[i]);
          interval->type_lengths[i]= lengthsp;
          ((unsigned char *)interval->type_names[i])[lengthsp]= '\0';
        }
        sql_field->interval_list.empty(); // Don't need interval_list anymore
      }

      /* DRIZZLE_TYPE_ENUM */
      {
        uint32_t field_length;
        assert(sql_field->sql_type == DRIZZLE_TYPE_ENUM);
        if (sql_field->def != NULL)
        {
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              return(true);
            }

            /* else, the defaults yield the correct length for NULLs. */
          }
          else /* not NULL */
          {
            def->length(cs->cset->lengthsp(cs, def->ptr(), def->length()));
            if (find_type2(interval, def->ptr(), def->length(), cs) == 0) /* not found */
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              return(true);
            }
          }
        }
        calculate_interval_lengths(cs, interval, &field_length, &dummy);
        sql_field->length= field_length;
      }
      set_if_smaller(sql_field->length, (uint32_t)MAX_FIELD_WIDTH-1);
    }

    sql_field->create_length_to_internal_length();
    if (prepare_blob_field(session, sql_field))
      return(true);

    if (!(sql_field->flags & NOT_NULL_FLAG))
      null_fields++;

    if (check_column_name(sql_field->field_name))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
      return(true);
    }

    /* Check if we have used the same field name before */
    for (dup_no=0; (dup_field=it2++) != sql_field; dup_no++)
    {
      if (my_strcasecmp(system_charset_info,
                        sql_field->field_name,
                        dup_field->field_name) == 0)
      {
	/*
	  If this was a CREATE ... SELECT statement, accept a field
	  redefinition if we are changing a field in the SELECT part
	*/
	if (field_no < select_field_pos || dup_no >= select_field_pos)
	{
	  my_error(ER_DUP_FIELDNAME, MYF(0), sql_field->field_name);
	  return(true);
	}
	else
	{
	  /* Field redefined */
	  sql_field->def=		dup_field->def;
	  sql_field->sql_type=		dup_field->sql_type;
	  sql_field->charset=		(dup_field->charset ?
					 dup_field->charset :
					 create_info->default_table_charset);
	  sql_field->length=		dup_field->char_length;
          sql_field->pack_length=	dup_field->pack_length;
          sql_field->key_length=	dup_field->key_length;
	  sql_field->decimals=		dup_field->decimals;
	  sql_field->create_length_to_internal_length();
	  sql_field->unireg_check=	dup_field->unireg_check;
          /*
            We're making one field from two, the result field will have
            dup_field->flags as flags. If we've incremented null_fields
            because of sql_field->flags, decrement it back.
          */
          if (!(sql_field->flags & NOT_NULL_FLAG))
            null_fields--;
	  sql_field->flags=		dup_field->flags;
          sql_field->interval=          dup_field->interval;
	  it2.remove();			// Remove first (create) definition
	  select_field_pos--;
	  break;
	}
      }
    }
    /* Don't pack rows in old tables if the user has requested this */
    if ((sql_field->flags & BLOB_FLAG) ||
	(sql_field->sql_type == DRIZZLE_TYPE_VARCHAR && create_info->row_type != ROW_TYPE_FIXED))
      (*db_options)|= HA_OPTION_PACK_RECORD;
    it2.rewind();
  }

  /* record_offset will be increased with 'length-of-null-bits' later */
  record_offset= 0;
  null_fields+= total_uneven_bit_length;

  it.rewind();
  while ((sql_field=it++))
  {
    assert(sql_field->charset != 0);

    if (prepare_create_field(sql_field, &blob_columns,
			     &timestamps, &timestamps_with_niladic,
			     cursor->ha_table_flags()))
      return(true);
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
  }
  if (timestamps_with_niladic > 1)
  {
    my_message(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,
               ER(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS), MYF(0));
    return(true);
  }
  if (auto_increment > 1)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    return(true);
  }
  if (auto_increment &&
      (cursor->ha_table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_message(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,
               ER(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT), MYF(0));
    return(true);
  }

  if (blob_columns && (cursor->ha_table_flags() & HA_NO_BLOBS))
  {
    my_message(ER_TABLE_CANT_HANDLE_BLOB, ER(ER_TABLE_CANT_HANDLE_BLOB),
               MYF(0));
    return(true);
  }

  /* Create keys */

  List_iterator<Key> key_iterator(alter_info->key_list);
  List_iterator<Key> key_iterator2(alter_info->key_list);
  uint32_t key_parts=0, fk_key_count=0;
  bool primary_key=0,unique_key=0;
  Key *key, *key2;
  uint32_t tmp, key_number;
  /* special marker for keys to be ignored */
  static char ignore_key[1];

  /* Calculate number of key segements */
  *key_count= 0;

  while ((key=key_iterator++))
  {
    if (key->type == Key::FOREIGN_KEY)
    {
      fk_key_count++;
      if (((Foreign_key *)key)->validate(alter_info->create_list))
        return true;
      Foreign_key *fk_key= (Foreign_key*) key;
      if (fk_key->ref_columns.elements &&
	  fk_key->ref_columns.elements != fk_key->columns.elements)
      {
        my_error(ER_WRONG_FK_DEF, MYF(0),
                 (fk_key->name.str ? fk_key->name.str :
                                     "foreign key without name"),
                 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	return(true);
      }
      continue;
    }
    (*key_count)++;
    tmp=cursor->max_key_parts();
    if (key->columns.elements > tmp)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      return(true);
    }
    if (check_identifier_name(&key->name, ER_TOO_LONG_IDENT))
      return(true);
    key_iterator2.rewind ();
    if (key->type != Key::FOREIGN_KEY)
    {
      while ((key2 = key_iterator2++) != key)
      {
	/*
          foreign_key_prefix(key, key2) returns 0 if key or key2, or both, is
          'generated', and a generated key is a prefix of the other key.
          Then we do not need the generated shorter key.
        */
        if ((key2->type != Key::FOREIGN_KEY &&
             key2->name.str != ignore_key &&
             !foreign_key_prefix(key, key2)))
        {
          /* TODO: issue warning message */
          /* mark that the generated key should be ignored */
          if (!key2->generated ||
              (key->generated && key->columns.elements <
               key2->columns.elements))
            key->name.str= ignore_key;
          else
          {
            key2->name.str= ignore_key;
            key_parts-= key2->columns.elements;
            (*key_count)--;
          }
          break;
        }
      }
    }
    if (key->name.str != ignore_key)
      key_parts+=key->columns.elements;
    else
      (*key_count)--;
    if (key->name.str && !tmp_table && (key->type != Key::PRIMARY) &&
        is_primary_key_name(key->name.str))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name.str);
      return(true);
    }
  }
  tmp=cursor->max_keys();
  if (*key_count > tmp)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),tmp);
    return(true);
  }

  (*key_info_buffer)= key_info= (KEY*) sql_calloc(sizeof(KEY) * (*key_count));
  key_part_info=(KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO)*key_parts);
  if (!*key_info_buffer || ! key_part_info)
    return(true);				// Out of memory

  key_iterator.rewind();
  key_number=0;
  for (; (key=key_iterator++) ; key_number++)
  {
    uint32_t key_length=0;
    Key_part_spec *column;

    if (key->name.str == ignore_key)
    {
      /* ignore redundant keys */
      do
	key=key_iterator++;
      while (key && key->name.str == ignore_key);
      if (!key)
	break;
    }

    switch (key->type) {
    case Key::MULTIPLE:
	key_info->flags= 0;
	break;
    case Key::FOREIGN_KEY:
      key_number--;				// Skip this key
      continue;
    default:
      key_info->flags = HA_NOSAME;
      break;
    }
    if (key->generated)
      key_info->flags|= HA_GENERATED_KEY;

    key_info->key_parts=(uint8_t) key->columns.elements;
    key_info->key_part=key_part_info;
    key_info->usable_key_parts= key_number;
    key_info->algorithm= key->key_create_info.algorithm;

    /* Take block size from key part or table part */
    /*
      TODO: Add warning if block size changes. We can't do it here, as
      this may depend on the size of the key
    */
    key_info->block_size= (key->key_create_info.block_size ?
                           key->key_create_info.block_size :
                           create_proto->options().key_block_size());

    if (key_info->block_size)
      key_info->flags|= HA_USES_BLOCK_SIZE;

    uint32_t tmp_len= system_charset_info->cset->charpos(system_charset_info,
                                           key->key_create_info.comment.str,
                                           key->key_create_info.comment.str +
                                           key->key_create_info.comment.length,
                                           INDEX_COMMENT_MAXLEN);

    if (tmp_len < key->key_create_info.comment.length)
    {
      my_error(ER_WRONG_STRING_LENGTH, MYF(0),
               key->key_create_info.comment.str,"INDEX COMMENT",
               (uint32_t) INDEX_COMMENT_MAXLEN);
      return -1;
    }

    key_info->comment.length= key->key_create_info.comment.length;
    if (key_info->comment.length > 0)
    {
      key_info->flags|= HA_USES_COMMENT;
      key_info->comment.str= key->key_create_info.comment.str;
    }

    message::Table::Field *protofield= NULL;

    List_iterator<Key_part_spec> cols(key->columns), cols2(key->columns);
    for (uint32_t column_nr=0 ; (column=cols++) ; column_nr++)
    {
      uint32_t length;
      Key_part_spec *dup_column;
      int proto_field_nr= 0;

      it.rewind();
      field=0;
      while ((sql_field=it++) && ++proto_field_nr &&
	     my_strcasecmp(system_charset_info,
			   column->field_name.str,
			   sql_field->field_name))
	field++;
      if (!sql_field)
      {
	my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
	return(true);
      }
      while ((dup_column= cols2++) != column)
      {
        if (!my_strcasecmp(system_charset_info,
	     	           column->field_name.str, dup_column->field_name.str))
	{
	  my_printf_error(ER_DUP_FIELDNAME,
			  ER(ER_DUP_FIELDNAME),MYF(0),
			  column->field_name.str);
	  return(true);
	}
      }
      cols2.rewind();

      if (create_proto->field_size() > 0)
        protofield= create_proto->mutable_field(proto_field_nr - 1);

      {
        column->length*= sql_field->charset->mbmaxlen;

        if (sql_field->sql_type == DRIZZLE_TYPE_BLOB)
        {
          if (! (cursor->ha_table_flags() & HA_CAN_INDEX_BLOBS))
          {
            my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name.str);
            return true;
          }
          if (! column->length)
          {
            my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name.str);
            return true;
          }
        }
        if (! (sql_field->flags & NOT_NULL_FLAG))
        {
          if (key->type == Key::PRIMARY)
          {
            /* Implicitly set primary key fields to NOT NULL for ISO conf. */
            sql_field->flags|= NOT_NULL_FLAG;
            null_fields--;

            if (protofield)
            {
              message::Table::Field::FieldConstraints *constraints;
              constraints= protofield->mutable_constraints();
              constraints->set_is_nullable(false);
            }

          }
          else
          {
            key_info->flags|= HA_NULL_PART_KEY;
            if (! (cursor->ha_table_flags() & HA_NULL_IN_KEY))
            {
              my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name.str);
              return true;
            }
          }
        }
        if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
        {
          if (column_nr == 0 || (cursor->ha_table_flags() & HA_AUTO_PART_KEY))
            auto_increment--;			// Field is used
        }
      }

      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16_t) sql_field->offset;
      key_part_info->key_type=sql_field->pack_flag;
      length= sql_field->key_length;

      if (column->length)
      {
	if (sql_field->sql_type == DRIZZLE_TYPE_BLOB)
	{
	  if ((length=column->length) > max_key_length ||
	      length > cursor->max_key_part_length())
	  {
	    length= min(max_key_length, cursor->max_key_part_length());
	    if (key->type == Key::MULTIPLE)
	    {
	      /* not a critical problem */
	      char warn_buff[DRIZZLE_ERRMSG_SIZE];
	      snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
                       length);
	      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			   ER_TOO_LONG_KEY, warn_buff);
              /* Align key length to multibyte char boundary */
              length-= length % sql_field->charset->mbmaxlen;
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY,MYF(0),length);
	      return(true);
	    }
	  }
	}
	else if ((column->length > length ||
            ! Field::type_can_have_key_part(sql_field->sql_type)))
	{
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  return(true);
	}
	else if (!(cursor->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  length=column->length;
      }
      else if (length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name.str);
	  return(true);
      }
      if (length > cursor->max_key_part_length())
      {
        length= cursor->max_key_part_length();
	if (key->type == Key::MULTIPLE)
	{
	  /* not a critical problem */
	  char warn_buff[DRIZZLE_ERRMSG_SIZE];
	  snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
                   length);
	  push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
		       ER_TOO_LONG_KEY, warn_buff);
          /* Align key length to multibyte char boundary */
          length-= length % sql_field->charset->mbmaxlen;
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY,MYF(0),length);
	  return(true);
	}
      }
      key_part_info->length=(uint16_t) length;
      /* Use packed keys for long strings on the first column */
      if (!((*db_options) & HA_OPTION_NO_PACK_KEYS) &&
	  (length >= KEY_DEFAULT_PACK_LENGTH &&
	   (sql_field->sql_type == DRIZZLE_TYPE_VARCHAR ||
      sql_field->sql_type == DRIZZLE_TYPE_BLOB)))
      {
        if ((column_nr == 0 && sql_field->sql_type == DRIZZLE_TYPE_BLOB) ||
            sql_field->sql_type == DRIZZLE_TYPE_VARCHAR)
          key_info->flags|= HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY;
        else
          key_info->flags|= HA_PACK_KEY;
      }
      /* Check if the key segment is partial, set the key flag accordingly */
      if (length != sql_field->key_length)
        key_info->flags|= HA_KEY_HAS_PART_KEY_SEG;

      key_length+=length;
      key_part_info++;

      /* Create the key name based on the first column (if not given) */
      if (column_nr == 0)
      {
	if (key->type == Key::PRIMARY)
	{
	  if (primary_key)
	  {
	    my_message(ER_MULTIPLE_PRI_KEY, ER(ER_MULTIPLE_PRI_KEY),
                       MYF(0));
	    return(true);
	  }
          static const char pkey_name[]= "PRIMARY";
	  key_name=pkey_name;
	  primary_key=1;
	}
	else if (!(key_name= key->name.str))
	  key_name=make_unique_key_name(sql_field->field_name,
					*key_info_buffer, key_info);
	if (check_if_keyname_exists(key_name, *key_info_buffer, key_info))
	{
	  my_error(ER_DUP_KEYNAME, MYF(0), key_name);
	  return(true);
	}
	key_info->name=(char*) key_name;
      }
    }
    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      return(true);
    }
    if (!(key_info->flags & HA_NULL_PART_KEY))
      unique_key=1;
    key_info->key_length=(uint16_t) key_length;
    if (key_length > max_key_length)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      return(true);
    }
    key_info++;
  }
  if (!unique_key && !primary_key &&
      (cursor->ha_table_flags() & HA_REQUIRE_PRIMARY_KEY))
  {
    my_message(ER_REQUIRES_PRIMARY_KEY, ER(ER_REQUIRES_PRIMARY_KEY), MYF(0));
    return(true);
  }
  if (auto_increment > 0)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    return(true);
  }
  /* Sort keys in optimized order */
  my_qsort((unsigned char*) *key_info_buffer, *key_count, sizeof(KEY),
	   (qsort_cmp) sort_keys);

  /* Check fields. */
  it.rewind();
  while ((sql_field=it++))
  {
    Field::utype type= (Field::utype) MTYP_TYPENR(sql_field->unireg_check);

    if (session->variables.sql_mode & MODE_NO_ZERO_DATE &&
        !sql_field->def &&
        sql_field->sql_type == DRIZZLE_TYPE_TIMESTAMP &&
        (sql_field->flags & NOT_NULL_FLAG) &&
        (type == Field::NONE || type == Field::TIMESTAMP_UN_FIELD))
    {
      /*
        An error should be reported if:
          - NO_ZERO_DATE SQL mode is active;
          - there is no explicit DEFAULT clause (default column value);
          - this is a TIMESTAMP column;
          - the column is not NULL;
          - this is not the DEFAULT CURRENT_TIMESTAMP column.

        In other words, an error should be reported if
          - NO_ZERO_DATE SQL mode is active;
          - the column definition is equivalent to
            'column_name TIMESTAMP DEFAULT 0'.
      */

      my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
      return(true);
    }
  }

  return(false);
}

/*
  Extend long VARCHAR fields to blob & prepare field if it's a blob

  SYNOPSIS
    prepare_blob_field()
    sql_field		Field to check

  RETURN
    0	ok
    1	Error (sql_field can't be converted to blob)
        In this case the error is given
*/

static bool prepare_blob_field(Session *,
                               CreateField *sql_field)
{

  if (sql_field->length > MAX_FIELD_VARCHARLENGTH &&
      !(sql_field->flags & BLOB_FLAG))
  {
    my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sql_field->field_name,
             MAX_FIELD_VARCHARLENGTH / sql_field->charset->mbmaxlen);
    return 1;
  }

  if ((sql_field->flags & BLOB_FLAG) && sql_field->length)
  {
    if (sql_field->sql_type == DRIZZLE_TYPE_BLOB)
    {
      /* The user has given a length to the blob column */
      sql_field->pack_length= calc_pack_length(sql_field->sql_type, 0);
    }
    sql_field->length= 0;
  }
  return 0;
}


/*
  Ignore the name of this function... it locks :(

  Create a table

  SYNOPSIS
    mysql_create_table_no_lock()
    session			Thread object
    db			Database
    table_name		Table name
    create_info	        Create information (like MAX_ROWS)
    fields		List of fields to create
    keys		List of keys to create
    internal_tmp_table  Set to 1 if this is an internal temporary table
			(From ALTER Table)
    select_field_count

  DESCRIPTION
    If one creates a temporary table, this is automatically opened

    Note that this function assumes that caller already have taken
    name-lock on table being created or used some other way to ensure
    that concurrent operations won't intervene. mysql_create_table()
    is a wrapper that can be used for this.

  RETURN VALUES
    false OK
    true  error
*/

bool mysql_create_table_no_lock(Session *session,
                                TableIdentifier &identifier,
                                HA_CREATE_INFO *create_info,
				message::Table *table_proto,
                                AlterInfo *alter_info,
                                bool internal_tmp_table,
                                uint32_t select_field_count, 
                                bool is_if_not_exists)
{
  uint		db_options, key_count;
  KEY		*key_info_buffer;
  Cursor	*cursor;
  bool		error= true;
  TableShare share;
  bool lex_identified_temp_table= 
    (table_proto->type() == drizzled::message::Table::TEMPORARY);

  /* Check for duplicate fields and check type of table to create */
  if (!alter_info->create_list.elements)
  {
    my_message(ER_TABLE_MUST_HAVE_COLUMNS, ER(ER_TABLE_MUST_HAVE_COLUMNS),
               MYF(0));
    return true;
  }
  assert(strcmp(identifier.getTableName(), table_proto->name().c_str())==0);
  db_options= create_info->table_options;
  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    db_options|=HA_OPTION_PACK_RECORD;
  if (!(cursor= create_info->db_type->getCursor(share, session->mem_root)))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Cursor));
    return true;
  }

  set_table_default_charset(create_info, identifier.getDBName());

  /* Check if table exists */
  if (mysql_prepare_create_table(session, create_info, table_proto, alter_info,
                                 internal_tmp_table,
                                 &db_options, cursor,
                                 &key_info_buffer, &key_count,
                                 select_field_count))
    goto err;

  /* Check if table already exists */
  if (lex_identified_temp_table &&
      session->find_temporary_table(identifier.getDBName(), identifier.getTableName()))
  {
    if (is_if_not_exists)
    {
      create_info->table_existed= 1;		// Mark that table existed
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          identifier.getTableName());
      error= 0;
      goto err;
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), identifier.getTableName());
    goto err;
  }

  pthread_mutex_lock(&LOCK_open); /* CREATE TABLE (some confussion on naming, double check) */
  if (!internal_tmp_table && ! lex_identified_temp_table)
  {
    if (plugin::StorageEngine::getTableDefinition(*session,
                                                  identifier)==EEXIST)
    {
      if (is_if_not_exists)
      {
        error= false;
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                            identifier.getTableName());
        create_info->table_existed= 1;		// Mark that table existed
      }
      else 
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), identifier.getTableName());

      goto unlock_and_end;
    }
    /*
      We don't assert here, but check the result, because the table could be
      in the table definition cache and in the same time the .frm could be
      missing from the disk, in case of manual intervention which deletes
      the .frm cursor. The user has to use FLUSH TABLES; to clear the cache.
      Then she could create the table. This case is pretty obscure and
      therefore we don't introduce a new error message only for it.
    */
    if (TableShare::getShare(identifier.getDBName(), identifier.getTableName()))
    {
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), identifier.getTableName());
      goto unlock_and_end;
    }
  }

  /*
    Check that table with given name does not already
    exist in any storage engine. In such a case it should
    be discovered and the error ER_TABLE_EXISTS_ERROR be returned
    unless user specified CREATE TABLE IF EXISTS
    The LOCK_open mutex has been locked to make sure no
    one else is attempting to discover the table. Since
    it's not on disk as a frm cursor, no one could be using it!
  */
  if (! lex_identified_temp_table)
  {
    char table_path[FN_REFLEN];
    uint32_t          table_path_length;

    table_path_length= build_table_filename(table_path, sizeof(table_path),
                                            identifier.getDBName(), identifier.getTableName(), false);

    int retcode= plugin::StorageEngine::getTableDefinition(*session,table_path, identifier.getDBName(), identifier.getTableName(), false);
    switch (retcode)
    {
      case ENOENT:
        /* Normal case, no table exists. we can go and create it */
        break;
      case EEXIST:
        if (is_if_not_exists)
        {
          error= false;
          push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                              ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                              identifier.getTableName());
          create_info->table_existed= 1;		// Mark that table existed
          goto unlock_and_end;
        }
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), identifier.getTableName());
        goto unlock_and_end;
      default:
        my_error(retcode, MYF(0), identifier.getTableName());
        goto unlock_and_end;
    }
  }

  session->set_proc_info("creating table");
  create_info->table_existed= 0;		// Mark that table is created

  create_info->table_options=db_options;

  if (rea_create_table(session, identifier,
		       table_proto,
                       create_info, alter_info->create_list,
                       key_count, key_info_buffer))
    goto unlock_and_end;

  if (lex_identified_temp_table)
  {
    /* Open table and put in temporary table list */
    if (!(session->open_temporary_table(identifier)))
    {
      (void) session->rm_temporary_table(create_info->db_type, identifier);
      goto unlock_and_end;
    }
  }

  /*
    Don't write statement if:
    - It is an internal temporary table,
    - Row-based logging is used and it we are creating a temporary table, or
    - The binary log is not open.
    Otherwise, the statement shall be binlogged.
   */
  if (!internal_tmp_table && ! lex_identified_temp_table)
    write_bin_log(session, session->query, session->query_length);
  error= false;
unlock_and_end:
  pthread_mutex_unlock(&LOCK_open);

err:
  session->set_proc_info("After create");
  delete cursor;
  return(error);
}


/*
  Database locking aware wrapper for mysql_create_table_no_lock(),
*/

bool mysql_create_table(Session *session,
                        TableIdentifier &identifier,
                        HA_CREATE_INFO *create_info,
			message::Table *table_proto,
                        AlterInfo *alter_info,
                        bool internal_tmp_table,
                        uint32_t select_field_count,
                        bool is_if_not_exists)
{
  Table *name_lock= NULL;
  bool result;
  bool lex_identified_temp_table=
    (table_proto->type() == drizzled::message::Table::TEMPORARY);

  if (! lex_identified_temp_table)
  {
    if (session->lock_table_name_if_not_cached(identifier.getDBName(),
                                               identifier.getTableName(),
                                               &name_lock))
    {
      result= true;
      goto unlock;
    }
    if (name_lock == NULL)
    {
      if (is_if_not_exists)
      {
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                            identifier.getTableName());
        create_info->table_existed= 1;
        result= false;
      }
      else
      {
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), identifier.getTableName());
        result= true;
      }
      goto unlock;
    }
  }

  result= mysql_create_table_no_lock(session,
                                     identifier,
                                     create_info,
                                     table_proto,
                                     alter_info,
                                     internal_tmp_table,
                                     select_field_count,
                                     is_if_not_exists);

unlock:
  if (name_lock)
  {
    pthread_mutex_lock(&LOCK_open); /* Lock for removing name_lock during table create */
    session->unlink_open_table(name_lock);
    pthread_mutex_unlock(&LOCK_open);
  }

  return(result);
}


/*
** Give the key name after the first field with an optional '_#' after
**/

static bool
check_if_keyname_exists(const char *name, KEY *start, KEY *end)
{
  for (KEY *key=start ; key != end ; key++)
    if (!my_strcasecmp(system_charset_info,name,key->name))
      return 1;
  return 0;
}


static char *
make_unique_key_name(const char *field_name,KEY *start,KEY *end)
{
  char buff[MAX_FIELD_NAME],*buff_end;

  if (!check_if_keyname_exists(field_name,start,end) &&
      !is_primary_key_name(field_name))
    return (char*) field_name;			// Use fieldname

  buff_end= strncpy(buff, field_name, sizeof(buff)-4);
  buff_end+= strlen(buff);

  /*
    Only 3 chars + '\0' left, so need to limit to 2 digit
    This is ok as we can't have more than 100 keys anyway
  */
  for (uint32_t i=2 ; i< 100; i++)
  {
    *buff_end= '_';
    int10_to_str(i, buff_end+1, 10);
    if (!check_if_keyname_exists(buff,start,end))
      return sql_strdup(buff);
  }
  return (char*) "not_specified";		// Should never happen
}


/****************************************************************************
** Alter a table definition
****************************************************************************/

/*
  Rename a table.

  SYNOPSIS
    mysql_rename_table()
      base                      The plugin::StorageEngine handle.
      old_db                    The old database name.
      old_name                  The old table name.
      new_db                    The new database name.
      new_name                  The new table name.
      flags                     flags for build_table_filename().
                                FN_FROM_IS_TMP old_name is temporary.
                                FN_TO_IS_TMP   new_name is temporary.

  RETURN
    false   OK
    true    Error
*/

bool
mysql_rename_table(plugin::StorageEngine *base, const char *old_db,
                   const char *old_name, const char *new_db,
                   const char *new_name, uint32_t flags)
{
  Session *session= current_session;
  char from[FN_REFLEN], to[FN_REFLEN];
  char *from_base= from, *to_base= to;
  int error= 0;

  assert(base);

  build_table_filename(from, sizeof(from), old_db, old_name,
                       flags & FN_FROM_IS_TMP);
  build_table_filename(to, sizeof(to), new_db, new_name,
                       flags & FN_TO_IS_TMP);

  if (!(error= base->renameTable(session, from_base, to_base)))
  {
    if (base->check_flag(HTON_BIT_HAS_DATA_DICTIONARY) == 0
       && rename_table_proto_file(from_base, to_base))
    {
      error= my_errno;
      base->renameTable(session, to_base, from_base);
    }
  }

  if (error == HA_ERR_WRONG_COMMAND)
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "ALTER Table");
  else if (error)
    my_error(ER_ERROR_ON_RENAME, MYF(0), from, to, error);
  return(error != 0);
}


/*
  Force all other threads to stop using the table

  SYNOPSIS
    wait_while_table_is_used()
    session			Thread Cursor
    table		Table to remove from cache
    function            HA_EXTRA_PREPARE_FOR_DROP if table is to be deleted
                        HA_EXTRA_FORCE_REOPEN if table is not be used
                        HA_EXTRA_PREPARE_FOR_RENAME if table is to be renamed
  NOTES
   When returning, the table will be unusable for other threads until
   the table is closed.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

void wait_while_table_is_used(Session *session, Table *table,
                              enum ha_extra_function function)
{

  safe_mutex_assert_owner(&LOCK_open);

  table->cursor->extra(function);
  /* Mark all tables that are in use as 'old' */
  mysql_lock_abort(session, table);	/* end threads waiting on lock */

  /* Wait until all there are no other threads that has this table open */
  remove_table_from_cache(session, table->s->db.str,
                          table->s->table_name.str,
                          RTFC_WAIT_OTHER_THREAD_FLAG);
}

/*
  Close a cached table

  SYNOPSIS
    close_cached_table()
    session			Thread Cursor
    table		Table to remove from cache

  NOTES
    Function ends by signaling threads waiting for the table to try to
    reopen the table.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

void Session::close_cached_table(Table *table)
{

  wait_while_table_is_used(this, table, HA_EXTRA_FORCE_REOPEN);
  /* Close lock if this is not got with LOCK TABLES */
  if (lock)
  {
    mysql_unlock_tables(this, lock);
    lock= NULL;			// Start locked threads
  }
  /* Close all copies of 'table'.  This also frees all LOCK TABLES lock */
  unlink_open_table(table);

  /* When lock on LOCK_open is freed other threads can continue */
  broadcast_refresh();
}

/*
  RETURN VALUES
    false Message sent to net (admin operation went ok)
    true  Message should be sent by caller
          (admin operation or network communication failed)
*/
static bool mysql_admin_table(Session* session, TableList* tables,
                              HA_CHECK_OPT* check_opt,
                              const char *operator_name,
                              thr_lock_type lock_type,
                              bool open_for_modify,
                              bool no_warnings_for_error,
                              uint32_t extra_open_options,
                              int (*prepare_func)(Session *, TableList *,
                                                  HA_CHECK_OPT *),
                              int (Cursor::*operator_func)(Session *,
                                                            HA_CHECK_OPT *))
{
  TableList *table;
  Select_Lex *select= &session->lex->select_lex;
  List<Item> field_list;
  Item *item;
  LEX *lex= session->lex;
  int result_code= 0;
  const CHARSET_INFO * const cs= system_charset_info;

  if (! session->endActiveTransaction())
    return 1;
  field_list.push_back(item = new Item_empty_string("Table",
                                                    NAME_CHAR_LEN * 2,
                                                    cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Op", 10, cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_type", 10, cs));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_text", 255, cs));
  item->maybe_null = 1;
  if (session->client->sendFields(&field_list))
    return true;

  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    char* db = table->db;
    bool fatal_error=0;

    sprintf(table_name,"%s.%s",db,table->table_name);
    session->open_options|= extra_open_options;
    table->lock_type= lock_type;
    /* open only one table from local list of command */
    {
      TableList *save_next_global, *save_next_local;
      save_next_global= table->next_global;
      table->next_global= 0;
      save_next_local= table->next_local;
      table->next_local= 0;
      select->table_list.first= (unsigned char*)table;
      /*
        Time zone tables and SP tables can be add to lex->query_tables list,
        so it have to be prepared.
        TODO: Investigate if we can put extra tables into argument instead of
        using lex->query_tables
      */
      lex->query_tables= table;
      lex->query_tables_last= &table->next_global;
      lex->query_tables_own_last= 0;
      session->no_warnings_for_error= no_warnings_for_error;

      session->openTablesLock(table);
      session->no_warnings_for_error= 0;
      table->next_global= save_next_global;
      table->next_local= save_next_local;
      session->open_options&= ~extra_open_options;
    }

    if (prepare_func)
    {
      switch ((*prepare_func)(session, table, check_opt)) {
      case  1:           // error, message written to net
        ha_autocommit_or_rollback(session, 1);
        session->endTransaction(ROLLBACK);
        session->close_thread_tables();
        continue;
      case -1:           // error, message could be written to net
        goto err;
      default:           // should be 0 otherwise
        ;
      }
    }

    /*
      CHECK Table command is only command where VIEW allowed here and this
      command use only temporary teble method for VIEWs resolving => there
      can't be VIEW tree substitition of join view => if opening table
      succeed then table->table will have real Table pointer as value (in
      case of join view substitution table->table can be 0, but here it is
      impossible)
    */
    if (!table->table)
    {
      if (!session->warn_list.elements)
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                     ER_CHECK_NO_SUCH_TABLE, ER(ER_CHECK_NO_SUCH_TABLE));
      result_code= HA_ADMIN_CORRUPT;
      goto send_result;
    }

    if ((table->table->db_stat & HA_READ_ONLY) && open_for_modify)
    {
      char buff[FN_REFLEN + DRIZZLE_ERRMSG_SIZE];
      uint32_t length;
      session->client->store(table_name);
      session->client->store(operator_name);
      session->client->store(STRING_WITH_LEN("error"));
      length= snprintf(buff, sizeof(buff), ER(ER_OPEN_AS_READONLY),
                       table_name);
      session->client->store(buff, length);
      ha_autocommit_or_rollback(session, 0);
      session->endTransaction(COMMIT);
      session->close_thread_tables();
      lex->reset_query_tables_list(false);
      table->table=0;				// For query cache
      if (session->client->flush())
	goto err;
      continue;
    }

    /* Close all instances of the table to allow repair to rename files */
    if (lock_type == TL_WRITE && table->table->s->version)
    {
      pthread_mutex_lock(&LOCK_open); /* Lock type is TL_WRITE and we lock to repair the table */
      const char *old_message=session->enter_cond(&COND_refresh, &LOCK_open,
					      "Waiting to get writelock");
      mysql_lock_abort(session,table->table);
      remove_table_from_cache(session, table->table->s->db.str,
                              table->table->s->table_name.str,
                              RTFC_WAIT_OTHER_THREAD_FLAG |
                              RTFC_CHECK_KILLED_FLAG);
      session->exit_cond(old_message);
      if (session->killed)
	goto err;
      open_for_modify= 0;
    }

    if (table->table->s->crashed && operator_func == &Cursor::ha_check)
    {
      session->client->store(table_name);
      session->client->store(operator_name);
      session->client->store(STRING_WITH_LEN("warning"));
      session->client->store(STRING_WITH_LEN("Table is marked as crashed"));
      if (session->client->flush())
        goto err;
    }

    result_code = (table->table->cursor->*operator_func)(session, check_opt);

send_result:

    lex->cleanup_after_one_table_open();
    session->clear_error();  // these errors shouldn't get client
    {
      List_iterator_fast<DRIZZLE_ERROR> it(session->warn_list);
      DRIZZLE_ERROR *err;
      while ((err= it++))
      {
        session->client->store(table_name);
        session->client->store(operator_name);
        session->client->store(warning_level_names[err->level].str,
                               warning_level_names[err->level].length);
        session->client->store(err->msg);
        if (session->client->flush())
          goto err;
      }
      drizzle_reset_errors(session, true);
    }
    session->client->store(table_name);
    session->client->store(operator_name);

send_result_message:

    switch (result_code) {
    case HA_ADMIN_NOT_IMPLEMENTED:
      {
	char buf[ERRMSGSIZE+20];
	uint32_t length=snprintf(buf, ERRMSGSIZE,
                             ER(ER_CHECK_NOT_IMPLEMENTED), operator_name);
	session->client->store(STRING_WITH_LEN("note"));
	session->client->store(buf, length);
      }
      break;

    case HA_ADMIN_OK:
      session->client->store(STRING_WITH_LEN("status"));
      session->client->store(STRING_WITH_LEN("OK"));
      break;

    case HA_ADMIN_FAILED:
      session->client->store(STRING_WITH_LEN("status"));
      session->client->store(STRING_WITH_LEN("Operation failed"));
      break;

    case HA_ADMIN_REJECT:
      session->client->store(STRING_WITH_LEN("status"));
      session->client->store(STRING_WITH_LEN("Operation need committed state"));
      open_for_modify= false;
      break;

    case HA_ADMIN_ALREADY_DONE:
      session->client->store(STRING_WITH_LEN("status"));
      session->client->store(STRING_WITH_LEN("Table is already up to date"));
      break;

    case HA_ADMIN_CORRUPT:
      session->client->store(STRING_WITH_LEN("error"));
      session->client->store(STRING_WITH_LEN("Corrupt"));
      fatal_error=1;
      break;

    case HA_ADMIN_INVALID:
      session->client->store(STRING_WITH_LEN("error"));
      session->client->store(STRING_WITH_LEN("Invalid argument"));
      break;

    case HA_ADMIN_TRY_ALTER:
    {
      /*
        This is currently used only by InnoDB. ha_innobase::optimize() answers
        "try with alter", so here we close the table, do an ALTER Table,
        reopen the table and do ha_innobase::analyze() on it.
      */
      ha_autocommit_or_rollback(session, 0);
      session->close_thread_tables();
      TableList *save_next_local= table->next_local,
                 *save_next_global= table->next_global;
      table->next_local= table->next_global= 0;
      result_code= mysql_recreate_table(session, table);
      /*
        mysql_recreate_table() can push OK or ERROR.
        Clear 'OK' status. If there is an error, keep it:
        we will store the error message in a result set row
        and then clear.
      */
      if (session->main_da.is_ok())
        session->main_da.reset_diagnostics_area();
      ha_autocommit_or_rollback(session, 0);
      session->close_thread_tables();
      if (!result_code) // recreation went ok
      {
        if ((table->table= session->openTableLock(table, lock_type)) &&
            ((result_code= table->table->cursor->ha_analyze(session, check_opt)) > 0))
          result_code= 0; // analyze went ok
      }
      if (result_code) // either mysql_recreate_table or analyze failed
      {
        assert(session->is_error());
        if (session->is_error())
        {
          const char *err_msg= session->main_da.message();
          /* Hijack the row already in-progress. */
          session->client->store(STRING_WITH_LEN("error"));
          session->client->store(err_msg);
          (void)session->client->flush();
          /* Start off another row for HA_ADMIN_FAILED */
          session->client->store(table_name);
          session->client->store(operator_name);
          session->clear_error();
        }
      }
      result_code= result_code ? HA_ADMIN_FAILED : HA_ADMIN_OK;
      table->next_local= save_next_local;
      table->next_global= save_next_global;
      goto send_result_message;
    }
    case HA_ADMIN_NEEDS_UPGRADE:
    case HA_ADMIN_NEEDS_ALTER:
    {
      char buf[ERRMSGSIZE];
      uint32_t length;

      session->client->store(STRING_WITH_LEN("error"));
      length=snprintf(buf, ERRMSGSIZE, ER(ER_TABLE_NEEDS_UPGRADE), table->table_name);
      session->client->store(buf, length);
      fatal_error=1;
      break;
    }

    default:				// Probably HA_ADMIN_INTERNAL_ERROR
      {
        char buf[ERRMSGSIZE+20];
        uint32_t length=snprintf(buf, ERRMSGSIZE,
                             _("Unknown - internal error %d during operation"),
                             result_code);
        session->client->store(STRING_WITH_LEN("error"));
        session->client->store(buf, length);
        fatal_error=1;
        break;
      }
    }
    if (table->table)
    {
      if (fatal_error)
        table->table->s->version=0;               // Force close of table
      else if (open_for_modify)
      {
        if (table->table->s->tmp_table)
          table->table->cursor->info(HA_STATUS_CONST);
        else
        {
          pthread_mutex_lock(&LOCK_open);
          remove_table_from_cache(session, table->table->s->db.str,
                                  table->table->s->table_name.str, RTFC_NO_FLAG);
          pthread_mutex_unlock(&LOCK_open);
        }
      }
    }
    ha_autocommit_or_rollback(session, 0);
    session->endTransaction(COMMIT);
    session->close_thread_tables();
    table->table=0;				// For query cache
    if (session->client->flush())
      goto err;
  }

  session->my_eof();
  return(false);

err:
  ha_autocommit_or_rollback(session, 1);
  session->endTransaction(ROLLBACK);
  session->close_thread_tables();			// Shouldn't be needed
  if (table)
    table->table=0;
  return(true);
}

bool mysql_optimize_table(Session* session, TableList* tables, HA_CHECK_OPT* check_opt)
{
  return(mysql_admin_table(session, tables, check_opt,
                           "optimize", TL_WRITE, 1,0,0,0,
                           &Cursor::ha_optimize));
}

/*
  Create a table identical to the specified table

  SYNOPSIS
    mysql_create_like_table()
    session		Thread object
    table       Table list element for target table
    src_table   Table list element for source table
    create_info Create info

  RETURN VALUES
    false OK
    true  error
*/

bool mysql_create_like_table(Session* session, TableList* table, TableList* src_table,
                             drizzled::message::Table& create_table_proto,
                             drizzled::plugin::StorageEngine *engine_arg,
                             bool is_if_not_exists,
                             bool is_engine_set)
{
  Table *name_lock= 0;
  char src_path[FN_REFLEN];
  char *db= table->db;
  char *table_name= table->table_name;
  int  err;
  bool res= true;
  uint32_t not_used;
  message::Table src_proto;
  bool lex_identified_temp_table=
    (create_table_proto.type() == drizzled::message::Table::TEMPORARY);

  /*
    By opening source table we guarantee that it exists and no concurrent
    DDL operation will mess with it. Later we also take an exclusive
    name-lock on target table name, which makes copying of .frm cursor,
    call to plugin::StorageEngine::createTable() and binlogging atomic
    against concurrent DML and DDL operations on target table.
    Thus by holding both these "locks" we ensure that our statement is
    properly isolated from all concurrent operations which matter.
  */
  if (session->open_tables_from_list(&src_table, &not_used))
    return true;

  strncpy(src_path, src_table->table->s->path.str, sizeof(src_path));


  TableIdentifier destination_identifier(db, table_name, lex_identified_temp_table ?
                                         (engine_arg->check_flag(HTON_BIT_DOES_TRANSACTIONS) ? TRANSACTIONAL_TMP_TABLE : NON_TRANSACTIONAL_TMP_TABLE) :
                                         NO_TMP_TABLE);

  /*
    Check that destination tables does not exist. Note that its name
    was already checked when it was added to the table list.
  */
  if (lex_identified_temp_table)
  {
    if (session->find_temporary_table(db, table_name))
      goto table_exists;
  }
  else
  {
    if (session->lock_table_name_if_not_cached(db, table_name, &name_lock))
      goto err;
    if (! name_lock)
      goto table_exists;

    if (plugin::StorageEngine::getTableDefinition(*session,
                                                  destination_identifier) == EEXIST)
      goto table_exists;
  }

  /*
    Create a new table by copying from source table

    Altough exclusive name-lock on target table protects us from concurrent
    DML and DDL operations on it we still want to wrap .FRM creation and call
    to plugin::StorageEngine::createTable() in critical section protected by
    LOCK_open in order to provide minimal atomicity against operations which
    disregard name-locks, like I_S implementation, for example. This is a
    temporary and should not be copied. Instead we should fix our code to
    always honor name-locks.

    Also some engines (e.g. NDB cluster) require that LOCK_open should be held
    during the call to plugin::StorageEngine::createTable().
    See bug #28614 for more info.
  */
  pthread_mutex_lock(&LOCK_open); /* We lock for CREATE TABLE LIKE to copy table definition */
  {
    int protoerr= EEXIST;

    if (src_table->schema_table)
    {
      /*
        If engine was not specified and we are reading from the I_S, then we need to
        toss an error. This should go away later on when we straighten out the
        I_S engine.
      */
      if (! is_engine_set)
      {
        pthread_mutex_unlock(&LOCK_open);
        my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
                 "INFORMATION_ENGINE",
                 "TEMPORARY");
        goto err;
      }

      if (create_like_schema_frm(session, src_table, &src_proto))
      {
        pthread_mutex_unlock(&LOCK_open);
        goto err;
      }
    }
    else
    {
      protoerr= plugin::StorageEngine::getTableDefinition(*session,
                                                          src_path,
                                                          db,
                                                          table_name,
                                                          false,
                                                          &src_proto);
    }

    message::Table new_proto(src_proto);

    if (lex_identified_temp_table)
    {
      new_proto.set_type(message::Table::TEMPORARY);
    }
    else
    {
      new_proto.set_type(message::Table::STANDARD);
    }

    if (is_engine_set)
    {
      message::Table::StorageEngine *protoengine;

      protoengine= new_proto.mutable_engine();
      protoengine->set_name(create_table_proto.engine().name());
    }

    if (protoerr == EEXIST)
    {
      plugin::StorageEngine* engine= plugin::StorageEngine::findByName(*session,
                                                                       new_proto.engine().name());

      if (engine->check_flag(HTON_BIT_HAS_DATA_DICTIONARY) == false)
      {
        string dst_proto_path(destination_identifier.getPath());
        dst_proto_path.append(".dfe");

        protoerr= drizzle_write_proto_file(dst_proto_path.c_str(), &new_proto);
      }
      else
      {
        protoerr= 0;
      }
    }

    if (protoerr)
    {
      if (my_errno == ENOENT)
        my_error(ER_BAD_DB_ERROR,MYF(0),db);
      else
        my_error(ER_CANT_CREATE_FILE, MYF(0), destination_identifier.getPath(), my_errno);
      pthread_mutex_unlock(&LOCK_open);
      goto err;
    }

    /*
      As mysql_truncate don't work on a new table at this stage of
      creation, instead create the table directly (for both normal
      and temporary tables).
    */
    err= plugin::StorageEngine::createTable(*session,
                                            destination_identifier,
                                            true, new_proto);
  }
  pthread_mutex_unlock(&LOCK_open);

  if (lex_identified_temp_table)
  {
    if (err || !session->open_temporary_table(destination_identifier))
    {
      (void) session->rm_temporary_table(engine_arg, destination_identifier);
      goto err;
    }
  }
  else if (err)
  {
    (void) quick_rm_table(*session, db,
			  table_name, false);
    goto err;
  }

  /*
    We have to write the query before we unlock the tables.
  */
  {
    /*
       Since temporary tables are not replicated under row-based
       replication, CREATE TABLE ... LIKE ... needs special
       treatement.  We have four cases to consider, according to the
       following decision table:

           ==== ========= ========= ==============================
           Case    Target    Source Write to binary log
           ==== ========= ========= ==============================
           1       normal    normal Original statement
           2       normal temporary Generated statement
           3    temporary    normal Nothing
           4    temporary temporary Nothing
           ==== ========= ========= ==============================
    */
    if (! lex_identified_temp_table)
    {
      if (src_table->table->s->tmp_table)               // Case 2
      {
        char buf[2048];
        String query(buf, sizeof(buf), system_charset_info);
        query.length(0);  // Have to zero it since constructor doesn't


        /*
          Here we open the destination table, on which we already have
          name-lock. This is needed for store_create_info() to work.
          The table will be closed by unlink_open_table() at the end
          of this function.
        */
        table->table= name_lock;
        pthread_mutex_lock(&LOCK_open); /* Open new table we have just acquired */
        if (session->reopen_name_locked_table(table, false))
        {
          pthread_mutex_unlock(&LOCK_open);
          goto err;
        }
        pthread_mutex_unlock(&LOCK_open);

        int result= store_create_info(table, &query, is_if_not_exists);

        assert(result == 0); // store_create_info() always return 0
        write_bin_log(session, query.ptr(), query.length());
      }
      else                                      // Case 1
        write_bin_log(session, session->query, session->query_length);
    }
  }

  res= false;
  goto err;

table_exists:
  if (is_if_not_exists)
  {
    char warn_buff[DRIZZLE_ERRMSG_SIZE];
    snprintf(warn_buff, sizeof(warn_buff),
             ER(ER_TABLE_EXISTS_ERROR), table_name);
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
		 ER_TABLE_EXISTS_ERROR,warn_buff);
    res= false;
  }
  else
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);

err:
  if (name_lock)
  {
    pthread_mutex_lock(&LOCK_open); /* unlink open tables for create table like*/
    session->unlink_open_table(name_lock);
    pthread_mutex_unlock(&LOCK_open);
  }
  return(res);
}


bool mysql_analyze_table(Session* session, TableList* tables, HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  return(mysql_admin_table(session, tables, check_opt,
				"analyze", lock_type, 1, 0, 0, 0,
				&Cursor::ha_analyze));
}


bool mysql_check_table(Session* session, TableList* tables,HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  return(mysql_admin_table(session, tables, check_opt,
				"check", lock_type,
				0, 0, HA_OPEN_FOR_REPAIR, 0,
				&Cursor::ha_check));
}

/*
  Recreates tables by calling drizzled::alter_table().

  SYNOPSIS
    mysql_recreate_table()
    session			Thread Cursor
    tables		Tables to recreate

 RETURN
    Like drizzled::alter_table().
*/
bool mysql_recreate_table(Session *session, TableList *table_list)
{
  HA_CREATE_INFO create_info;
  AlterInfo alter_info;
  message::Table table_proto;

  assert(!table_list->next_global);
  /*
    table_list->table has been closed and freed. Do not reference
    uninitialized data. open_tables() could fail.
  */
  table_list->table= NULL;

  memset(&create_info, 0, sizeof(create_info));
  create_info.row_type=ROW_TYPE_NOT_USED;
  create_info.default_table_charset=default_charset_info;
  /* Force alter table to recreate table */
  alter_info.flags.set(ALTER_CHANGE_COLUMN);
  alter_info.flags.set(ALTER_RECREATE);
  return(alter_table(session, NULL, NULL, &create_info, &table_proto,
                     table_list, &alter_info, 0,
                     (order_st *) 0, 0));
}


bool mysql_checksum_table(Session *session, TableList *tables,
                          HA_CHECK_OPT *check_opt)
{
  TableList *table;
  List<Item> field_list;
  Item *item;

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null= 1;
  field_list.push_back(item= new Item_int("Checksum", (int64_t) 1,
                                          MY_INT64_NUM_DECIMAL_DIGITS));
  item->maybe_null= 1;
  if (session->client->sendFields(&field_list))
    return true;

  /* Open one table after the other to keep lock time as short as possible. */
  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    Table *t;

    sprintf(table_name,"%s.%s",table->db,table->table_name);

    t= table->table= session->openTableLock(table, TL_READ);
    session->clear_error();			// these errors shouldn't get client

    session->client->store(table_name);

    if (!t)
    {
      /* Table didn't exist */
      session->client->store();
      session->clear_error();
    }
    else
    {
      if (t->cursor->ha_table_flags() & HA_HAS_CHECKSUM &&
	  !(check_opt->flags & T_EXTEND))
	session->client->store((uint64_t)t->cursor->checksum());
      else if (!(t->cursor->ha_table_flags() & HA_HAS_CHECKSUM) &&
	       (check_opt->flags & T_QUICK))
	session->client->store();
      else
      {
	/* calculating table's checksum */
	ha_checksum crc= 0;
        unsigned char null_mask=256 -  (1 << t->s->last_null_bit_pos);

        t->use_all_columns();

	if (t->cursor->ha_rnd_init(1))
	  session->client->store();
	else
	{
	  for (;;)
	  {
	    ha_checksum row_crc= 0;
            int error= t->cursor->rnd_next(t->record[0]);
            if (unlikely(error))
            {
              if (error == HA_ERR_RECORD_DELETED)
                continue;
              break;
            }
	    if (t->s->null_bytes)
            {
              /* fix undefined null bits */
              t->record[0][t->s->null_bytes-1] |= null_mask;
              if (!(t->s->db_create_options & HA_OPTION_PACK_RECORD))
                t->record[0][0] |= 1;

	      row_crc= my_checksum(row_crc, t->record[0], t->s->null_bytes);
            }

	    for (uint32_t i= 0; i < t->s->fields; i++ )
	    {
	      Field *f= t->field[i];
	      if ((f->type() == DRIZZLE_TYPE_BLOB) ||
                  (f->type() == DRIZZLE_TYPE_VARCHAR))
	      {
		String tmp;
		f->val_str(&tmp);
		row_crc= my_checksum(row_crc, (unsigned char*) tmp.ptr(), tmp.length());
	      }
	      else
		row_crc= my_checksum(row_crc, f->ptr,
				     f->pack_length());
	    }

	    crc+= row_crc;
	  }
	  session->client->store((uint64_t)crc);
          t->cursor->ha_rnd_end();
	}
      }
      session->clear_error();
      session->close_thread_tables();
      table->table=0;				// For query cache
    }
    if (session->client->flush())
      goto err;
  }

  session->my_eof();
  return(false);

 err:
  session->close_thread_tables();			// Shouldn't be needed
  if (table)
    table->table=0;
  return(true);
}
