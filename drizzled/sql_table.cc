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
#include <storage/myisam/myisam.h>
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
#include <drizzled/replicator.h>
#include <drizzled/lock.h>
#include <drizzled/unireg.h>

extern HASH lock_db_cache;

int creating_table= 0;        // How many mysql_create_table are running


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
static int copy_data_between_tables(Table *from,Table *to,
                                    List<Create_field> &create, bool ignore,
                                    uint32_t order_num, order_st *order,
                                    ha_rows *copied,ha_rows *deleted,
                                    enum enum_enable_or_disable keys_onoff,
                                    bool error_if_not_empty);

static bool prepare_blob_field(Session *session, Create_field *sql_field);
static bool check_engine(Session *, const char *, HA_CREATE_INFO *);
static int
mysql_prepare_create_table(Session *session, HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool tmp_table,
                               uint32_t *db_options,
                               handler *file, KEY **key_info_buffer,
                               uint32_t *key_count, int select_field_count);
static bool
mysql_prepare_alter_table(Session *session, Table *table,
                          HA_CREATE_INFO *create_info,
                          Alter_info *alter_info);

/*
  Translate a file name to a table name (WL #1324).

  SYNOPSIS
    filename_to_tablename()
      from                      The file name in my_charset_filename.
      to                OUT     The table name in system_charset_info.
      to_length                 The size of the table name buffer.

  RETURN
    Table name length.
*/
uint32_t filename_to_tablename(const char *from, char *to, uint32_t to_length)
{
  uint32_t errors;
  uint32_t res;

  if (!memcmp(from, TMP_FILE_PREFIX, TMP_FILE_PREFIX_LENGTH))
  {
    /* Temporary table name. */
    res= (my_stpncpy(to, from, to_length) - to);
  }
  else
  {
    res= strconvert(&my_charset_filename, from,
                    system_charset_info,  to, to_length, &errors);
    if (errors) // Old 5.0 name
    {
      strcpy(to, MYSQL50_TABLE_NAME_PREFIX);
      strncat(to, from, to_length-MYSQL50_TABLE_NAME_PREFIX_LENGTH-1);
      res= strlen(to);
      sql_print_error(_("Invalid (old?) table or database name '%s'"), from);
    }
  }

  return(res);
}


/*
  Translate a table name to a file name (WL #1324).

  SYNOPSIS
    tablename_to_filename()
      from                      The table name in system_charset_info.
      to                OUT     The file name in my_charset_filename.
      to_length                 The size of the file name buffer.

  RETURN
    File name length.
*/
uint32_t tablename_to_filename(const char *from, char *to, uint32_t to_length)
{
  uint32_t errors, length;

  if (from[0] == '#' && !strncmp(from, MYSQL50_TABLE_NAME_PREFIX,
                                 MYSQL50_TABLE_NAME_PREFIX_LENGTH))
    return((uint) (strncpy(to, from+MYSQL50_TABLE_NAME_PREFIX_LENGTH,
                           to_length-1) -
                           (from + MYSQL50_TABLE_NAME_PREFIX_LENGTH)));
  length= strconvert(system_charset_info, from,
                     &my_charset_filename, to, to_length, &errors);
  if (check_if_legal_tablename(to) &&
      length + 4 < to_length)
  {
    memcpy(to + length, "@@@", 4);
    length+= 3;
  }
  return(length);
}


/*
  Creates path to a file: drizzle_data_dir/db/table.ext

  SYNOPSIS
   build_table_filename()
     buff                       Where to write result in my_charset_filename.
                                This may be the same as table_name.
     bufflen                    buff size
     db                         Database name in system_charset_info.
     table_name                 Table name in system_charset_info.
     ext                        File extension.
     flags                      FN_FROM_IS_TMP or FN_TO_IS_TMP or FN_IS_TMP
                                table_name is temporary, do not change.

  NOTES

    Uses database and table name, and extension to create
    a file name in drizzle_data_dir. Database and table
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
    path length
*/

uint32_t build_table_filename(char *buff, size_t bufflen, const char *db,
                          const char *table_name, const char *ext, uint32_t flags)
{
  char dbbuff[FN_REFLEN];
  char tbbuff[FN_REFLEN];

  if (flags & FN_IS_TMP) // FN_FROM_IS_TMP | FN_TO_IS_TMP
    my_stpncpy(tbbuff, table_name, sizeof(tbbuff));
  else
    tablename_to_filename(table_name, tbbuff, sizeof(tbbuff));

  tablename_to_filename(db, dbbuff, sizeof(dbbuff));

  char *end = buff + bufflen;
  /* Don't add FN_ROOTDIR if dirzzle_data_home already includes it */
  char *pos = my_stpncpy(buff, drizzle_data_home, bufflen);
  int rootdir_len= strlen(FN_ROOTDIR);
  if (pos - rootdir_len >= buff &&
      memcmp(pos - rootdir_len, FN_ROOTDIR, rootdir_len) != 0)
    pos= my_stpncpy(pos, FN_ROOTDIR, end - pos);
  pos= my_stpncpy(pos, dbbuff, end-pos);
  pos= my_stpncpy(pos, FN_ROOTDIR, end-pos);
#ifdef USE_SYMDIR
  unpack_dirname(buff, buff);
  pos= strend(buff);
#endif
  pos= my_stpncpy(pos, tbbuff, end - pos);
  pos= my_stpncpy(pos, ext, end - pos);

  return(pos - buff);
}


/*
  Creates path to a file: drizzle_tmpdir/#sql1234_12_1.ext

  SYNOPSIS
   build_tmptable_filename()
     session                        The thread handle.
     buff                       Where to write result in my_charset_filename.
     bufflen                    buff size

  NOTES

    Uses current_pid, thread_id, and tmp_table counter to create
    a file name in drizzle_tmpdir.

  RETURN
    path length
*/

uint32_t build_tmptable_filename(Session* session, char *buff, size_t bufflen)
{

  char *p= my_stpncpy(buff, drizzle_tmpdir, bufflen);
  snprintf(p, bufflen - (p - buff), "/%s%lx_%"PRIx64"_%x%s",
           TMP_FILE_PREFIX, (unsigned long)current_pid,
           session->thread_id, session->tmp_table++, reg_ext);

  if (lower_case_table_names)
  {
    /* Convert all except tmpdir to lower case */
    my_casedn_str(files_charset_info, p);
  }

  uint32_t length= unpack_filename(buff, buff);
  return(length);
}

/*
  SYNOPSIS
    write_bin_log()
    session                           Thread object
    clear_error                   is clear_error to be called
    query                         Query to log
    query_length                  Length of query

  RETURN VALUES
    NONE

  DESCRIPTION
    Write the binlog if open, routine used in multiple places in this
    file
*/

void write_bin_log(Session *session, bool,
                   char const *query, size_t query_length)
{
  (void)replicator_statement(session, query, query_length);
}


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

bool mysql_rm_table(Session *session,TableList *tables, bool if_exists, bool drop_temporary)
{
  bool error, need_start_waiting= false;

  if (tables && tables->schema_table)
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), "", "", INFORMATION_SCHEMA_NAME.c_str());
    return(true);
  }

  /* mark for close and remove all cached entries */

  if (!drop_temporary)
  {
    if (!session->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(session, 0, 1)))
      return(true);
  }

  /*
    Acquire LOCK_open after wait_if_global_read_lock(). If we would hold
    LOCK_open during wait_if_global_read_lock(), other threads could not
    close their tables. This would make a pretty deadlock.
  */
  error= mysql_rm_table_part2(session, tables, if_exists, drop_temporary, 0);

  if (need_start_waiting)
    start_waiting_global_read_lock(session);

  if (error)
    return(true);
  my_ok(session);
  return(false);
}

/*
  Execute the drop of a normal or temporary table

  SYNOPSIS
    mysql_rm_table_part2()
    session			Thread handler
    tables		Tables to drop
    if_exists		If set, don't give an error if table doesn't exists.
			In this case we give an warning of level 'NOTE'
    drop_temporary	Only drop temporary tables
    drop_view		Allow to delete VIEW .frm
    dont_log_query	Don't write query to log files. This will also not
			generate warnings if the handler files doesn't exists

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
                         bool drop_temporary, bool dont_log_query)
{
  TableList *table;
  char path[FN_REFLEN], *alias;
  uint32_t path_length;
  String wrong_tables;
  int error= 0;
  int non_temp_tables_count= 0;
  bool some_tables_deleted=0, tmp_table_deleted=0, foreign_key_error=0;
  String built_query;

  if (!dont_log_query)
  {
    built_query.set_charset(system_charset_info);
    if (if_exists)
      built_query.append("DROP Table IF EXISTS ");
    else
      built_query.append("DROP Table ");
  }

  mysql_ha_rm_tables(session, tables, false);

  pthread_mutex_lock(&LOCK_open);

  /*
    If we have the table in the definition cache, we don't have to check the
    .frm file to find if the table is a normal table (not view) and what
    engine to use.
  */

  for (table= tables; table; table= table->next_local)
  {
    TABLE_SHARE *share;
    table->db_type= NULL;
    if ((share= get_cached_table_share(table->db, table->table_name)))
      table->db_type= share->db_type();
  }

  if (!drop_temporary && lock_table_names_exclusively(session, tables))
  {
    pthread_mutex_unlock(&LOCK_open);
    return(1);
  }

  /* Don't give warnings for not found errors, as we already generate notes */
  session->no_warnings_for_error= 1;

  for (table= tables; table; table= table->next_local)
  {
    char *db=table->db;
    handlerton *table_type;

    error= drop_temporary_table(session, table);

    switch (error) {
    case  0:
      // removed temporary table
      tmp_table_deleted= 1;
      continue;
    case -1:
      error= 1;
      goto err_with_placeholders;
    default:
      // temporary table not found
      error= 0;
    }

    /*
      If row-based replication is used and the table is not a
      temporary table, we add the table name to the drop statement
      being built.  The string always end in a comma and the comma
      will be chopped off before being written to the binary log.
      */
    if (!dont_log_query)
    {
      non_temp_tables_count++;
      /*
        Don't write the database name if it is the current one (or if
        session->db is NULL).
      */
      built_query.append("`");
      if (session->db == NULL || strcmp(db,session->db) != 0)
      {
        built_query.append(db);
        built_query.append("`.`");
      }

      built_query.append(table->table_name);
      built_query.append("`,");
    }

    table_type= table->db_type;
    if (!drop_temporary)
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
      alias= (lower_case_table_names == 2) ? table->alias : table->table_name;
      /* remove .frm file and engine files */
      path_length= build_table_filename(path, sizeof(path), db, alias, reg_ext,
                                        table->internal_tmp_table ?
                                        FN_IS_TMP : 0);
    }
    if (drop_temporary ||
        ((table_type == NULL && (access(path, F_OK) && ha_create_table_from_engine(session, db, alias))))
        )
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
      char *end;
      // Remove extension for delete
      *(end= path + path_length - reg_ext_length)= '\0';
      error= ha_delete_table(session, path, db, table->table_name,
                             !dont_log_query);
      if ((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE) &&
	  if_exists)
      {
	error= 0;
        session->clear_error();
      }
      if (error == HA_ERR_ROW_IS_REFERENCED)
      {
        /* the table is referenced by a foreign key constraint */
        foreign_key_error=1;
      }
      if (!error || error == ENOENT || error == HA_ERR_NO_SUCH_TABLE)
      {
        int new_error;

        /* for some weird-ass reason, we ignore the return code here
           and things work. */
        delete_table_proto_file(path);

        /* Delete the table definition file */
        strcpy(end,reg_ext);
        if (!(new_error=my_delete(path,MYF(MY_WME))))
        {
          some_tables_deleted=1;
          new_error= 0;
        }
        error|= new_error;
      }
    }
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
  session->thread_specific_used|= tmp_table_deleted;
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

  if (some_tables_deleted || tmp_table_deleted || !error)
  {
    if (!dont_log_query)
    {
      if ((non_temp_tables_count > 0 && !tmp_table_deleted))
      {
        /*
          In this case, we are either using statement-based
          replication or using row-based replication but have only
          deleted one or more non-temporary tables (and no temporary
          tables).  In this case, we can write the original query into
          the binary log.
         */
        write_bin_log(session, !error, session->query, session->query_length);
      }
      else if (non_temp_tables_count > 0 &&
               tmp_table_deleted)
      {
        /*
          In this case we have deleted both temporary and
          non-temporary tables, so:
          - since we have deleted a non-temporary table we have to
            binlog the statement, but
          - since we have deleted a temporary table we cannot binlog
            the statement (since the table has not been created on the
            slave, this might cause the slave to stop).

          Instead, we write a built statement, only containing the
          non-temporary tables, to the binary log
        */
        built_query.chop();                  // Chop of the last comma
        built_query.append(" /* generated by server */");
        write_bin_log(session, !error, built_query.ptr(), built_query.length());
      }
      /*
        The remaining cases are:
        - no tables where deleted and
        - only temporary tables where deleted and row-based
          replication is used.
        In both these cases, nothing should be written to the binary
        log.
      */
    }
  }
  pthread_mutex_lock(&LOCK_open);
err_with_placeholders:
  unlock_table_names(session, tables, (TableList*) 0);
  pthread_mutex_unlock(&LOCK_open);
  session->no_warnings_for_error= 0;
  return(error);
}


/*
  Quickly remove a table.

  SYNOPSIS
    quick_rm_table()
      base                      The handlerton handle.
      db                        The database name.
      table_name                The table name.
      flags                     flags for build_table_filename().

  RETURN
    0           OK
    != 0        Error
*/

bool quick_rm_table(handlerton *base __attribute__((unused)),const char *db,
                    const char *table_name, uint32_t flags)
{
  char path[FN_REFLEN];
  bool error= 0;

  uint32_t path_length= build_table_filename(path, sizeof(path),
                                         db, table_name, reg_ext, flags);
  if (my_delete(path,MYF(0)))
    error= 1; /* purecov: inspected */

  path[path_length - reg_ext_length]= '\0'; // Remove reg_ext

  error|= delete_table_proto_file(path);

  return(ha_delete_table(current_session, path, db, table_name, 0) ||
              error);
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

bool check_duplicates_in_interval(const char *set_or_name,
                                  const char *name, TYPELIB *typelib,
                                  const CHARSET_INFO * const cs, unsigned int *dup_val_count)
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
void calculate_interval_lengths(const CHARSET_INFO * const cs, TYPELIB *interval,
                                uint32_t *max_length, uint32_t *tot_length)
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
    This function prepares a Create_field instance.
    Fields such as pack_flag are valid after this call.

  RETURN VALUES
   0	ok
   1	Error
*/

int prepare_create_field(Create_field *sql_field,
                         uint32_t *blob_columns,
                         int *timestamps, int *timestamps_with_niladic,
                         int64_t table_flags __attribute__((unused)))
{
  unsigned int dup_val_count;

  /*
    This code came from mysql_prepare_create_table.
    Indent preserved to make patching easier
  */
  assert(sql_field->charset);

  switch (sql_field->sql_type) {
  case DRIZZLE_TYPE_BLOB:
    sql_field->pack_flag=FIELDFLAG_BLOB |
      pack_length_to_packflag(sql_field->pack_length -
                              portable_sizeof_char_ptr);
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->length=8;			// Unireg field length
    sql_field->unireg_check=Field::BLOB_FIELD;
    (*blob_columns)++;
    break;
  case DRIZZLE_TYPE_VARCHAR:
    sql_field->pack_flag=0;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    break;
  case DRIZZLE_TYPE_ENUM:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
      FIELDFLAG_INTERVAL;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->unireg_check=Field::INTERVAL_FIELD;
    if (check_duplicates_in_interval("ENUM",sql_field->field_name,
                                 sql_field->interval,
                                     sql_field->charset, &dup_val_count))
      return(1);
    break;
  case DRIZZLE_TYPE_DATE:  // Rest of string types
  case DRIZZLE_TYPE_TIME:
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_NULL:
    sql_field->pack_flag=f_settype((uint) sql_field->sql_type);
    break;
  case DRIZZLE_TYPE_NEWDECIMAL:
    sql_field->pack_flag=(FIELDFLAG_NUMBER |
                          (sql_field->flags & UNSIGNED_FLAG ? 0 :
                           FIELDFLAG_DECIMAL) |
                          (sql_field->flags & DECIMAL_FLAG ?  FIELDFLAG_DECIMAL_POSITION : 0) |
                          (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
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
    sql_field->pack_flag=(FIELDFLAG_NUMBER |
                          (sql_field->flags & UNSIGNED_FLAG ? 0 :
                           FIELDFLAG_DECIMAL) |
                          f_settype((uint) sql_field->sql_type) |
                          (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
    break;
  }
  if (!(sql_field->flags & NOT_NULL_FLAG) ||
      (sql_field->vcol_info)) /* Make virtual columns always allow NULL values */
    sql_field->pack_flag|= FIELDFLAG_MAYBE_NULL;
  if (sql_field->flags & NO_DEFAULT_VALUE_FLAG)
    sql_field->pack_flag|= FIELDFLAG_NO_DEFAULT;
  return(0);
}

/*
  Preparation for table creation

  SYNOPSIS
    mysql_prepare_create_table()
      session                       Thread object.
      create_info               Create information (like MAX_ROWS).
      alter_info                List of columns and indexes to create
      tmp_table                 If a temporary table is to be created.
      db_options          INOUT Table options (like HA_OPTION_PACK_RECORD).
      file                      The handler for the new table.
      key_info_buffer     OUT   An array of KEY structs for the indexes.
      key_count           OUT   The number of elements in the array.
      select_field_count        The number of fields coming from a select table.

  DESCRIPTION
    Prepares the table and key structures for table creation.

  NOTES
    sets create_info->varchar if the table has a varchar

  RETURN VALUES
    false    OK
    true     error
*/

static int
mysql_prepare_create_table(Session *session, HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool tmp_table,
                               uint32_t *db_options,
                               handler *file, KEY **key_info_buffer,
                               uint32_t *key_count, int select_field_count)
{
  const char	*key_name;
  Create_field	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  ulong		record_offset= 0;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  List_iterator<Create_field> it(alter_info->create_list);
  List_iterator<Create_field> it2(alter_info->create_list);
  uint32_t total_uneven_bit_length= 0;

  select_field_pos= alter_info->create_list.elements - select_field_count;
  null_fields=blob_columns=0;
  create_info->varchar= 0;
  max_key_length= file->max_key_length();

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
        !(sql_field->charset= get_charset_by_csname(sql_field->charset->csname,
                                                    MY_CS_BINSORT,MYF(0))))
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
        Starting from 5.1 we work here with a copy of Create_field
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
      set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
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
          sql_field->vcol_info=         dup_field->vcol_info;
          sql_field->is_stored=      dup_field->is_stored;
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
			     file->ha_table_flags()))
      return(true);
    if (sql_field->sql_type == DRIZZLE_TYPE_VARCHAR)
      create_info->varchar= true;
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    /*
          For now skip fields that are not physically stored in the database
          (virtual fields) and update their offset later
          (see the next loop).
        */
    if (sql_field->is_stored)
      record_offset+= sql_field->pack_length;
  }
  /* Update virtual fields' offset */
  it.rewind();
  while ((sql_field=it++))
  {
    if (not sql_field->is_stored)
    {
      sql_field->offset= record_offset;
      record_offset+= sql_field->pack_length;
    }
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
      (file->ha_table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_message(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,
               ER(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT), MYF(0));
    return(true);
  }

  if (blob_columns && (file->ha_table_flags() & HA_NO_BLOBS))
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
    tmp=file->max_key_parts();
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
  tmp=file->max_keys();
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
                           create_info->key_block_size);

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
               (uint) INDEX_COMMENT_MAXLEN);
      return(-1);
    }

    key_info->comment.length= key->key_create_info.comment.length;
    if (key_info->comment.length > 0)
    {
      key_info->flags|= HA_USES_COMMENT;
      key_info->comment.str= key->key_create_info.comment.str;
    }

    List_iterator<Key_part_spec> cols(key->columns), cols2(key->columns);
    for (uint32_t column_nr=0 ; (column=cols++) ; column_nr++)
    {
      uint32_t length;
      Key_part_spec *dup_column;

      it.rewind();
      field=0;
      while ((sql_field=it++) &&
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
      {
	column->length*= sql_field->charset->mbmaxlen;

	if (f_is_blob(sql_field->pack_flag))
	{
	  if (!(file->ha_table_flags() & HA_CAN_INDEX_BLOBS))
	  {
	    my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name.str);
	    return(true);
	  }
	  if (!column->length)
	  {
	    my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name.str);
	    return(true);
	  }
	}
        if (not sql_field->is_stored)
        {
          /* Key fields must always be physically stored. */
          my_error(ER_KEY_BASED_ON_GENERATED_VIRTUAL_COLUMN, MYF(0));
          return(true);
        }
        if (key->type == Key::PRIMARY && sql_field->vcol_info)
        {
          my_error(ER_PRIMARY_KEY_BASED_ON_VIRTUAL_COLUMN, MYF(0));
          return(true);
        }
	if (!(sql_field->flags & NOT_NULL_FLAG))
	{
	  if (key->type == Key::PRIMARY)
	  {
	    /* Implicitly set primary key fields to NOT NULL for ISO conf. */
	    sql_field->flags|= NOT_NULL_FLAG;
	    sql_field->pack_flag&= ~FIELDFLAG_MAYBE_NULL;
            null_fields--;
	  }
	  else
          {
            key_info->flags|= HA_NULL_PART_KEY;
            if (!(file->ha_table_flags() & HA_NULL_IN_KEY))
            {
              my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name.str);
              return(true);
            }
          }
	}
	if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
	{
	  if (column_nr == 0 || (file->ha_table_flags() & HA_AUTO_PART_KEY))
	    auto_increment--;			// Field is used
	}
      }

      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16_t) sql_field->offset;
      key_part_info->key_type=sql_field->pack_flag;
      length= sql_field->key_length;

      if (column->length)
      {
	if (f_is_blob(sql_field->pack_flag))
	{
	  if ((length=column->length) > max_key_length ||
	      length > file->max_key_part_length())
	  {
	    length=cmin(max_key_length, file->max_key_part_length());
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
                   !Field::type_can_have_key_part (sql_field->sql_type) ||
		   ((f_is_packed(sql_field->pack_flag) ||
		     ((file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
		      (key_info->flags & HA_NOSAME))) &&
		    column->length != length)))
	{
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  return(true);
	}
	else if (!(file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  length=column->length;
      }
      else if (length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name.str);
	  return(true);
      }
      if (length > file->max_key_part_length())
      {
        length= file->max_key_part_length();
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
	    sql_field->pack_flag & FIELDFLAG_BLOB)))
      {
	if ((column_nr == 0 && (sql_field->pack_flag & FIELDFLAG_BLOB)) ||
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
      (file->ha_table_flags() & HA_REQUIRE_PRIMARY_KEY))
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
  create_info->null_bits= null_fields;

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
  Set table default charset, if not set

  SYNOPSIS
    set_table_default_charset()
    create_info        Table create information

  DESCRIPTION
    If the table character set was not given explicitely,
    let's fetch the database default character set and
    apply it to the table.
*/

static void set_table_default_charset(Session *session,
                                      HA_CREATE_INFO *create_info, char *db)
{
  /*
    If the table character set was not given explicitly,
    let's fetch the database default character set and
    apply it to the table.
  */
  if (!create_info->default_table_charset)
  {
    HA_CREATE_INFO db_info;

    load_db_opt_by_name(session, db, &db_info);

    create_info->default_table_charset= db_info.default_table_charset;
  }
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

static bool prepare_blob_field(Session *session __attribute__((unused)),
                               Create_field *sql_field)
{

  if (sql_field->length > MAX_FIELD_VARCHARLENGTH &&
      !(sql_field->flags & BLOB_FLAG))
  {
    my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sql_field->field_name,
             MAX_FIELD_VARCHARLENGTH / sql_field->charset->mbmaxlen);
    return(1);
  }

  if ((sql_field->flags & BLOB_FLAG) && sql_field->length)
  {
    if (sql_field->sql_type == DRIZZLE_TYPE_BLOB)
    {
      /* The user has given a length to the blob column */
      sql_field->sql_type= get_blob_type_from_length(sql_field->length);
      sql_field->pack_length= calc_pack_length(sql_field->sql_type, 0);
    }
    sql_field->length= 0;
  }
  return(0);
}


/*
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

    no_log is needed for the case of CREATE ... SELECT,
    as the logging will be done later in sql_insert.cc
    select_field_count is also used for CREATE ... SELECT,
    and must be zero for standard create of table.

  RETURN VALUES
    false OK
    true  error
*/

bool mysql_create_table_no_lock(Session *session,
                                const char *db, const char *table_name,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                bool internal_tmp_table,
                                uint32_t select_field_count,
                                bool lock_open_lock)
{
  char		path[FN_REFLEN];
  uint32_t          path_length;
  const char	*alias;
  uint		db_options, key_count;
  KEY		*key_info_buffer;
  handler	*file;
  bool		error= true;
  /* Check for duplicate fields and check type of table to create */
  if (!alter_info->create_list.elements)
  {
    my_message(ER_TABLE_MUST_HAVE_COLUMNS, ER(ER_TABLE_MUST_HAVE_COLUMNS),
               MYF(0));
    return(true);
  }
  if (check_engine(session, table_name, create_info))
    return(true);
  db_options= create_info->table_options;
  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    db_options|=HA_OPTION_PACK_RECORD;
  alias= table_case_name(create_info, table_name);
  if (!(file= get_new_handler((TABLE_SHARE*) 0, session->mem_root,
                              create_info->db_type)))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(handler));
    return(true);
  }

  set_table_default_charset(session, create_info, (char*) db);

  if (mysql_prepare_create_table(session, create_info, alter_info,
                                 internal_tmp_table,
                                 &db_options, file,
			  &key_info_buffer, &key_count,
			  select_field_count))
    goto err;

      /* Check if table exists */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    path_length= build_tmptable_filename(session, path, sizeof(path));
    create_info->table_options|=HA_CREATE_DELAY_KEY_WRITE;
  }
  else
  {
 #ifdef FN_DEVCHAR
    /* check if the table name contains FN_DEVCHAR when defined */
    if (strchr(alias, FN_DEVCHAR))
    {
      my_error(ER_WRONG_TABLE_NAME, MYF(0), alias);
      return(true);
    }
#endif
    path_length= build_table_filename(path, sizeof(path), db, alias, reg_ext,
                                      internal_tmp_table ? FN_IS_TMP : 0);
  }

  /* Check if table already exists */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      find_temporary_table(session, db, table_name))
  {
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      create_info->table_existed= 1;		// Mark that table existed
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          alias);
      error= 0;
      goto err;
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alias);
    goto err;
  }

  if (lock_open_lock)
    pthread_mutex_lock(&LOCK_open);
  if (!internal_tmp_table && !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    if (!access(path,F_OK))
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto unlock_and_end;
    }
    /*
      We don't assert here, but check the result, because the table could be
      in the table definition cache and in the same time the .frm could be
      missing from the disk, in case of manual intervention which deletes
      the .frm file. The user has to use FLUSH TABLES; to clear the cache.
      Then she could create the table. This case is pretty obscure and
      therefore we don't introduce a new error message only for it.
    */
    if (get_cached_table_share(db, alias))
    {
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);
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
    it's not on disk as a frm file, no one could be using it!
  */
  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    bool create_if_not_exists =
      create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS;
    int retcode = ha_table_exists_in_engine(session, db, table_name);
    switch (retcode)
    {
      case HA_ERR_NO_SUCH_TABLE:
        /* Normal case, no table exists. we can go and create it */
        break;
      case HA_ERR_TABLE_EXIST:

      if (create_if_not_exists)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto unlock_and_end;
        break;
      default:
        my_error(retcode, MYF(0),table_name);
        goto unlock_and_end;
    }
  }

  session->set_proc_info("creating table");
  create_info->table_existed= 0;		// Mark that table is created

#ifdef HAVE_READLINK
  if (test_if_data_home_dir(create_info->data_file_name))
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "DATA DIRECTORY");
    goto unlock_and_end;
  }
  if (test_if_data_home_dir(create_info->index_file_name))
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "INDEX DIRECTORY");
    goto unlock_and_end;
  }

  if (!my_use_symdir)
#endif /* HAVE_READLINK */
  {
    if (create_info->data_file_name)
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, 0,
                   "DATA DIRECTORY option ignored");
    if (create_info->index_file_name)
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, 0,
                   "INDEX DIRECTORY option ignored");
    create_info->data_file_name= create_info->index_file_name= 0;
  }
  create_info->table_options=db_options;

  path[path_length - reg_ext_length]= '\0'; // Remove .frm extension
  if (rea_create_table(session, path, db, table_name,
                       create_info, alter_info->create_list,
                       key_count, key_info_buffer, file))
    goto unlock_and_end;

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    /* Open table and put in temporary table list */
    if (!(open_temporary_table(session, path, db, table_name, 1, OTM_OPEN)))
    {
      (void) rm_temporary_table(create_info->db_type, path);
      goto unlock_and_end;
    }
    session->thread_specific_used= true;
  }

  /*
    Don't write statement if:
    - It is an internal temporary table,
    - Row-based logging is used and it we are creating a temporary table, or
    - The binary log is not open.
    Otherwise, the statement shall be binlogged.
   */
  if (!internal_tmp_table &&
      ((!(create_info->options & HA_LEX_CREATE_TMP_TABLE))))
    write_bin_log(session, true, session->query, session->query_length);
  error= false;
unlock_and_end:
  if (lock_open_lock)
    pthread_mutex_unlock(&LOCK_open);

err:
  session->set_proc_info("After create");
  delete file;
  return(error);

warn:
  error= false;
  push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                      ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                      alias);
  create_info->table_existed= 1;		// Mark that table existed
  goto unlock_and_end;
}


/*
  Database locking aware wrapper for mysql_create_table_no_lock(),
*/

bool mysql_create_table(Session *session, const char *db, const char *table_name,
                        HA_CREATE_INFO *create_info,
                        Alter_info *alter_info,
                        bool internal_tmp_table,
                        uint32_t select_field_count)
{
  Table *name_lock= 0;
  bool result;

  /* Wait for any database locks */
  pthread_mutex_lock(&LOCK_lock_db);
  while (!session->killed &&
         hash_search(&lock_db_cache,(unsigned char*) db, strlen(db)))
  {
    wait_for_condition(session, &LOCK_lock_db, &COND_refresh);
    pthread_mutex_lock(&LOCK_lock_db);
  }

  if (session->killed)
  {
    pthread_mutex_unlock(&LOCK_lock_db);
    return(true);
  }
  creating_table++;
  pthread_mutex_unlock(&LOCK_lock_db);

  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    if (lock_table_name_if_not_cached(session, db, table_name, &name_lock))
    {
      result= true;
      goto unlock;
    }
    if (!name_lock)
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
      {
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                            table_name);
        create_info->table_existed= 1;
        result= false;
      }
      else
      {
        my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
        result= true;
      }
      goto unlock;
    }
  }

  result= mysql_create_table_no_lock(session, db, table_name, create_info,
                                     alter_info,
                                     internal_tmp_table,
                                     select_field_count, true);

unlock:
  if (name_lock)
  {
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(session, name_lock, false);
    pthread_mutex_unlock(&LOCK_open);
  }
  pthread_mutex_lock(&LOCK_lock_db);
  if (!--creating_table && creating_database)
    pthread_cond_signal(&COND_refresh);
  pthread_mutex_unlock(&LOCK_lock_db);
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
      base                      The handlerton handle.
      old_db                    The old database name.
      old_name                  The old table name.
      new_db                    The new database name.
      new_name                  The new table name.
      flags                     flags for build_table_filename().
                                FN_FROM_IS_TMP old_name is temporary.
                                FN_TO_IS_TMP   new_name is temporary.
                                NO_FRM_RENAME  Don't rename the FRM file
                                but only the table in the storage engine.

  RETURN
    false   OK
    true    Error
*/

bool
mysql_rename_table(handlerton *base, const char *old_db,
                   const char *old_name, const char *new_db,
                   const char *new_name, uint32_t flags)
{
  Session *session= current_session;
  char from[FN_REFLEN], to[FN_REFLEN], lc_from[FN_REFLEN], lc_to[FN_REFLEN];
  char *from_base= from, *to_base= to;
  char tmp_name[NAME_LEN+1];
  handler *file;
  int error=0;

  file= (base == NULL ? 0 :
         get_new_handler((TABLE_SHARE*) 0, session->mem_root, base));

  build_table_filename(from, sizeof(from), old_db, old_name, "",
                       flags & FN_FROM_IS_TMP);
  build_table_filename(to, sizeof(to), new_db, new_name, "",
                       flags & FN_TO_IS_TMP);

  /*
    If lower_case_table_names == 2 (case-preserving but case-insensitive
    file system) and the storage is not HA_FILE_BASED, we need to provide
    a lowercase file name, but we leave the .frm in mixed case.
   */
  if (lower_case_table_names == 2 && file &&
      !(file->ha_table_flags() & HA_FILE_BASED))
  {
    strcpy(tmp_name, old_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_filename(lc_from, sizeof(lc_from), old_db, tmp_name, "",
                         flags & FN_FROM_IS_TMP);
    from_base= lc_from;

    strcpy(tmp_name, new_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_filename(lc_to, sizeof(lc_to), new_db, tmp_name, "",
                         flags & FN_TO_IS_TMP);
    to_base= lc_to;
  }
  if (!file || !(error=file->ha_rename_table(from_base, to_base)))
  {
    if (!(flags & NO_FRM_RENAME) && rename_file_ext(from,to,reg_ext))
    {
      error=my_errno;
      /* Restore old file name */
      if (file)
        file->ha_rename_table(to_base, from_base);
    }

    if(!(flags & NO_FRM_RENAME)
       && rename_table_proto_file(from_base, to_base))
    {
      error= errno;
      rename_file_ext(to, from, reg_ext);
      if (file)
        file->ha_rename_table(to_base, from_base);
    }
  }
  delete file;
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
    session			Thread handler
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

  table->file->extra(function);
  /* Mark all tables that are in use as 'old' */
  mysql_lock_abort(session, table, true);	/* end threads waiting on lock */

  /* Wait until all there are no other threads that has this table open */
  remove_table_from_cache(session, table->s->db.str,
                          table->s->table_name.str,
                          RTFC_WAIT_OTHER_THREAD_FLAG);
  return;
}

/*
  Close a cached table

  SYNOPSIS
    close_cached_table()
    session			Thread handler
    table		Table to remove from cache

  NOTES
    Function ends by signaling threads waiting for the table to try to
    reopen the table.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

void close_cached_table(Session *session, Table *table)
{

  wait_while_table_is_used(session, table, HA_EXTRA_FORCE_REOPEN);
  /* Close lock if this is not got with LOCK TABLES */
  if (session->lock)
  {
    mysql_unlock_tables(session, session->lock);
    session->lock=0;			// Start locked threads
  }
  /* Close all copies of 'table'.  This also frees all LOCK TABLES lock */
  unlink_open_table(session, table, true);

  /* When lock on LOCK_open is freed other threads can continue */
  broadcast_refresh();
  return;
}

static int send_check_errmsg(Session *session, TableList* table,
			     const char* operator_name, const char* errmsg)

{
  Protocol *protocol= session->protocol;
  protocol->prepare_for_resend();
  protocol->store(table->alias, system_charset_info);
  protocol->store((char*) operator_name, system_charset_info);
  protocol->store(STRING_WITH_LEN("error"), system_charset_info);
  protocol->store(errmsg, system_charset_info);
  session->clear_error();
  if (protocol->write())
    return -1;
  return 1;
}


static int prepare_for_repair(Session *session, TableList *table_list,
			      HA_CHECK_OPT *check_opt)
{
  int error= 0;
  Table tmp_table, *table;
  TABLE_SHARE *share;
  char from[FN_REFLEN],tmp[FN_REFLEN+32];
  const char **ext;
  struct stat stat_info;

  if (!(check_opt->sql_flags & TT_USEFRM))
    return(0);

  if (!(table= table_list->table))		/* if open_ltable failed */
  {
    char key[MAX_DBKEY_LENGTH];
    uint32_t key_length;

    key_length= create_table_def_key(session, key, table_list, 0);
    pthread_mutex_lock(&LOCK_open);
    if (!(share= (get_table_share(session, table_list, key, key_length, 0,
                                  &error))))
    {
      pthread_mutex_unlock(&LOCK_open);
      return(0);				// Can't open frm file
    }

    if (open_table_from_share(session, share, "", 0, 0, 0, &tmp_table, OTM_OPEN))
    {
      release_table_share(share, RELEASE_NORMAL);
      pthread_mutex_unlock(&LOCK_open);
      return(0);                           // Out of memory
    }
    table= &tmp_table;
    pthread_mutex_unlock(&LOCK_open);
  }

  /*
    REPAIR Table ... USE_FRM for temporary tables makes little sense.
  */
  if (table->s->tmp_table)
  {
    error= send_check_errmsg(session, table_list, "repair",
			     "Cannot repair temporary table from .frm file");
    goto end;
  }

  /*
    User gave us USE_FRM which means that the header in the index file is
    trashed.
    In this case we will try to fix the table the following way:
    - Rename the data file to a temporary name
    - Truncate the table
    - Replace the new data file with the old one
    - Run a normal repair using the new index file and the old data file
  */

  /*
    Check if this is a table type that stores index and data separately,
    like ISAM or MyISAM. We assume fixed order of engine file name
    extentions array. First element of engine file name extentions array
    is meta/index file extention. Second element - data file extention.
  */
  ext= table->file->bas_ext();
  if (!ext[0] || !ext[1])
    goto end;					// No data file

  // Name of data file
  strxmov(from, table->s->normalized_path.str, ext[1], NULL);
  if (stat(from, &stat_info))
    goto end;				// Can't use USE_FRM flag

  snprintf(tmp, sizeof(tmp), "%s-%lx_%"PRIx64,
           from, (unsigned long)current_pid, session->thread_id);

  /* If we could open the table, close it */
  if (table_list->table)
  {
    pthread_mutex_lock(&LOCK_open);
    close_cached_table(session, table);
    pthread_mutex_unlock(&LOCK_open);
  }
  if (lock_and_wait_for_table_name(session,table_list))
  {
    error= -1;
    goto end;
  }
  if (my_rename(from, tmp, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(session, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(session, table_list, "repair",
			     "Failed renaming data file");
    goto end;
  }
  if (mysql_truncate(session, table_list, 1))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(session, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(session, table_list, "repair",
			     "Failed generating table from .frm file");
    goto end;
  }
  if (my_rename(tmp, from, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(session, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(session, table_list, "repair",
			     "Failed restoring .MYD file");
    goto end;
  }

  /*
    Now we should be able to open the partially repaired table
    to finish the repair in the handler later on.
  */
  pthread_mutex_lock(&LOCK_open);
  if (reopen_name_locked_table(session, table_list, true))
  {
    unlock_table_name(session, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(session, table_list, "repair",
                             "Failed to open partially repaired table");
    goto end;
  }
  pthread_mutex_unlock(&LOCK_open);

end:
  if (table == &tmp_table)
  {
    pthread_mutex_lock(&LOCK_open);
    closefrm(table, 1);				// Free allocated memory
    pthread_mutex_unlock(&LOCK_open);
  }
  return(error);
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
                              int (handler::*operator_func)(Session *,
                                                            HA_CHECK_OPT *))
{
  TableList *table;
  SELECT_LEX *select= &session->lex->select_lex;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= session->protocol;
  LEX *lex= session->lex;
  int result_code= 0;
  const CHARSET_INFO * const cs= system_charset_info;

  if (end_active_trans(session))
    return(1);
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
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return(true);

  mysql_ha_rm_tables(session, tables, false);

  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    char* db = table->db;
    bool fatal_error=0;

    strxmov(table_name, db, ".", table->table_name, NULL);
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

      open_and_lock_tables(session, table);
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
        end_trans(session, ROLLBACK);
        close_thread_tables(session);
        continue;
      case -1:           // error, message could be written to net
        /* purecov: begin inspected */
        goto err;
        /* purecov: end */
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
      goto send_result;
    }

    if ((table->table->db_stat & HA_READ_ONLY) && open_for_modify)
    {
      /* purecov: begin inspected */
      char buff[FN_REFLEN + DRIZZLE_ERRMSG_SIZE];
      uint32_t length;
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length= snprintf(buff, sizeof(buff), ER(ER_OPEN_AS_READONLY),
                       table_name);
      protocol->store(buff, length, system_charset_info);
      ha_autocommit_or_rollback(session, 0);
      end_trans(session, COMMIT);
      close_thread_tables(session);
      lex->reset_query_tables_list(false);
      table->table=0;				// For query cache
      if (protocol->write())
	goto err;
      continue;
      /* purecov: end */
    }

    /* Close all instances of the table to allow repair to rename files */
    if (lock_type == TL_WRITE && table->table->s->version)
    {
      pthread_mutex_lock(&LOCK_open);
      const char *old_message=session->enter_cond(&COND_refresh, &LOCK_open,
					      "Waiting to get writelock");
      mysql_lock_abort(session,table->table, true);
      remove_table_from_cache(session, table->table->s->db.str,
                              table->table->s->table_name.str,
                              RTFC_WAIT_OTHER_THREAD_FLAG |
                              RTFC_CHECK_KILLED_FLAG);
      session->exit_cond(old_message);
      if (session->killed)
	goto err;
      open_for_modify= 0;
    }

    if (table->table->s->crashed && operator_func == &handler::ha_check)
    {
      /* purecov: begin inspected */
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("warning"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Table is marked as crashed"),
                      system_charset_info);
      if (protocol->write())
        goto err;
      /* purecov: end */
    }

    if (operator_func == &handler::ha_repair &&
        !(check_opt->sql_flags & TT_USEFRM))
    {
      if ((table->table->file->check_old_types() == HA_ADMIN_NEEDS_ALTER) ||
          (table->table->file->ha_check_for_upgrade(check_opt) ==
           HA_ADMIN_NEEDS_ALTER))
      {
        ha_autocommit_or_rollback(session, 1);
        close_thread_tables(session);
        tmp_disable_binlog(session); // binlogging is done by caller if wanted
        result_code= mysql_recreate_table(session, table);
        reenable_binlog(session);
        /*
          mysql_recreate_table() can push OK or ERROR.
          Clear 'OK' status. If there is an error, keep it:
          we will store the error message in a result set row
          and then clear.
        */
        if (session->main_da.is_ok())
          session->main_da.reset_diagnostics_area();
        goto send_result;
      }
    }

    result_code = (table->table->file->*operator_func)(session, check_opt);

send_result:

    lex->cleanup_after_one_table_open();
    session->clear_error();  // these errors shouldn't get client
    {
      List_iterator_fast<DRIZZLE_ERROR> it(session->warn_list);
      DRIZZLE_ERROR *err;
      while ((err= it++))
      {
        protocol->prepare_for_resend();
        protocol->store(table_name, system_charset_info);
        protocol->store((char*) operator_name, system_charset_info);
        protocol->store(warning_level_names[err->level].str,
                        warning_level_names[err->level].length,
                        system_charset_info);
        protocol->store(err->msg, system_charset_info);
        if (protocol->write())
          goto err;
      }
      drizzle_reset_errors(session, true);
    }
    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);
    protocol->store(operator_name, system_charset_info);

send_result_message:

    switch (result_code) {
    case HA_ADMIN_NOT_IMPLEMENTED:
      {
	char buf[ERRMSGSIZE+20];
	uint32_t length=snprintf(buf, ERRMSGSIZE,
                             ER(ER_CHECK_NOT_IMPLEMENTED), operator_name);
	protocol->store(STRING_WITH_LEN("note"), system_charset_info);
	protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_NOT_BASE_TABLE:
      {
        char buf[ERRMSGSIZE+20];
        uint32_t length= snprintf(buf, ERRMSGSIZE,
                              ER(ER_BAD_TABLE_ERROR), table_name);
        protocol->store(STRING_WITH_LEN("note"), system_charset_info);
        protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_OK:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("OK"), system_charset_info);
      break;

    case HA_ADMIN_FAILED:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Operation failed"),
                      system_charset_info);
      break;

    case HA_ADMIN_REJECT:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Operation need committed state"),
                      system_charset_info);
      open_for_modify= false;
      break;

    case HA_ADMIN_ALREADY_DONE:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Table is already up to date"),
                      system_charset_info);
      break;

    case HA_ADMIN_CORRUPT:
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Corrupt"), system_charset_info);
      fatal_error=1;
      break;

    case HA_ADMIN_INVALID:
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Invalid argument"),
                      system_charset_info);
      break;

    case HA_ADMIN_TRY_ALTER:
    {
      /*
        This is currently used only by InnoDB. ha_innobase::optimize() answers
        "try with alter", so here we close the table, do an ALTER Table,
        reopen the table and do ha_innobase::analyze() on it.
      */
      ha_autocommit_or_rollback(session, 0);
      close_thread_tables(session);
      TableList *save_next_local= table->next_local,
                 *save_next_global= table->next_global;
      table->next_local= table->next_global= 0;
      tmp_disable_binlog(session); // binlogging is done by caller if wanted
      result_code= mysql_recreate_table(session, table);
      reenable_binlog(session);
      /*
        mysql_recreate_table() can push OK or ERROR.
        Clear 'OK' status. If there is an error, keep it:
        we will store the error message in a result set row
        and then clear.
      */
      if (session->main_da.is_ok())
        session->main_da.reset_diagnostics_area();
      ha_autocommit_or_rollback(session, 0);
      close_thread_tables(session);
      if (!result_code) // recreation went ok
      {
        if ((table->table= open_ltable(session, table, lock_type, 0)) &&
            ((result_code= table->table->file->ha_analyze(session, check_opt)) > 0))
          result_code= 0; // analyze went ok
      }
      if (result_code) // either mysql_recreate_table or analyze failed
      {
        assert(session->is_error());
        if (session->is_error())
        {
          const char *err_msg= session->main_da.message();
          if (!session->vio_ok())
          {
            sql_print_error("%s",err_msg);
          }
          else
          {
            /* Hijack the row already in-progress. */
            protocol->store(STRING_WITH_LEN("error"), system_charset_info);
            protocol->store(err_msg, system_charset_info);
            (void)protocol->write();
            /* Start off another row for HA_ADMIN_FAILED */
            protocol->prepare_for_resend();
            protocol->store(table_name, system_charset_info);
            protocol->store(operator_name, system_charset_info);
          }
          session->clear_error();
        }
      }
      result_code= result_code ? HA_ADMIN_FAILED : HA_ADMIN_OK;
      table->next_local= save_next_local;
      table->next_global= save_next_global;
      goto send_result_message;
    }
    case HA_ADMIN_WRONG_CHECKSUM:
    {
      protocol->store(STRING_WITH_LEN("note"), system_charset_info);
      protocol->store(ER(ER_VIEW_CHECKSUM), strlen(ER(ER_VIEW_CHECKSUM)),
                      system_charset_info);
      break;
    }

    case HA_ADMIN_NEEDS_UPGRADE:
    case HA_ADMIN_NEEDS_ALTER:
    {
      char buf[ERRMSGSIZE];
      uint32_t length;

      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length=snprintf(buf, ERRMSGSIZE, ER(ER_TABLE_NEEDS_UPGRADE), table->table_name);
      protocol->store(buf, length, system_charset_info);
      fatal_error=1;
      break;
    }

    default:				// Probably HA_ADMIN_INTERNAL_ERROR
      {
        char buf[ERRMSGSIZE+20];
        uint32_t length=snprintf(buf, ERRMSGSIZE,
                             _("Unknown - internal error %d during operation"),
                             result_code);
        protocol->store(STRING_WITH_LEN("error"), system_charset_info);
        protocol->store(buf, length, system_charset_info);
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
          table->table->file->info(HA_STATUS_CONST);
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
    end_trans(session, COMMIT);
    close_thread_tables(session);
    table->table=0;				// For query cache
    if (protocol->write())
      goto err;
  }

  my_eof(session);
  return(false);

err:
  ha_autocommit_or_rollback(session, 1);
  end_trans(session, ROLLBACK);
  close_thread_tables(session);			// Shouldn't be needed
  if (table)
    table->table=0;
  return(true);
}


bool mysql_repair_table(Session* session, TableList* tables, HA_CHECK_OPT* check_opt)
{
  return(mysql_admin_table(session, tables, check_opt,
				"repair", TL_WRITE, 1,
                                test(check_opt->sql_flags & TT_USEFRM),
                                HA_OPEN_FOR_REPAIR,
				&prepare_for_repair,
				&handler::ha_repair));
}


bool mysql_optimize_table(Session* session, TableList* tables, HA_CHECK_OPT* check_opt)
{
  return(mysql_admin_table(session, tables, check_opt,
				"optimize", TL_WRITE, 1,0,0,0,
				&handler::ha_optimize));
}


/*
  Assigned specified indexes for a table into key cache

  SYNOPSIS
    mysql_assign_to_keycache()
    session		Thread object
    tables	Table list (one table only)

  RETURN VALUES
   false ok
   true  error
*/

bool mysql_assign_to_keycache(Session* session, TableList* tables,
			     LEX_STRING *key_cache_name)
{
  HA_CHECK_OPT check_opt;
  KEY_CACHE *key_cache;

  check_opt.init();
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!(key_cache= get_key_cache(key_cache_name)))
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    my_error(ER_UNKNOWN_KEY_CACHE, MYF(0), key_cache_name->str);
    return(true);
  }
  pthread_mutex_unlock(&LOCK_global_system_variables);
  check_opt.key_cache= key_cache;
  return(mysql_admin_table(session, tables, &check_opt,
				"assign_to_keycache", TL_READ_NO_INSERT, 0, 0,
				0, 0, &handler::assign_to_keycache));
}


/*
  Reassign all tables assigned to a key cache to another key cache

  SYNOPSIS
    reassign_keycache_tables()
    session		Thread object
    src_cache	Reference to the key cache to clean up
    dest_cache	New key cache

  NOTES
    This is called when one sets a key cache size to zero, in which
    case we have to move the tables associated to this key cache to
    the "default" one.

    One has to ensure that one never calls this function while
    some other thread is changing the key cache. This is assured by
    the caller setting src_cache->in_init before calling this function.

    We don't delete the old key cache as there may still be pointers pointing
    to it for a while after this function returns.

 RETURN VALUES
    0	  ok
*/

int reassign_keycache_tables(Session *session __attribute__((unused)),
                             KEY_CACHE *src_cache,
                             KEY_CACHE *dst_cache)
{
  assert(src_cache != dst_cache);
  assert(src_cache->in_init);
  src_cache->param_buff_size= 0;		// Free key cache
  ha_resize_key_cache(src_cache);
  ha_change_key_cache(src_cache, dst_cache);
  return(0);
}

/**
  @brief          Create frm file based on I_S table

  @param[in]      session                      thread handler
  @param[in]      schema_table             I_S table
  @param[in]      dst_path                 path where frm should be created
  @param[in]      create_info              Create info

  @return         Operation status
    @retval       0                        success
    @retval       1                        error
*/
bool mysql_create_like_schema_frm(Session* session, TableList* schema_table,
                                  char *dst_path, HA_CREATE_INFO *create_info)
{
  HA_CREATE_INFO local_create_info;
  Alter_info alter_info;
  bool tmp_table= (create_info->options & HA_LEX_CREATE_TMP_TABLE);
  uint32_t keys= schema_table->table->s->keys;
  uint32_t db_options= 0;

  memset(&local_create_info, 0, sizeof(local_create_info));
  local_create_info.db_type= schema_table->table->s->db_type();
  local_create_info.row_type= schema_table->table->s->row_type;
  local_create_info.default_table_charset=default_charset_info;
  alter_info.flags= (ALTER_CHANGE_COLUMN | ALTER_RECREATE);
  schema_table->table->use_all_columns();
  if (mysql_prepare_alter_table(session, schema_table->table,
                                &local_create_info, &alter_info))
    return(1);
  if (mysql_prepare_create_table(session, &local_create_info, &alter_info,
                                 tmp_table, &db_options,
                                 schema_table->table->file,
                                 &schema_table->table->s->key_info, &keys, 0))
    return(1);
  local_create_info.max_rows= 0;
  if (mysql_create_frm(session, dst_path, NULL, NULL,
                       &local_create_info, alter_info.create_list,
                       keys, schema_table->table->s->key_info,
                       schema_table->table->file))
    return(1);
  return(0);
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
                             HA_CREATE_INFO *create_info)
{
  Table *name_lock= 0;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN];
  uint32_t dst_path_length;
  char *db= table->db;
  char *table_name= table->table_name;
  int  err;
  bool res= true;
  uint32_t not_used;

  /*
    By opening source table we guarantee that it exists and no concurrent
    DDL operation will mess with it. Later we also take an exclusive
    name-lock on target table name, which makes copying of .frm file,
    call to ha_create_table() and binlogging atomic against concurrent DML
    and DDL operations on target table. Thus by holding both these "locks"
    we ensure that our statement is properly isolated from all concurrent
    operations which matter.
  */
  if (open_tables(session, &src_table, &not_used, 0))
    return(true);

  strxmov(src_path, src_table->table->s->path.str, reg_ext, NULL);

  /*
    Check that destination tables does not exist. Note that its name
    was already checked when it was added to the table list.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (find_temporary_table(session, db, table_name))
      goto table_exists;
    dst_path_length= build_tmptable_filename(session, dst_path, sizeof(dst_path));
    create_info->table_options|= HA_CREATE_DELAY_KEY_WRITE;
  }
  else
  {
    if (lock_table_name_if_not_cached(session, db, table_name, &name_lock))
      goto err;
    if (!name_lock)
      goto table_exists;
    dst_path_length= build_table_filename(dst_path, sizeof(dst_path),
                                          db, table_name, reg_ext, 0);
    if (!access(dst_path, F_OK))
      goto table_exists;
  }

  /*
    Create a new table by copying from source table

    Altough exclusive name-lock on target table protects us from concurrent
    DML and DDL operations on it we still want to wrap .FRM creation and call
    to ha_create_table() in critical section protected by LOCK_open in order
    to provide minimal atomicity against operations which disregard name-locks,
    like I_S implementation, for example. This is a temporary and should not
    be copied. Instead we should fix our code to always honor name-locks.

    Also some engines (e.g. NDB cluster) require that LOCK_open should be held
    during the call to ha_create_table(). See bug #28614 for more info.
  */
  pthread_mutex_lock(&LOCK_open);
  if (src_table->schema_table)
  {
    if (mysql_create_like_schema_frm(session, src_table, dst_path, create_info))
    {
      pthread_mutex_unlock(&LOCK_open);
      goto err;
    }
  }
  else if (my_copy(src_path, dst_path, MYF(MY_DONT_OVERWRITE_FILE)))
  {
    if (my_errno == ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_FILE,MYF(0),dst_path,my_errno);
    pthread_mutex_unlock(&LOCK_open);
    goto err;
  }

  /*
    As mysql_truncate don't work on a new table at this stage of
    creation, instead create the table directly (for both normal
    and temporary tables).
  */
  dst_path[dst_path_length - reg_ext_length]= '\0';  // Remove .frm
  if (session->variables.keep_files_on_create)
    create_info->options|= HA_CREATE_KEEP_FILES;
  err= ha_create_table(session, dst_path, db, table_name, create_info, 1);
  pthread_mutex_unlock(&LOCK_open);

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (err || !open_temporary_table(session, dst_path, db, table_name, 1,
                                     OTM_OPEN))
    {
      (void) rm_temporary_table(create_info->db_type,
				dst_path);
      goto err;     /* purecov: inspected */
    }
  }
  else if (err)
  {
    (void) quick_rm_table(create_info->db_type, db,
			  table_name, 0); /* purecov: inspected */
    goto err;	    /* purecov: inspected */
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
    if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
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
        pthread_mutex_lock(&LOCK_open);
        if (reopen_name_locked_table(session, table, false))
        {
          pthread_mutex_unlock(&LOCK_open);
          goto err;
        }
        pthread_mutex_unlock(&LOCK_open);

        int result= store_create_info(session, table, &query,
                                               create_info);

        assert(result == 0); // store_create_info() always return 0
        write_bin_log(session, true, query.ptr(), query.length());
      }
      else                                      // Case 1
        write_bin_log(session, true, session->query, session->query_length);
    }
    /*
      Case 3 and 4 does nothing under RBR
    */
  }

  res= false;
  goto err;

table_exists:
  if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
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
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(session, name_lock, false);
    pthread_mutex_unlock(&LOCK_open);
  }
  return(res);
}


bool mysql_analyze_table(Session* session, TableList* tables, HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  return(mysql_admin_table(session, tables, check_opt,
				"analyze", lock_type, 1, 0, 0, 0,
				&handler::ha_analyze));
}


bool mysql_check_table(Session* session, TableList* tables,HA_CHECK_OPT* check_opt)
{
  thr_lock_type lock_type = TL_READ_NO_INSERT;

  return(mysql_admin_table(session, tables, check_opt,
				"check", lock_type,
				0, 0, HA_OPEN_FOR_REPAIR, 0,
				&handler::ha_check));
}


/* table_list should contain just one table */
static int
mysql_discard_or_import_tablespace(Session *session,
                                   TableList *table_list,
                                   enum tablespace_op_type tablespace_op)
{
  Table *table;
  bool discard;
  int error;

  /*
    Note that DISCARD/IMPORT TABLESPACE always is the only operation in an
    ALTER Table
  */

  session->set_proc_info("discard_or_import_tablespace");

  discard= test(tablespace_op == DISCARD_TABLESPACE);

 /*
   We set this flag so that ha_innobase::open and ::external_lock() do
   not complain when we lock the table
 */
  session->tablespace_op= true;
  if (!(table=open_ltable(session, table_list, TL_WRITE, 0)))
  {
    session->tablespace_op=false;
    return(-1);
  }

  error= table->file->ha_discard_or_import_tablespace(discard);

  session->set_proc_info("end");

  if (error)
    goto err;

  /* The ALTER Table is always in its own transaction */
  error = ha_autocommit_or_rollback(session, 0);
  if (end_active_trans(session))
    error=1;
  if (error)
    goto err;
  write_bin_log(session, false, session->query, session->query_length);

err:
  ha_autocommit_or_rollback(session, error);
  session->tablespace_op=false;

  if (error == 0)
  {
    my_ok(session);
    return(0);
  }

  table->file->print_error(error, MYF(0));

  return(-1);
}

/**
  Copy all changes detected by parser to the HA_ALTER_FLAGS
*/

void setup_ha_alter_flags(Alter_info *alter_info, HA_ALTER_FLAGS *alter_flags)
{
  uint32_t flags= alter_info->flags;

  if (ALTER_ADD_COLUMN & flags)
    *alter_flags|= HA_ADD_COLUMN;
  if (ALTER_DROP_COLUMN & flags)
    *alter_flags|= HA_DROP_COLUMN;
  if (ALTER_RENAME & flags)
    *alter_flags|= HA_RENAME_TABLE;
  if (ALTER_CHANGE_COLUMN & flags)
    *alter_flags|= HA_CHANGE_COLUMN;
  if (ALTER_COLUMN_DEFAULT & flags)
    *alter_flags|= HA_COLUMN_DEFAULT_VALUE;
  if (ALTER_COLUMN_STORAGE & flags)
    *alter_flags|= HA_COLUMN_STORAGE;
  if (ALTER_COLUMN_FORMAT & flags)
    *alter_flags|= HA_COLUMN_FORMAT;
  if (ALTER_COLUMN_ORDER & flags)
    *alter_flags|= HA_ALTER_COLUMN_ORDER;
  if (ALTER_STORAGE & flags)
    *alter_flags|= HA_ALTER_STORAGE;
  if (ALTER_ROW_FORMAT & flags)
    *alter_flags|= HA_ALTER_ROW_FORMAT;
  if (ALTER_RECREATE & flags)
    *alter_flags|= HA_RECREATE;
  if (ALTER_FOREIGN_KEY & flags)
    *alter_flags|= HA_ALTER_FOREIGN_KEY;
}


/**
   @param       session                Thread
   @param       table              The original table.
   @param       alter_info         Alter options, fields and keys for the new
                                   table.
   @param       create_info        Create options for the new table.
   @param       order_num          Number of order list elements.
   @param[out]  ha_alter_flags  Flags that indicate what will be changed
   @param[out]  ha_alter_info      Data structures needed for on-line alter
   @param[out]  table_changes      Information about particular change

   First argument 'table' contains information of the original
   table, which includes all corresponding parts that the new
   table has in arguments create_list, key_list and create_info.

   By comparing the changes between the original and new table
   we can determine how much it has changed after ALTER Table
   and whether we need to make a copy of the table, or just change
   the .frm file.

   Mark any changes detected in the ha_alter_flags.

   If there are no data changes, but index changes, 'index_drop_buffer'
   and/or 'index_add_buffer' are populated with offsets into
   table->key_info or key_info_buffer respectively for the indexes
   that need to be dropped and/or (re-)created.

   @retval true  error
   @retval false success
*/

static
bool
compare_tables(Session *session,
               Table *table,
               Alter_info *alter_info,
                           HA_CREATE_INFO *create_info,
               uint32_t order_num,
               HA_ALTER_FLAGS *alter_flags,
               HA_ALTER_INFO *ha_alter_info,
               uint32_t *table_changes)
{
  Field **f_ptr, *field;
  uint32_t table_changes_local= 0;
  List_iterator_fast<Create_field> new_field_it(alter_info->create_list);
  Create_field *new_field;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;

  {
    /*
      Create a copy of alter_info.
      To compare the new and old table definitions, we need to "prepare"
      the new definition - transform it from parser output to a format
      that describes the final table layout (all column defaults are
      initialized, duplicate columns are removed). This is done by
      mysql_prepare_create_table.  Unfortunately,
      mysql_prepare_create_table performs its transformations
      "in-place", that is, modifies the argument.  Since we would
      like to keep compare_tables() idempotent (not altering any
      of the arguments) we create a copy of alter_info here and
      pass it to mysql_prepare_create_table, then use the result
      to evaluate possibility of fast ALTER Table, and then
      destroy the copy.
    */
    Alter_info tmp_alter_info(*alter_info, session->mem_root);
    Session *session= table->in_use;
    uint32_t db_options= 0; /* not used */
    /* Create the prepared information. */
    if (mysql_prepare_create_table(session, create_info,
                                   &tmp_alter_info,
                                   (table->s->tmp_table != NO_TMP_TABLE),
                                   &db_options,
                                   table->file,
                                   &ha_alter_info->key_info_buffer,
                                   &ha_alter_info->key_count,
                                   /* select_field_count */ 0))
      return(true);
    /* Allocate result buffers. */
    if (! (ha_alter_info->index_drop_buffer=
           (uint*) session->alloc(sizeof(uint) * table->s->keys)) ||
        ! (ha_alter_info->index_add_buffer=
           (uint*) session->alloc(sizeof(uint) *
                              tmp_alter_info.key_list.elements)))
      return(true);
  }
  /*
    First we setup ha_alter_flags based on what was detected
    by parser
  */
  setup_ha_alter_flags(alter_info, alter_flags);


  /*
    Some very basic checks. If number of fields changes, or the
    handler, we need to run full ALTER Table. In the future
    new fields can be added and old dropped without copy, but
    not yet.

    Test also that engine was not given during ALTER Table, or
    we are force to run regular alter table (copy).
    E.g. ALTER Table tbl_name ENGINE=MyISAM.

    For the following ones we also want to run regular alter table:
    ALTER Table tbl_name order_st BY ..
    ALTER Table tbl_name CONVERT TO CHARACTER SET ..

    At the moment we can't handle altering temporary tables without a copy.
    We also test if OPTIMIZE Table was given and was mapped to alter table.
    In that case we always do full copy.

    There was a bug prior to mysql-4.0.25. Number of null fields was
    calculated incorrectly. As a result frm and data files gets out of
    sync after fast alter table. There is no way to determine by which
    mysql version (in 4.0 and 4.1 branches) table was created, thus we
    disable fast alter table for all tables created by mysql versions
    prior to 5.0 branch.
    See BUG#6236.
  */
  if (table->s->fields != alter_info->create_list.elements ||
      table->s->db_type() != create_info->db_type ||
      table->s->tmp_table ||
      create_info->used_fields & HA_CREATE_USED_ENGINE ||
      create_info->used_fields & HA_CREATE_USED_CHARSET ||
      create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET ||
      create_info->used_fields & HA_CREATE_USED_ROW_FORMAT ||
      (alter_info->flags & (ALTER_RECREATE | ALTER_FOREIGN_KEY)) ||
      order_num ||
      !table->s->mysql_version)
  {
    *table_changes= IS_EQUAL_NO;
    /*
      Check what has changed and set alter_flags
    */
    if (table->s->fields < alter_info->create_list.elements)
      *alter_flags|= HA_ADD_COLUMN;
    else if (table->s->fields > alter_info->create_list.elements)
      *alter_flags|= HA_DROP_COLUMN;
    if (create_info->db_type != table->s->db_type() ||
        create_info->used_fields & HA_CREATE_USED_ENGINE)
      *alter_flags|= HA_ALTER_STORAGE_ENGINE;
    if (create_info->used_fields & HA_CREATE_USED_CHARSET)
      *alter_flags|= HA_CHANGE_CHARACTER_SET;
    if (create_info->used_fields & HA_CREATE_USED_DEFAULT_CHARSET)
      *alter_flags|= HA_SET_DEFAULT_CHARACTER_SET;
    if (alter_info->flags & ALTER_RECREATE)
      *alter_flags|= HA_RECREATE;
    /* TODO check for ADD/DROP FOREIGN KEY */
    if (alter_info->flags & ALTER_FOREIGN_KEY)
      *alter_flags|=  HA_ALTER_FOREIGN_KEY;
  }
  /*
    Go through fields and check if the original ones are compatible
    with new table.
  */
  for (f_ptr= table->field, new_field= new_field_it++;
       (new_field && (field= *f_ptr));
       f_ptr++, new_field= new_field_it++)
  {
    /* Make sure we have at least the default charset in use. */
    if (!new_field->charset)
      new_field->charset= create_info->default_table_charset;

    /* Don't pack rows in old tables if the user has requested this. */
    if (create_info->row_type == ROW_TYPE_DYNAMIC ||
        (new_field->flags & BLOB_FLAG) ||
        (new_field->sql_type == DRIZZLE_TYPE_VARCHAR && create_info->row_type != ROW_TYPE_FIXED))
      create_info->table_options|= HA_OPTION_PACK_RECORD;

    /* Check how fields have been modified */
    if (alter_info->flags & ALTER_CHANGE_COLUMN)
    {
      /* Evaluate changes bitmap and send to check_if_incompatible_data() */
      if (!(table_changes_local= field->is_equal(new_field)))
        *alter_flags|= HA_ALTER_COLUMN_TYPE;

      /*
        Check if the altered column is a stored virtual field.
        TODO: Mark such a column with an alter flag only if
        the expression functions are not equal.
      */
      if (field->is_stored && field->vcol_info)
        *alter_flags|= HA_ALTER_STORED_VCOL;

      /* Check if field was renamed */
      field->flags&= ~FIELD_IS_RENAMED;
      if (my_strcasecmp(system_charset_info,
                        field->field_name,
                        new_field->field_name))
      {
        field->flags|= FIELD_IS_RENAMED;
        *alter_flags|= HA_ALTER_COLUMN_NAME;
      }

      *table_changes&= table_changes_local;
      if (table_changes_local == IS_EQUAL_PACK_LENGTH)
        *alter_flags|= HA_ALTER_COLUMN_TYPE;

      /* Check that NULL behavior is same for old and new fields */
      if ((new_field->flags & NOT_NULL_FLAG) !=
          (uint) (field->flags & NOT_NULL_FLAG))
      {
        *table_changes= IS_EQUAL_NO;
        *alter_flags|= HA_ALTER_COLUMN_NULLABLE;
      }
    }

    /* Clear indexed marker */
    field->flags&= ~FIELD_IN_ADD_INDEX;
  }

  /*
    Go through keys and check if the original ones are compatible
    with new table.
  */
  KEY *table_key;
  KEY *table_key_end= table->key_info + table->s->keys;
  KEY *new_key;
  KEY *new_key_end=
       ha_alter_info->key_info_buffer + ha_alter_info->key_count;

  /*
    Step through all keys of the old table and search matching new keys.
  */
  ha_alter_info->index_drop_count= 0;
  ha_alter_info->index_add_count= 0;
  for (table_key= table->key_info; table_key < table_key_end; table_key++)
  {
    KEY_PART_INFO *table_part;
    KEY_PART_INFO *table_part_end= table_key->key_part + table_key->key_parts;
    KEY_PART_INFO *new_part;

    /* Search a new key with the same name. */
    for (new_key= ha_alter_info->key_info_buffer;
         new_key < new_key_end;
         new_key++)
    {
      if (! strcmp(table_key->name, new_key->name))
        break;
    }
    if (new_key >= new_key_end)
    {
      /* Key not found. Add the offset of the key to the drop buffer. */
      ha_alter_info->index_drop_buffer
           [ha_alter_info->index_drop_count++]=
           table_key - table->key_info;
      if (table_key->flags & HA_NOSAME)
      {
        /* Unique key. Check for "PRIMARY". */
        if (is_primary_key(table_key))
          *alter_flags|= HA_DROP_PK_INDEX;
        else
          *alter_flags|= HA_DROP_UNIQUE_INDEX;
      }
      else
        *alter_flags|= HA_DROP_INDEX;
      *table_changes= IS_EQUAL_NO;
      continue;
    }

    /* Check that the key types are compatible between old and new tables. */
    if ((table_key->algorithm != new_key->algorithm) ||
        ((table_key->flags & HA_KEYFLAG_MASK) !=
         (new_key->flags & HA_KEYFLAG_MASK)) ||
        (table_key->key_parts != new_key->key_parts))
    {
      if (table_key->flags & HA_NOSAME)
      {
        // Unique key. Check for "PRIMARY".
        if (is_primary_key(table_key))
          *alter_flags|= HA_ALTER_PK_INDEX;
        else
          *alter_flags|= HA_ALTER_UNIQUE_INDEX;
      }
      else
        *alter_flags|= HA_ALTER_INDEX;
      goto index_changed;
    }

    /*
      Check that the key parts remain compatible between the old and
      new tables.
    */
    for (table_part= table_key->key_part, new_part= new_key->key_part;
         table_part < table_part_end;
         table_part++, new_part++)
    {
      /*
        Key definition has changed if we are using a different field or
	if the used key part length is different. We know that the fields
        did not change. Comparing field numbers is sufficient.
      */
      if ((table_part->length != new_part->length) ||
          (table_part->fieldnr - 1 != new_part->fieldnr))
      {
        if (table_key->flags & HA_NOSAME)
        {
          /* Unique key. Check for "PRIMARY" */
          if (is_primary_key(table_key))
            *alter_flags|= HA_ALTER_PK_INDEX;
          else
            *alter_flags|= HA_ALTER_UNIQUE_INDEX;
        }
        else
          *alter_flags|= HA_ALTER_INDEX;
        goto index_changed;
      }
    }
    continue;

  index_changed:
    /* Key modified. Add the offset of the key to both buffers. */
    ha_alter_info->index_drop_buffer
         [ha_alter_info->index_drop_count++]=
         table_key - table->key_info;
    ha_alter_info->index_add_buffer
         [ha_alter_info->index_add_count++]=
         new_key - ha_alter_info->key_info_buffer;
    key_part= new_key->key_part;
    end= key_part + new_key->key_parts;
    for(; key_part != end; key_part++)
    {
      /* Mark field to be part of new key */
      if ((field= table->field[key_part->fieldnr]))
        field->flags|= FIELD_IN_ADD_INDEX;
    }
    *table_changes= IS_EQUAL_NO;
  }
  /*end of for (; table_key < table_key_end;) */

  /*
    Step through all keys of the new table and find matching old keys.
  */
  for (new_key= ha_alter_info->key_info_buffer;
       new_key < new_key_end;
       new_key++)
  {
    /* Search an old key with the same name. */
    for (table_key= table->key_info; table_key < table_key_end; table_key++)
    {
      if (! strcmp(table_key->name, new_key->name))
        break;
    }
    if (table_key >= table_key_end)
    {
      /* Key not found. Add the offset of the key to the add buffer. */
      ha_alter_info->index_add_buffer
           [ha_alter_info->index_add_count++]=
           new_key - ha_alter_info->key_info_buffer;
      key_part= new_key->key_part;
      end= key_part + new_key->key_parts;
      for(; key_part != end; key_part++)
      {
        /* Mark field to be part of new key */
        if ((field= table->field[key_part->fieldnr]))
          field->flags|= FIELD_IN_ADD_INDEX;
      }
      if (new_key->flags & HA_NOSAME)
      {
        /* Unique key. Check for "PRIMARY" */
        if (is_primary_key(new_key))
          *alter_flags|= HA_ADD_PK_INDEX;
        else
        *alter_flags|= HA_ADD_UNIQUE_INDEX;
      }
      else
        *alter_flags|= HA_ADD_INDEX;
      *table_changes= IS_EQUAL_NO;
    }
  }

  return(false);
}


/*
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

static
bool alter_table_manage_keys(Table *table, int indexes_were_disabled,
                             enum enum_enable_or_disable keys_onoff)
{
  int error= 0;
  switch (keys_onoff) {
  case ENABLE:
    error= table->file->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
    break;
  case LEAVE_AS_IS:
    if (!indexes_were_disabled)
      break;
    /* fall-through: disabled indexes */
  case DISABLE:
    error= table->file->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
  }

  if (error == HA_ERR_WRONG_COMMAND)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                        ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                        table->s->table_name.str);
    error= 0;
  } else if (error)
    table->file->print_error(error, MYF(0));

  return(error);
}

int create_temporary_table(Session *session,
                           Table *table,
                           char *new_db,
                           char *tmp_name,
                           HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool db_changed)
{
  int error;
  char index_file[FN_REFLEN], data_file[FN_REFLEN];
  handlerton *old_db_type, *new_db_type;
  old_db_type= table->s->db_type();
  new_db_type= create_info->db_type;
  /*
    Handling of symlinked tables:
    If no rename:
      Create new data file and index file on the same disk as the
      old data and index files.
      Copy data.
      Rename new data file over old data file and new index file over
      old index file.
      Symlinks are not changed.

   If rename:
      Create new data file and index file on the same disk as the
      old data and index files.  Create also symlinks to point at
      the new tables.
      Copy data.
      At end, rename intermediate tables, and symlinks to intermediate
      table, to final table name.
      Remove old table and old symlinks

    If rename is made to another database:
      Create new tables in new database.
      Copy data.
      Remove old table and symlinks.
  */
  if (db_changed)		// Ignore symlink if db changed
  {
    if (create_info->index_file_name)
    {
      /* Fix index_file_name to have 'tmp_name' as basename */
      strcpy(index_file, tmp_name);
      create_info->index_file_name=fn_same(index_file,
                                           create_info->index_file_name,
                                           1);
    }
    if (create_info->data_file_name)
    {
      /* Fix data_file_name to have 'tmp_name' as basename */
      strcpy(data_file, tmp_name);
      create_info->data_file_name=fn_same(data_file,
                                          create_info->data_file_name,
                                          1);
    }
  }
  else
    create_info->data_file_name=create_info->index_file_name=0;

  /*
    Create a table with a temporary name.
    We don't log the statement, it will be logged later.
  */
  tmp_disable_binlog(session);
  error= mysql_create_table(session, new_db, tmp_name,
                            create_info, alter_info, 1, 0);
  reenable_binlog(session);

  return(error);
}

/*
  Create a temporary table that reflects what an alter table operation
  will accomplish.

  SYNOPSIS
    create_altered_table()
      session              Thread handle
      table            The original table
      create_info      Information from the parsing phase about new
                       table properties.
      alter_info       Lists of fields, keys to be changed, added
                       or dropped.
      db_change        Specifies if the table is moved to another database
  RETURN
    A temporary table with all changes
    NULL if error
  NOTES
    The temporary table is created without storing it in any storage engine
    and is opened only to get the table struct and frm file reference.
*/
Table *create_altered_table(Session *session,
                            Table *table,
                            char *new_db,
                            HA_CREATE_INFO *create_info,
                            Alter_info *alter_info,
                            bool db_change)
{
  int error;
  HA_CREATE_INFO altered_create_info(*create_info);
  Table *altered_table;
  char tmp_name[80];
  char path[FN_REFLEN];

  snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%"PRIx64,
           TMP_FILE_PREFIX, (unsigned long)current_pid, session->thread_id);
  /* Safety fix for InnoDB */
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tmp_name);
  altered_create_info.options&= ~HA_LEX_CREATE_TMP_TABLE;

  if ((error= create_temporary_table(session, table, new_db, tmp_name,
                                     &altered_create_info,
                                     alter_info, db_change)))
  {
    return(NULL);
  };

  build_table_filename(path, sizeof(path), new_db, tmp_name, "",
                       FN_IS_TMP);
  altered_table= open_temporary_table(session, path, new_db, tmp_name, 1,
                                      OTM_ALTER);
  return(altered_table);

  return(NULL);
}


/*
  Perform a fast or on-line alter table

  SYNOPSIS
    mysql_fast_or_online_alter_table()
      session              Thread handle
      table            The original table
      altered_table    A temporary table showing how we will change table
      create_info      Information from the parsing phase about new
                       table properties.
      alter_info       Storage place for data used during different phases
      ha_alter_flags   Bitmask that shows what will be changed
      keys_onoff       Specifies if keys are to be enabled/disabled
  RETURN
     0  OK
    >0  An error occured during the on-line alter table operation
    -1  Error when re-opening table
  NOTES
    If mysql_alter_table does not need to copy the table, it is
    either a fast alter table where the storage engine does not
    need to know about the change, only the frm will change,
    or the storage engine supports performing the alter table
    operation directly, on-line without mysql having to copy
    the table.
*/
int mysql_fast_or_online_alter_table(Session *session,
                                     Table *table,
                                     Table *altered_table,
                                     HA_CREATE_INFO *create_info,
                                     HA_ALTER_INFO *alter_info,
                                     HA_ALTER_FLAGS *ha_alter_flags,
                                     enum enum_enable_or_disable keys_onoff)
{
  int error= 0;
  bool online= (table->file->ha_table_flags() & HA_ONLINE_ALTER)?true:false;
  Table *t_table;

  if (online)
  {
   /*
      Tell the handler to prepare for the online alter
    */
    if ((error= table->file->alter_table_phase1(session,
                                                altered_table,
                                                create_info,
                                                alter_info,
                                                ha_alter_flags)))
    {
      goto err;
    }

    /*
       Tell the storage engine to perform the online alter table
       TODO:
       if check_if_supported_alter() returned HA_ALTER_SUPPORTED_WAIT_LOCK
       we need to wrap the next call with a DDL lock.
     */
    if ((error= table->file->alter_table_phase2(session,
                                                altered_table,
                                                create_info,
                                                alter_info,
                                                ha_alter_flags)))
    {
      goto err;
    }
  }
  /*
    The final .frm file is already created as a temporary file
    and will be renamed to the original table name.
  */
  pthread_mutex_lock(&LOCK_open);
  wait_while_table_is_used(session, table, HA_EXTRA_FORCE_REOPEN);
  alter_table_manage_keys(table, table->file->indexes_are_disabled(),
                          keys_onoff);
  close_data_files_and_morph_locks(session,
                                   table->pos_in_table_list->db,
                                   table->pos_in_table_list->table_name);
  if (mysql_rename_table(NULL,
			 altered_table->s->db.str,
                         altered_table->s->table_name.str,
			 table->s->db.str,
                         table->s->table_name.str, FN_FROM_IS_TMP))
  {
    error= 1;
    pthread_mutex_unlock(&LOCK_open);
    goto err;
  }
  broadcast_refresh();
  pthread_mutex_unlock(&LOCK_open);

  /*
    The ALTER Table is always in its own transaction.
    Commit must not be called while LOCK_open is locked. It could call
    wait_if_global_read_lock(), which could create a deadlock if called
    with LOCK_open.
  */
  error= ha_autocommit_or_rollback(session, 0);

  if (ha_commit(session))
    error=1;
  if (error)
    goto err;
  if (online)
  {
    pthread_mutex_lock(&LOCK_open);
    if (reopen_table(table))
    {
      error= -1;
      goto err;
    }
    pthread_mutex_unlock(&LOCK_open);
    t_table= table;

   /*
      Tell the handler that the changed frm is on disk and table
      has been re-opened
   */
    if ((error= t_table->file->alter_table_phase3(session, t_table)))
    {
      goto err;
    }

    /*
      We are going to reopen table down on the road, so we have to restore
      state of the Table object which we used for obtaining of handler
      object to make it suitable for reopening.
    */
    assert(t_table == table);
    table->open_placeholder= 1;
    pthread_mutex_lock(&LOCK_open);
    close_handle_and_leave_table_as_lock(table);
    pthread_mutex_unlock(&LOCK_open);
  }

 err:
  if (error)
    return(error);
  return 0;
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

static bool
mysql_prepare_alter_table(Session *session, Table *table,
                          HA_CREATE_INFO *create_info,
                          Alter_info *alter_info)
{
  /* New column definitions are added here */
  List<Create_field> new_create_list;
  /* New key definitions are added here */
  List<Key> new_key_list;
  List_iterator<Alter_drop> drop_it(alter_info->drop_list);
  List_iterator<Create_field> def_it(alter_info->create_list);
  List_iterator<Alter_column> alter_it(alter_info->alter_list);
  List_iterator<Key> key_it(alter_info->key_list);
  List_iterator<Create_field> find_it(new_create_list);
  List_iterator<Create_field> field_it(new_create_list);
  List<Key_part_spec> key_parts;
  uint32_t db_create_options= (table->s->db_create_options
                           & ~(HA_OPTION_PACK_RECORD));
  uint32_t used_fields= create_info->used_fields;
  KEY *key_info=table->key_info;
  bool rc= true;


  create_info->varchar= false;
  /* Let new create options override the old ones */
  if (!(used_fields & HA_CREATE_USED_MIN_ROWS))
    create_info->min_rows= table->s->min_rows;
  if (!(used_fields & HA_CREATE_USED_MAX_ROWS))
    create_info->max_rows= table->s->max_rows;
  if (!(used_fields & HA_CREATE_USED_AVG_ROW_LENGTH))
    create_info->avg_row_length= table->s->avg_row_length;
  if (!(used_fields & HA_CREATE_USED_BLOCK_SIZE))
    create_info->block_size= table->s->block_size;
  if (!(used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
    create_info->default_table_charset= table->s->table_charset;
  if (!(used_fields & HA_CREATE_USED_AUTO) && table->found_next_number_field)
    {
    /* Table has an autoincrement, copy value to new table */
    table->file->info(HA_STATUS_AUTO);
    create_info->auto_increment_value= table->file->stats.auto_increment_value;
  }
  if (!(used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE))
    create_info->key_block_size= table->s->key_block_size;

  restore_record(table, s->default_values);     // Empty record for DEFAULT
  Create_field *def;

    /*
    First collect all fields from table which isn't in drop_list
    */
  Field **f_ptr,*field;
  for (f_ptr=table->field ; (field= *f_ptr) ; f_ptr++)
    {
    /* Check if field should be dropped */
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::COLUMN &&
	  !my_strcasecmp(system_charset_info,field->field_name, drop->name))
    {
	/* Reset auto_increment value if it was dropped */
	if (MTYP_TYPENR(field->unireg_check) == Field::NEXT_NUMBER &&
	    !(used_fields & HA_CREATE_USED_AUTO))
      {
	  create_info->auto_increment_value=0;
	  create_info->used_fields|=HA_CREATE_USED_AUTO;
      }
	break;
    }
  }
    if (drop)
      {
      drop_it.remove();
      continue;
    }
    /* Check if field is changed */
    def_it.rewind();
    while ((def=def_it++))
    {
      if (def->change &&
	  !my_strcasecmp(system_charset_info,field->field_name, def->change))
	break;
    }
    if (def)
    {						// Field is changed
      def->field=field;
      if (field->is_stored != def->is_stored)
      {
        my_error(ER_UNSUPPORTED_ACTION_ON_VIRTUAL_COLUMN,
                 MYF(0),
                 "Changing the STORED status");
        goto err;
      }
      if (!def->after)
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
      def= new Create_field(field, field);
      new_create_list.push_back(def);
      alter_it.rewind();			// Change default if ALTER
      Alter_column *alter;
      while ((alter=alter_it++))
        {
	if (!my_strcasecmp(system_charset_info,field->field_name, alter->name))
	  break;
        }
      if (alter)
	{
	if (def->sql_type == DRIZZLE_TYPE_BLOB)
	{
	  my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0), def->change);
          goto err;
	}
	if ((def->def=alter->def))              // Use new default
          def->flags&= ~NO_DEFAULT_VALUE_FLAG;
        else
          def->flags|= NO_DEFAULT_VALUE_FLAG;
	alter_it.remove();
      }
    }
  }
  def_it.rewind();
  while ((def=def_it++))			// Add new columns
  {
    if (def->change && ! def->field)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), def->change, table->s->table_name.str);
      goto err;
    }
      /*
      Check that the DATE/DATETIME not null field we are going to add is
      either has a default value or the '0000-00-00' is allowed by the
      set sql mode.
      If the '0000-00-00' value isn't allowed then raise the error_if_not_empty
      flag to allow ALTER Table only if the table to be altered is empty.
      */
    if ((def->sql_type == DRIZZLE_TYPE_DATE ||
         def->sql_type == DRIZZLE_TYPE_DATETIME) &&
         !alter_info->datetime_field &&
         !(~def->flags & (NO_DEFAULT_VALUE_FLAG | NOT_NULL_FLAG)) &&
         session->variables.sql_mode & MODE_NO_ZERO_DATE)
    {
        alter_info->datetime_field= def;
        alter_info->error_if_not_empty= true;
    }
    if (!def->after)
      new_create_list.push_back(def);
    else if (def->after == first_keyword)
      new_create_list.push_front(def);
    else
    {
      Create_field *find;
      find_it.rewind();
      while ((find=find_it++))			// Add new columns
      {
	if (!my_strcasecmp(system_charset_info,def->after, find->field_name))
	  break;
  }
      if (!find)
  {
	my_error(ER_BAD_FIELD_ERROR, MYF(0), def->after, table->s->table_name.str);
    goto err;
  }
      find_it.after(def);			// Put element after this
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
      if (alter_info->build_method == HA_BUILD_ONLINE)
      {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0), session->query);
        goto err;
      }
      alter_info->build_method= HA_BUILD_OFFLINE;
    }
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0),
             alter_info->alter_list.head()->name, table->s->table_name.str);
    goto err;
    }
  if (!new_create_list.elements)
    {
    my_message(ER_CANT_REMOVE_ALL_FIELDS, ER(ER_CANT_REMOVE_ALL_FIELDS),
               MYF(0));
    goto err;
    }

    /*
    Collect all keys which isn't in drop list. Add only those
    for which some fields exists.
    */

  for (uint32_t i=0 ; i < table->s->keys ; i++,key_info++)
    {
    char *key_name= key_info->name;
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
      {
      if (drop->type == Alter_drop::KEY &&
	  !my_strcasecmp(system_charset_info,key_name, drop->name))
	break;
      }
    if (drop)
        {
      drop_it.remove();
      continue;
    }

    KEY_PART_INFO *key_part= key_info->key_part;
    key_parts.empty();
    for (uint32_t j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (!key_part->field)
	continue;				// Wrong field (from UNIREG)
      const char *key_part_name=key_part->field->field_name;
      Create_field *cfield;
      field_it.rewind();
      while ((cfield=field_it++))
    {
	if (cfield->change)
    {
	  if (!my_strcasecmp(system_charset_info, key_part_name,
			     cfield->change))
	    break;
	}
	else if (!my_strcasecmp(system_charset_info,
				key_part_name, cfield->field_name))
	  break;
      }
      if (!cfield)
	continue;				// Field is removed
      uint32_t key_part_length=key_part->length;
      if (cfield->field)			// Not new field
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
        if (!Field::type_can_have_key_part(cfield->field->type()) ||
            !Field::type_can_have_key_part(cfield->sql_type) ||
            (cfield->field->field_length == key_part_length &&
             !f_is_blob(key_part->key_type)) ||
	    (cfield->length && (cfield->length < key_part_length /
                                key_part->field->charset()->mbmaxlen)))
	  key_part_length= 0;			// Use whole field
      }
      key_part_length /= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(new Key_part_spec(cfield->field_name,
                                            strlen(cfield->field_name),
					    key_part_length));
    }
    if (key_parts.elements)
    {
      KEY_CREATE_INFO key_create_info;
      Key *key;
      enum Key::Keytype key_type;
      memset(&key_create_info, 0, sizeof(key_create_info));

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
        key_type= Key::MULTIPLE;

      key= new Key(key_type, key_name, strlen(key_name),
                   &key_create_info,
                   test(key_info->flags & HA_GENERATED_KEY),
                   key_parts);
      new_key_list.push_back(key);
    }
  }
  {
    Key *key;
    while ((key=key_it++))			// Add new keys
    {
      if (key->type == Key::FOREIGN_KEY &&
          ((Foreign_key *)key)->validate(new_create_list))
        goto err;
      if (key->type != Key::FOREIGN_KEY)
        new_key_list.push_back(key);
      if (key->name.str && is_primary_key_name(key->name.str))
      {
	my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name.str);
        goto err;
      }
    }
  }

  if (alter_info->drop_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
             alter_info->drop_list.head()->name);
    goto err;
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
             alter_info->alter_list.head()->name);
    goto err;
  }

  if (!create_info->comment.str)
  {
    create_info->comment.str= table->s->comment.str;
    create_info->comment.length= table->s->comment.length;
  }

  table->file->update_create_info(create_info);
  if ((create_info->table_options &
       (HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS)) ||
      (used_fields & HA_CREATE_USED_PACK_KEYS))
    db_create_options&= ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS);
  if (create_info->table_options &
      (HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM))
    db_create_options&= ~(HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM);
  if (create_info->table_options &
      (HA_OPTION_DELAY_KEY_WRITE | HA_OPTION_NO_DELAY_KEY_WRITE))
    db_create_options&= ~(HA_OPTION_DELAY_KEY_WRITE |
			  HA_OPTION_NO_DELAY_KEY_WRITE);
  create_info->table_options|= db_create_options;

  if (table->s->tmp_table)
    create_info->options|=HA_LEX_CREATE_TMP_TABLE;

  rc= false;
  alter_info->create_list.swap(new_create_list);
  alter_info->key_list.swap(new_key_list);
err:
  return(rc);
}


/*
  Alter table

  SYNOPSIS
    mysql_alter_table()
      session              Thread handle
      new_db           If there is a RENAME clause
      new_name         If there is a RENAME clause
      create_info      Information from the parsing phase about new
                       table properties.
      table_list       The table to change.
      alter_info       Lists of fields, keys to be changed, added
                       or dropped.
      order_num        How many order_st BY fields has been specified.
      order            List of fields to order_st BY.
      ignore           Whether we have ALTER IGNORE Table

  DESCRIPTION
    This is a veery long function and is everything but the kitchen sink :)
    It is used to alter a table and not only by ALTER Table but also
    CREATE|DROP INDEX are mapped on this function.

    When the ALTER Table statement just does a RENAME or ENABLE|DISABLE KEYS,
    or both, then this function short cuts its operation by renaming
    the table and/or enabling/disabling the keys. In this case, the FRM is
    not changed, directly by mysql_alter_table. However, if there is a
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

bool mysql_alter_table(Session *session,char *new_db, char *new_name,
                       HA_CREATE_INFO *create_info,
                       TableList *table_list,
                       Alter_info *alter_info,
                       uint32_t order_num, order_st *order, bool ignore)
{
  Table *table, *new_table=0, *name_lock= 0;;
  int error= 0;
  char tmp_name[80],old_name[32],new_name_buff[FN_REFLEN];
  char new_alias_buff[FN_REFLEN], *table_name, *db, *new_alias, *alias;
  char path[FN_REFLEN];
  ha_rows copied= 0,deleted= 0;
  handlerton *old_db_type, *new_db_type, *save_old_db_type;

  new_name_buff[0]= '\0';

  if (table_list && table_list->schema_table)
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), "", "", INFORMATION_SCHEMA_NAME.c_str());
    return(true);
  }

  /*
    Assign variables table_name, new_name, db, new_db, path
    to simplify further comparisons: we want to see if it's a RENAME
    later just by comparing the pointers, avoiding the need for strcmp.
  */
  session->set_proc_info("init");
  table_name=table_list->table_name;
  alias= (lower_case_table_names == 2) ? table_list->alias : table_name;
  db=table_list->db;
  if (!new_db || !my_strcasecmp(table_alias_charset, new_db, db))
    new_db= db;
  build_table_filename(path, sizeof(path), db, table_name, "", 0);

  mysql_ha_rm_tables(session, table_list, false);

  /* DISCARD/IMPORT TABLESPACE is always alone in an ALTER Table */
  if (alter_info->tablespace_op != NO_TABLESPACE_OP)
    /* Conditionally writes to binlog. */
    return(mysql_discard_or_import_tablespace(session,table_list,
                                              alter_info->tablespace_op));
  char* pos= new_name_buff;
  char* pos_end= pos+strlen(new_name_buff)-1;
  pos= my_stpncpy(new_name_buff, drizzle_data_home, pos_end-pos);
  pos= my_stpncpy(new_name_buff, "/", pos_end-pos);
  pos= my_stpncpy(new_name_buff, db, pos_end-pos);
  pos= my_stpncpy(new_name_buff, "/", pos_end-pos);
  pos= my_stpncpy(new_name_buff, table_name, pos_end-pos);
  pos= my_stpncpy(new_name_buff, reg_ext, pos_end-pos);

  (void) unpack_filename(new_name_buff, new_name_buff);
  /*
    If this is just a rename of a view, short cut to the
    following scenario: 1) lock LOCK_open 2) do a RENAME
    2) unlock LOCK_open.
    This is a copy-paste added to make sure
    ALTER (sic:) Table .. RENAME works for views. ALTER VIEW is handled
    as an independent branch in mysql_execute_command. The need
    for a copy-paste arose because the main code flow of ALTER Table
    ... RENAME tries to use open_ltable, which does not work for views
    (open_ltable was never modified to merge table lists of child tables
    into the main table list, like open_tables does).
    This code is wrong and will be removed, please do not copy.
  */

  if (!(table= open_n_lock_single_table(session, table_list, TL_WRITE_ALLOW_READ)))
    return(true);
  table->use_all_columns();

  /* Check that we are not trying to rename to an existing table */
  if (new_name)
  {
    strcpy(new_name_buff,new_name);
    strcpy(new_alias= new_alias_buff, new_name);
    if (lower_case_table_names)
    {
      if (lower_case_table_names != 2)
      {
        my_casedn_str(files_charset_info, new_name_buff);
        new_alias= new_name;			// Create lower case table name
      }
      my_casedn_str(files_charset_info, new_name);
    }
    if (new_db == db &&
	!my_strcasecmp(table_alias_charset, new_name_buff, table_name))
    {
      /*
	Source and destination table names are equal: make later check
	easier.
      */
      new_alias= new_name= table_name;
    }
    else
    {
      if (table->s->tmp_table != NO_TMP_TABLE)
      {
	if (find_temporary_table(session,new_db,new_name_buff))
	{
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name_buff);
	  return(true);
	}
      }
      else
      {
        if (lock_table_name_if_not_cached(session, new_db, new_name, &name_lock))
          return(true);
        if (!name_lock)
        {
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
	  return(true);
        }

        build_table_filename(new_name_buff, sizeof(new_name_buff),
                             new_db, new_name_buff, reg_ext, 0);
        if (!access(new_name_buff, F_OK))
	{
	  /* Table will be closed in do_command() */
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
	  goto err;
	}
      }
    }
  }
  else
  {
    new_alias= (lower_case_table_names == 2) ? alias : table_name;
    new_name= table_name;
  }

  old_db_type= table->s->db_type();
  if (!create_info->db_type)
  {
    create_info->db_type= old_db_type;
  }

  if (check_engine(session, new_name, create_info))
    goto err;
  new_db_type= create_info->db_type;

  if (new_db_type != old_db_type &&
      !table->file->can_switch_engines())
  {
    assert(0);
    my_error(ER_ROW_IS_REFERENCED, MYF(0));
    goto err;
  }

  if (create_info->row_type == ROW_TYPE_NOT_USED)
    create_info->row_type= table->s->row_type;

  if (ha_check_storage_engine_flag(old_db_type, HTON_BIT_ALTER_NOT_SUPPORTED) ||
      ha_check_storage_engine_flag(new_db_type, HTON_BIT_ALTER_NOT_SUPPORTED))
  {
    my_error(ER_ILLEGAL_HA, MYF(0), table_name);
    goto err;
  }

  session->set_proc_info("setup");
  if (!(alter_info->flags & ~(ALTER_RENAME | ALTER_KEYS_ONOFF)) &&
      !table->s->tmp_table) // no need to touch frm
  {
    switch (alter_info->keys_onoff) {
    case LEAVE_AS_IS:
      break;
    case ENABLE:
      /*
        wait_while_table_is_used() ensures that table being altered is
        opened only by this thread and that Table::TABLE_SHARE::version
        of Table object corresponding to this table is 0.
        The latter guarantees that no DML statement will open this table
        until ALTER Table finishes (i.e. until close_thread_tables())
        while the fact that the table is still open gives us protection
        from concurrent DDL statements.
      */
      pthread_mutex_lock(&LOCK_open);
      wait_while_table_is_used(session, table, HA_EXTRA_FORCE_REOPEN);
      pthread_mutex_unlock(&LOCK_open);
      error= table->file->ha_enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
      /* COND_refresh will be signaled in close_thread_tables() */
      break;
    case DISABLE:
      pthread_mutex_lock(&LOCK_open);
      wait_while_table_is_used(session, table, HA_EXTRA_FORCE_REOPEN);
      pthread_mutex_unlock(&LOCK_open);
      error=table->file->ha_disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
      /* COND_refresh will be signaled in close_thread_tables() */
      break;
    default:
      assert(false);
      error= 0;
      break;
    }
    if (error == HA_ERR_WRONG_COMMAND)
    {
      error= 0;
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->alias);
    }

    pthread_mutex_lock(&LOCK_open);
    /*
      Unlike to the above case close_cached_table() below will remove ALL
      instances of Table from table cache (it will also remove table lock
      held by this thread). So to make actual table renaming and writing
      to binlog atomic we have to put them into the same critical section
      protected by LOCK_open mutex. This also removes gap for races between
      access() and mysql_rename_table() calls.
    */

    if (!error && (new_name != table_name || new_db != db))
    {
      session->set_proc_info("rename");
      /*
        Then do a 'simple' rename of the table. First we need to close all
        instances of 'source' table.
      */
      close_cached_table(session, table);
      /*
        Then, we want check once again that target table does not exist.
        Actually the order of these two steps does not matter since
        earlier we took name-lock on the target table, so we do them
        in this particular order only to be consistent with 5.0, in which
        we don't take this name-lock and where this order really matters.
        TODO: Investigate if we need this access() check at all.
      */
      if (!access(new_name_buff,F_OK))
      {
	my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name);
	error= -1;
      }
      else
      {
	*fn_ext(new_name)=0;
	if (mysql_rename_table(old_db_type,db,table_name,new_db,new_alias, 0))
	  error= -1;
        else if (0)
      {
          mysql_rename_table(old_db_type, new_db, new_alias, db,
                             table_name, 0);
          error= -1;
      }
    }
  }

    if (error == HA_ERR_WRONG_COMMAND)
  {
      error= 0;
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->alias);
  }

    if (!error)
    {
      write_bin_log(session, true, session->query, session->query_length);
      my_ok(session);
  }
    else if (error > 0)
  {
      table->file->print_error(error, MYF(0));
      error= -1;
    }
    if (name_lock)
      unlink_open_table(session, name_lock, false);
    pthread_mutex_unlock(&LOCK_open);
    table_list->table= NULL;                    // For query cache
    return(error);
  }

  /* We have to do full alter table. */

    /*
    If the old table had partitions and we are doing ALTER Table ...
    engine= <new_engine>, the new table must preserve the original
    partitioning. That means that the new engine is still the
    partitioning engine, not the engine specified in the parser.
    This is discovered  in prep_alter_part_table, which in such case
    updates create_info->db_type.
    Now we need to update the stack copy of create_info->db_type,
    as otherwise we won't be able to correctly move the files of the
    temporary table to the result table files.
  */
  new_db_type= create_info->db_type;

  if (mysql_prepare_alter_table(session, table, create_info, alter_info))
      goto err;

  set_table_default_charset(session, create_info, db);


  if (session->variables.old_alter_table
      || (table->s->db_type() != create_info->db_type)
     )
  {
    if (alter_info->build_method == HA_BUILD_ONLINE)
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), session->query);
      goto err;
    }
    alter_info->build_method= HA_BUILD_OFFLINE;
  }

  if (alter_info->build_method != HA_BUILD_OFFLINE)
  {
    Table *altered_table= 0;
    HA_ALTER_INFO ha_alter_info;
    HA_ALTER_FLAGS ha_alter_flags;
    uint32_t table_changes= IS_EQUAL_YES;
    bool need_copy_table= true;
    /* Check how much the tables differ. */
    if (compare_tables(session, table, alter_info,
                       create_info, order_num,
                       &ha_alter_flags,
                       &ha_alter_info,
                       &table_changes))
    {
      return(true);
    }

    /*
      Check if storage engine supports altering the table
      on-line.
    */


    /*
      If table is not renamed, changed database and
      some change was detected then check if engine
      can do the change on-line
    */
    if (new_name == table_name && new_db == db &&
        ha_alter_flags.is_set())
    {
      Alter_info tmp_alter_info(*alter_info, session->mem_root);

      /*
        If no table rename,
        check if table can be altered on-line
      */
      if (!(altered_table= create_altered_table(session,
                                                table,
                                                new_db,
                                                create_info,
                                                &tmp_alter_info,
                                                !strcmp(db, new_db))))
        goto err;

      switch (table->file->check_if_supported_alter(altered_table,
                                                    create_info,
                                                    &ha_alter_flags,
                                                    table_changes)) {
      case HA_ALTER_SUPPORTED_WAIT_LOCK:
      case HA_ALTER_SUPPORTED_NO_LOCK:
        /*
          @todo: Currently we always acquire an exclusive name
          lock on the table metadata when performing fast or online
          ALTER Table. In future we may consider this unnecessary,
          and narrow the scope of the exclusive name lock to only
          cover manipulation with .frms. Storage engine API
          call check_if_supported_alter has provision for this
          already now.
        */
        need_copy_table= false;
        break;
      case HA_ALTER_NOT_SUPPORTED:
        if (alter_info->build_method == HA_BUILD_ONLINE)
        {
          my_error(ER_NOT_SUPPORTED_YET, MYF(0), session->query);
          close_temporary_table(session, altered_table, 1, 1);
          goto err;
        }
        need_copy_table= true;
        break;
      case HA_ALTER_ERROR:
      default:
        close_temporary_table(session, altered_table, 1, 1);
        goto err;
      }

    }
    /* TODO need to check if changes can be handled as fast ALTER Table */
    if (!altered_table)
      need_copy_table= true;

    if (!need_copy_table)
    {
      error= mysql_fast_or_online_alter_table(session,
                                              table,
                                              altered_table,
                                              create_info,
                                              &ha_alter_info,
                                              &ha_alter_flags,
                                              alter_info->keys_onoff);
      if (session->lock)
      {
        mysql_unlock_tables(session, session->lock);
        session->lock=0;
      }
      close_temporary_table(session, altered_table, 1, 1);

      if (error)
      {
        switch (error) {
        case(-1):
          goto err_with_placeholders;
        default:
          goto err;
        }
      }
      else
      {
        pthread_mutex_lock(&LOCK_open);
        goto end_online;
      }
    }

    if (altered_table)
      close_temporary_table(session, altered_table, 1, 1);
  }

  snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%"PRIx64, TMP_FILE_PREFIX,
           (unsigned long)current_pid, session->thread_id);
  /* Safety fix for innodb */
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tmp_name);


  /* Create a temporary table with the new format */
  if ((error= create_temporary_table(session, table, new_db, tmp_name,
                                     create_info, alter_info,
                                     !strcmp(db, new_db))))
  {
    goto err;
  }

  /* Open the table so we need to copy the data to it. */
  if (table->s->tmp_table)
  {
    TableList tbl;
    memset(&tbl, 0, sizeof(tbl));
    tbl.db= new_db;
    tbl.table_name= tbl.alias= tmp_name;
    /* Table is in session->temporary_tables */
    new_table= open_table(session, &tbl, (bool*) 0, DRIZZLE_LOCK_IGNORE_FLUSH);
  }
  else
  {
    char path[FN_REFLEN];
    /* table is a normal table: Create temporary table in same directory */
    build_table_filename(path, sizeof(path), new_db, tmp_name, "",
                         FN_IS_TMP);
    /* Open our intermediate table */
    new_table=open_temporary_table(session, path, new_db, tmp_name, 0, OTM_OPEN);
  }
  if (!new_table)
    goto err1;

  /* Copy the data if necessary. */
  session->count_cuted_fields= CHECK_FIELD_WARN;	// calc cuted fields
  session->cuted_fields=0L;
  session->set_proc_info("copy to tmp table");
  copied=deleted=0;
  /*
    We do not copy data for MERGE tables. Only the children have data.
    MERGE tables have HA_NO_COPY_ON_ALTER set.
  */
  if (new_table && !(new_table->file->ha_table_flags() & HA_NO_COPY_ON_ALTER))
  {
    /* We don't want update TIMESTAMP fields during ALTER Table. */
    new_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    new_table->next_number_field=new_table->found_next_number_field;
    error= copy_data_between_tables(table, new_table,
                                    alter_info->create_list, ignore,
                                   order_num, order, &copied, &deleted,
                                    alter_info->keys_onoff,
                                    alter_info->error_if_not_empty);
  }
  else
  {
    pthread_mutex_lock(&LOCK_open);
    wait_while_table_is_used(session, table, HA_EXTRA_FORCE_REOPEN);
    pthread_mutex_unlock(&LOCK_open);
    alter_table_manage_keys(table, table->file->indexes_are_disabled(),
                            alter_info->keys_onoff);
    error= ha_autocommit_or_rollback(session, 0);
    if (end_active_trans(session))
      error= 1;
  }
  session->count_cuted_fields= CHECK_FIELD_IGNORE;

  if (table->s->tmp_table != NO_TMP_TABLE)
  {
    /* We changed a temporary table */
    if (error)
      goto err1;
    /* Close lock if this is a transactional table */
    if (session->lock)
    {
      mysql_unlock_tables(session, session->lock);
      session->lock=0;
    }
    /* Remove link to old table and rename the new one */
    close_temporary_table(session, table, 1, 1);
    /* Should pass the 'new_name' as we store table name in the cache */
    if (rename_temporary_table(session, new_table, new_db, new_name))
      goto err1;
    goto end_temporary;
  }

  if (new_table)
  {
    /*
      Close the intermediate table that will be the new table.
      Note that MERGE tables do not have their children attached here.
    */
    intern_close_table(new_table);
    free(new_table);
  }
  pthread_mutex_lock(&LOCK_open);
  if (error)
  {
    quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP);
    pthread_mutex_unlock(&LOCK_open);
    goto err;
  }

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
  snprintf(old_name, sizeof(old_name), "%s2-%lx-%"PRIx64, TMP_FILE_PREFIX,
           (unsigned long)current_pid, session->thread_id);
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, old_name);

  wait_while_table_is_used(session, table, HA_EXTRA_PREPARE_FOR_RENAME);
  close_data_files_and_morph_locks(session, db, table_name);

  error=0;
  save_old_db_type= old_db_type;

  /*
    This leads to the storage engine (SE) not being notified for renames in
    mysql_rename_table(), because we just juggle with the FRM and nothing
    more. If we have an intermediate table, then we notify the SE that
    it should become the actual table. Later, we will recycle the old table.
    However, in case of ALTER Table RENAME there might be no intermediate
    table. This is when the old and new tables are compatible, according to
    compare_table(). Then, we need one additional call to
    mysql_rename_table() with flag NO_FRM_RENAME, which does nothing else but
    actual rename in the SE and the FRM is not touched. Note that, if the
    table is renamed and the SE is also changed, then an intermediate table
    is created and the additional call will not take place.
  */
  if (mysql_rename_table(old_db_type, db, table_name, db, old_name,
                         FN_TO_IS_TMP))
  {
    error=1;
    quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP);
  }
  else if (mysql_rename_table(new_db_type, new_db, tmp_name, new_db,
                              new_alias, FN_FROM_IS_TMP) || ((new_name != table_name || new_db != db) && 0))
  {
    /* Try to get everything back. */
    error=1;
    quick_rm_table(new_db_type,new_db,new_alias, 0);
    quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP);
    mysql_rename_table(old_db_type, db, old_name, db, alias,
                       FN_FROM_IS_TMP);
  }

  if (error)
  {
    /* This shouldn't happen. But let us play it safe. */
    goto err_with_placeholders;
  }

  quick_rm_table(old_db_type, db, old_name, FN_IS_TMP);

end_online:
  if (session->locked_tables && new_name == table_name && new_db == db)
  {
    session->in_lock_tables= 1;
    error= reopen_tables(session, 1, 1);
    session->in_lock_tables= 0;
    if (error)
      goto err_with_placeholders;
  }
  pthread_mutex_unlock(&LOCK_open);

  session->set_proc_info("end");

  assert(!(drizzle_bin_log.is_open() &&
                (create_info->options & HA_LEX_CREATE_TMP_TABLE)));
  write_bin_log(session, true, session->query, session->query_length);

  if (ha_check_storage_engine_flag(old_db_type, HTON_BIT_FLUSH_AFTER_RENAME))
  {
    /*
      For the alter table to be properly flushed to the logs, we
      have to open the new table.  If not, we get a problem on server
      shutdown. But we do not need to attach MERGE children.
    */
    char path[FN_REFLEN];
    Table *t_table;
    build_table_filename(path, sizeof(path), new_db, table_name, "", 0);
    t_table= open_temporary_table(session, path, new_db, tmp_name, false, OTM_OPEN);
    if (t_table)
    {
      intern_close_table(t_table);
      free(t_table);
    }
    else
      sql_print_warning(_("Could not open table %s.%s after rename\n"),
                        new_db,table_name);
    ha_flush_logs(old_db_type);
  }
  table_list->table=0;				// For query cache

  if (session->locked_tables && (new_name != table_name || new_db != db))
  {
    /*
      If are we under LOCK TABLES and did ALTER Table with RENAME we need
      to remove placeholders for the old table and for the target table
      from the list of open tables and table cache. If we are not under
      LOCK TABLES we can rely on close_thread_tables() doing this job.
    */
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(session, table, false);
    unlink_open_table(session, name_lock, false);
    pthread_mutex_unlock(&LOCK_open);
  }

end_temporary:
  snprintf(tmp_name, sizeof(tmp_name), ER(ER_INSERT_INFO),
           (ulong) (copied + deleted), (ulong) deleted,
           (ulong) session->cuted_fields);
  my_ok(session, copied + deleted, 0L, tmp_name);
  session->some_tables_deleted=0;
  return(false);

err1:
  if (new_table)
  {
    /* close_temporary_table() frees the new_table pointer. */
    close_temporary_table(session, new_table, 1, 1);
  }
  else
    quick_rm_table(new_db_type, new_db, tmp_name, FN_IS_TMP);

err:
  /*
    No default value was provided for a DATE/DATETIME field, the
    current sql_mode doesn't allow the '0000-00-00' value and
    the table to be altered isn't empty.
    Report error here.
  */
  if (alter_info->error_if_not_empty && session->row_count)
  {
    const char *f_val= 0;
    enum enum_drizzle_timestamp_type t_type= DRIZZLE_TIMESTAMP_DATE;
    switch (alter_info->datetime_field->sql_type)
    {
      case DRIZZLE_TYPE_DATE:
        f_val= "0000-00-00";
        t_type= DRIZZLE_TIMESTAMP_DATE;
        break;
      case DRIZZLE_TYPE_DATETIME:
        f_val= "0000-00-00 00:00:00";
        t_type= DRIZZLE_TIMESTAMP_DATETIME;
        break;
      default:
        /* Shouldn't get here. */
        assert(0);
    }
    bool save_abort_on_warning= session->abort_on_warning;
    session->abort_on_warning= true;
    make_truncated_value_warning(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                                 f_val, strlength(f_val), t_type,
                                 alter_info->datetime_field->field_name);
    session->abort_on_warning= save_abort_on_warning;
  }
  if (name_lock)
  {
    pthread_mutex_lock(&LOCK_open);
    unlink_open_table(session, name_lock, false);
    pthread_mutex_unlock(&LOCK_open);
  }
  return(true);

err_with_placeholders:
  /*
    An error happened while we were holding exclusive name-lock on table
    being altered. To be safe under LOCK TABLES we should remove placeholders
    from list of open tables list and table cache.
  */
  unlink_open_table(session, table, false);
  if (name_lock)
    unlink_open_table(session, name_lock, false);
  pthread_mutex_unlock(&LOCK_open);
  return(true);
}
/* mysql_alter_table */

static int
copy_data_between_tables(Table *from,Table *to,
			 List<Create_field> &create,
                         bool ignore,
			 uint32_t order_num, order_st *order,
			 ha_rows *copied,
			 ha_rows *deleted,
                         enum enum_enable_or_disable keys_onoff,
                         bool error_if_not_empty)
{
  int error;
  Copy_field *copy,*copy_end;
  ulong found_count,delete_count;
  Session *session= current_session;
  uint32_t length= 0;
  SORT_FIELD *sortorder;
  READ_RECORD info;
  TableList   tables;
  List<Item>   fields;
  List<Item>   all_fields;
  ha_rows examined_rows;
  bool auto_increment_field_copied= 0;
  ulong save_sql_mode;
  uint64_t prev_insert_id;

  /*
    Turn off recovery logging since rollback of an alter table is to
    delete the new table so there is no need to log the changes to it.

    This needs to be done before external_lock
  */
  error= ha_enable_transaction(session, false);
  if (error)
    return(-1);

  if (!(copy= new Copy_field[to->s->fields]))
    return(-1);				/* purecov: inspected */

  if (to->file->ha_external_lock(session, F_WRLCK))
    return(-1);

  /* We need external lock before we can disable/enable keys */
  alter_table_manage_keys(to, from->file->indexes_are_disabled(), keys_onoff);

  /* We can abort alter table for any table type */
  session->abort_on_warning= !ignore;

  from->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  to->file->ha_start_bulk_insert(from->file->stats.records);

  save_sql_mode= session->variables.sql_mode;

  List_iterator<Create_field> it(create);
  Create_field *def;
  copy_end=copy;
  for (Field **ptr=to->field ; *ptr ; ptr++)
  {
    def=it++;
    if (def->field)
    {
      if (*ptr == to->next_number_field)
        auto_increment_field_copied= true;

      (copy_end++)->set(*ptr,def->field,0);
    }

  }

  found_count=delete_count=0;

  if (order)
  {
    if (to->s->primary_key != MAX_KEY && to->file->primary_key_is_clustered())
    {
      char warn_buff[DRIZZLE_ERRMSG_SIZE];
      snprintf(warn_buff, sizeof(warn_buff),
               _("order_st BY ignored because there is a user-defined clustered "
                 "index in the table '%-.192s'"),
               from->s->table_name.str);
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                   warn_buff);
    }
    else
    {
      from->sort.io_cache=(IO_CACHE*) malloc(sizeof(IO_CACHE));
      memset(from->sort.io_cache, 0, sizeof(IO_CACHE));

      memset(&tables, 0, sizeof(tables));
      tables.table= from;
      tables.alias= tables.table_name= from->s->table_name.str;
      tables.db= from->s->db.str;
      error= 1;

      if (session->lex->select_lex.setup_ref_array(session, order_num) ||
          setup_order(session, session->lex->select_lex.ref_pointer_array,
                      &tables, fields, all_fields, order) ||
          !(sortorder= make_unireg_sortorder(order, &length, NULL)) ||
          (from->sort.found_records= filesort(session, from, sortorder, length,
                                              (SQL_SELECT *) 0, HA_POS_ERROR,
                                              1, &examined_rows)) ==
          HA_POS_ERROR)
        goto err;
    }
  };

  /* Tell handler that we have values for all columns in the to table */
  to->use_all_columns();
  init_read_record(&info, session, from, (SQL_SELECT *) 0, 1,1);
  if (ignore)
    to->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  session->row_count= 0;
  restore_record(to, s->default_values);        // Create empty record
  while (!(error=info.read_record(&info)))
  {
    if (session->killed)
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

    for (Copy_field *copy_ptr=copy ; copy_ptr != copy_end ; copy_ptr++)
    {
      copy_ptr->do_copy(copy_ptr);
    }
    prev_insert_id= to->file->next_insert_id;
    update_virtual_fields_marked_for_write(to, false);
    error=to->file->ha_write_row(to->record[0]);
    to->auto_increment_field_not_null= false;
    if (error)
    {
      if (!ignore ||
          to->file->is_fatal_error(error, HA_CHECK_DUP))
      {
         if (!to->file->is_fatal_error(error, HA_CHECK_DUP))
         {
           uint32_t key_nr= to->file->get_dup_key(error);
           if ((int) key_nr >= 0)
           {
             const char *err_msg= ER(ER_DUP_ENTRY_WITH_KEY_NAME);
             if (key_nr == 0 &&
                 (to->key_info[0].key_part[0].field->flags &
                  AUTO_INCREMENT_FLAG))
               err_msg= ER(ER_DUP_ENTRY_AUTOINCREMENT_CASE);
             to->file->print_keydup_error(key_nr, err_msg);
             break;
           }
         }

	to->file->print_error(error,MYF(0));
	break;
      }
      to->file->restore_auto_increment(prev_insert_id);
      delete_count++;
    }
    else
      found_count++;
  }
  end_read_record(&info);
  free_io_cache(from);
  delete [] copy;				// This is never 0

  if (to->file->ha_end_bulk_insert() && error <= 0)
  {
    to->file->print_error(my_errno,MYF(0));
    error=1;
  }
  to->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  if (ha_enable_transaction(session, true))
  {
    error= 1;
    goto err;
  }

  /*
    Ensure that the new table is saved properly to disk so that we
    can do a rename
  */
  if (ha_autocommit_or_rollback(session, 0))
    error=1;
  if (end_active_trans(session))
    error=1;

 err:
  session->variables.sql_mode= save_sql_mode;
  session->abort_on_warning= 0;
  free_io_cache(from);
  *copied= found_count;
  *deleted=delete_count;
  to->file->ha_release_auto_increment();
  if (to->file->ha_external_lock(session,F_UNLCK))
    error=1;
  return(error > 0 ? -1 : 0);
}


/*
  Recreates tables by calling mysql_alter_table().

  SYNOPSIS
    mysql_recreate_table()
    session			Thread handler
    tables		Tables to recreate

 RETURN
    Like mysql_alter_table().
*/
bool mysql_recreate_table(Session *session, TableList *table_list)
{
  HA_CREATE_INFO create_info;
  Alter_info alter_info;

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
  alter_info.flags= (ALTER_CHANGE_COLUMN | ALTER_RECREATE);
  return(mysql_alter_table(session, NULL, NULL, &create_info,
                                table_list, &alter_info, 0,
                                (order_st *) 0, 0));
}


bool mysql_checksum_table(Session *session, TableList *tables,
                          HA_CHECK_OPT *check_opt)
{
  TableList *table;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= session->protocol;

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null= 1;
  field_list.push_back(item= new Item_int("Checksum", (int64_t) 1,
                                          MY_INT64_NUM_DECIMAL_DIGITS));
  item->maybe_null= 1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return(true);

  /* Open one table after the other to keep lock time as short as possible. */
  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    Table *t;

    strxmov(table_name, table->db ,".", table->table_name, NULL);

    t= table->table= open_n_lock_single_table(session, table, TL_READ);
    session->clear_error();			// these errors shouldn't get client

    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);

    if (!t)
    {
      /* Table didn't exist */
      protocol->store_null();
      session->clear_error();
    }
    else
    {
      if (t->file->ha_table_flags() & HA_HAS_CHECKSUM &&
	  !(check_opt->flags & T_EXTEND))
	protocol->store((uint64_t)t->file->checksum());
      else if (!(t->file->ha_table_flags() & HA_HAS_CHECKSUM) &&
	       (check_opt->flags & T_QUICK))
	protocol->store_null();
      else
      {
	/* calculating table's checksum */
	ha_checksum crc= 0;
        unsigned char null_mask=256 -  (1 << t->s->last_null_bit_pos);

        t->use_all_columns();

	if (t->file->ha_rnd_init(1))
	  protocol->store_null();
	else
	{
	  for (;;)
	  {
	    ha_checksum row_crc= 0;
            int error= t->file->rnd_next(t->record[0]);
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
	  protocol->store((uint64_t)crc);
          t->file->ha_rnd_end();
	}
      }
      session->clear_error();
      close_thread_tables(session);
      table->table=0;				// For query cache
    }
    if (protocol->write())
      goto err;
  }

  my_eof(session);
  return(false);

 err:
  close_thread_tables(session);			// Shouldn't be needed
  if (table)
    table->table=0;
  return(true);
}

static bool check_engine(Session *session, const char *table_name,
                         HA_CREATE_INFO *create_info)
{
  handlerton **new_engine= &create_info->db_type;
  handlerton *req_engine= *new_engine;
  bool no_substitution= 1;
  if (!(*new_engine= ha_checktype(session, ha_legacy_type(req_engine),
                                  no_substitution, 1)))
    return true;

  if (req_engine && req_engine != *new_engine)
  {
    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                       ER_WARN_USING_OTHER_HANDLER,
                       ER(ER_WARN_USING_OTHER_HANDLER),
                       ha_resolve_storage_engine_name(*new_engine),
                       table_name);
  }
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE &&
      ha_check_storage_engine_flag(*new_engine, HTON_BIT_TEMPORARY_NOT_SUPPORTED))
  {
    if (create_info->used_fields & HA_CREATE_USED_ENGINE)
    {
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
               ha_resolve_storage_engine_name(*new_engine), "TEMPORARY");
      *new_engine= 0;
      return true;
    }
    *new_engine= myisam_hton;
  }
  return false;
}
