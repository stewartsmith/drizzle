/* Copyright (C) 2000-2003 MySQL AB

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


/* create and drop of databases */
#include "config.h"
#include <string>
#include <fstream>
#include <fcntl.h>
#include <drizzled/message/schema.pb.h>
#include "drizzled/my_error.h"
#include "drizzled/internal/my_dir.h"
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/my_hash.h>
#include "drizzled/internal/m_string.h"
#include <drizzled/session.h>
#include <drizzled/db.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/replication_services.h>
#include <drizzled/message/schema.pb.h>
#include "drizzled/sql_table.h"
#include "drizzled/plugin/info_schema_table.h"
#include "drizzled/global_charset_info.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/charset.h"

#include "drizzled/internal/my_sys.h"

using namespace std;
using namespace drizzled;

#define MY_DB_OPT_FILE "db.opt"
#define MAX_DROP_TABLE_Q_LEN      1024

const char *del_exts[]= {".dfe", ".blk", ".arz", ".BAK", ".TMD",".opt", NULL};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts", del_exts, NULL};

static long mysql_rm_known_files(Session *session, CachedDirectory &dirp,
                                 const string &db, const char *path,
                                 TableList **dropped_tables);

/**
  Return default database collation.

  @param session     Thread context.
  @param db_name Database name.

  @return CHARSET_INFO object. The operation always return valid character
    set, even if the database does not exist.
*/

const CHARSET_INFO *get_default_db_collation(const char *db_name)
{
  message::Schema db;

  get_database_metadata(db_name, &db);

  /* If for some reason the db.opt file lacks a collation,
     we just return the default */

  if (db.has_collation())
  {
    const string buffer= db.collation();
    const CHARSET_INFO* cs= get_charset_by_name(buffer.c_str());

    if (!cs)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Error while loading database options: '%s':"),db_name);
      errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_UNKNOWN_COLLATION), buffer.c_str());

      return default_charset_info;
    }

    return cs;
  }

  return default_charset_info;
}

/* path is path to database, not schema file */
static int write_schema_file(const DatabasePathName &path, const message::Schema &db)
{
  char schema_file_tmp[FN_REFLEN];
  string schema_file(path.to_string());

  snprintf(schema_file_tmp, FN_REFLEN, "%s%c%s.tmpXXXXXX", path.to_string().c_str(), FN_LIBCHAR, MY_DB_OPT_FILE);

  schema_file.append(1, FN_LIBCHAR);
  schema_file.append(MY_DB_OPT_FILE);

  int fd= mkstemp(schema_file_tmp);

  if (fd==-1)
    return errno;


  if (!db.SerializeToFileDescriptor(fd))
  {
    close(fd);
    unlink(schema_file_tmp);
    return -1;
  }

  if (rename(schema_file_tmp, schema_file.c_str()) == -1)
  {
    close(fd);
    return errno;
  }

  close(fd);
  return 0;
}

int get_database_metadata(const char *dbname, message::Schema *db)
{
  char db_opt_path[FN_REFLEN];
  size_t length;

  /*
    Pass an empty file name, and the database options file name as extension
    to avoid table name to file name encoding.
  */
  length= build_table_filename(db_opt_path, sizeof(db_opt_path),
                              dbname, "", false);
  strcpy(db_opt_path + length, MY_DB_OPT_FILE);

  int fd= open(db_opt_path, O_RDONLY);

  if (fd == -1)
    return errno;

  if (!db->ParseFromFileDescriptor(fd))
  {
    close(fd);
    return -1;
  }
  close(fd);

  return 0;
}

/*
  Create a database

  SYNOPSIS
  mysql_create_db()
  session		Thread handler
  db		Name of database to create
		Function assumes that this is already validated.
  create_info	Database create options (like character set)

  SIDE-EFFECTS
   1. Report back to client that command succeeded (my_ok)
   2. Report errors to client
   3. Log event to binary log

  RETURN VALUES
  false ok
  true  Error

*/

bool mysql_create_db(Session *session, const NormalisedDatabaseName &database_name, message::Schema *schema_message, bool is_if_not_exists)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  long result= 1;
  int error_erno;
  bool error= false;
  DatabasePathName database_path(database_name);

  /* do not create 'information_schema' db */
  if (!my_strcasecmp(system_charset_info, database_name.to_string().c_str(), INFORMATION_SCHEMA_NAME.c_str()))
  {
    my_error(ER_DB_CREATE_EXISTS, MYF(0), database_name.to_string().c_str());
    return(-1);
  }

  assert(database_name.isValid());
  schema_message->set_name(database_name.to_string());

  /*
    Do not create database if another thread is holding read lock.
    Wait for global read lock before acquiring LOCK_create_db.
    After wait_if_global_read_lock() we have protection against another
    global read lock. If we would acquire LOCK_create_db first,
    another thread could step in and get the global read lock before we
    reach wait_if_global_read_lock(). If this thread tries the same as we
    (admin a db), it would then go and wait on LOCK_create_db...
    Furthermore wait_if_global_read_lock() checks if the current thread
    has the global read lock and refuses the operation with
    ER_CANT_UPDATE_WITH_READLOCK if applicable.
  */
  if (wait_if_global_read_lock(session, 0, 1))
  {
    error= true;
    goto exit2;
  }

  pthread_mutex_lock(&LOCK_create_db);

  if (mkdir(database_path.to_string().c_str(),0777) == -1)
  {
    if (errno == EEXIST)
    {
      if (! is_if_not_exists)
      {
	my_error(ER_DB_CREATE_EXISTS, MYF(0), database_name.to_string().c_str());
	error= true;
	goto exit;
      }
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
			  ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS),
                          database_name.to_string().c_str());
      session->my_ok();
      error= false;
      goto exit;
    }

    my_error(ER_CANT_CREATE_DB, MYF(0), database_name.to_string().c_str(), errno);
    error= true;
    goto exit;
  }

  error_erno= write_schema_file(database_path, *schema_message);
  if (error_erno && error_erno != EEXIST)
  {
    if (rmdir(database_path.to_string().c_str()) >= 0)
    {
      error= true;
      goto exit;
    }
  }
  else if (error_erno)
    error= true;

  replication_services.rawStatement(session, session->query, session->query_length);
  session->my_ok(result);

exit:
  pthread_mutex_unlock(&LOCK_create_db);
  start_waiting_global_read_lock(session);
exit2:
  return error;
}


/* db-name is already validated when we come here */

bool mysql_alter_db(Session *session, const NormalisedDatabaseName &database_name, message::Schema *schema_message)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  long result=1;
  int error= 0;
  DatabasePathName database_path(database_name);

  /*
    Do not alter database if another thread is holding read lock.
    Wait for global read lock before acquiring LOCK_create_db.
    After wait_if_global_read_lock() we have protection against another
    global read lock. If we would acquire LOCK_create_db first,
    another thread could step in and get the global read lock before we
    reach wait_if_global_read_lock(). If this thread tries the same as we
    (admin a db), it would then go and wait on LOCK_create_db...
    Furthermore wait_if_global_read_lock() checks if the current thread
    has the global read lock and refuses the operation with
    ER_CANT_UPDATE_WITH_READLOCK if applicable.
  */
  if ((error=wait_if_global_read_lock(session,0,1)))
    goto exit;

  assert(schema_message);
  assert(database_name.isValid());

  schema_message->set_name(database_name.to_string());

  pthread_mutex_lock(&LOCK_create_db);

  error= write_schema_file(database_path, *schema_message);
  if (error && error != EEXIST)
  {
    /* TODO: find some witty way of getting back an error message */
    pthread_mutex_unlock(&LOCK_create_db);
    goto exit;
  }

  replication_services.rawStatement(session, session->getQueryString(), session->getQueryLength());
  session->my_ok(result);

  pthread_mutex_unlock(&LOCK_create_db);
  start_waiting_global_read_lock(session);
exit:
  return error ? true : false;
}


/*
  Drop all tables in a database and the database itself

  SYNOPSIS
    mysql_rm_db()
    session			Thread handle
    db			Database name in the case given by user
		        It's already validated and set to lower case
                        (if needed) when we come here
    if_exists		Don't give error if database doesn't exists
    silent		Don't generate errors

  RETURN
    false ok (Database dropped)
    ERROR Error
*/

bool mysql_rm_db(Session *session, const NormalisedDatabaseName &database_name, bool if_exists)
{
  long deleted=0;
  int error= false;
  char	path[FN_REFLEN+16];
  uint32_t length;
  TableList *dropped_tables= NULL;

  if (database_name.to_string().compare(INFORMATION_SCHEMA_NAME) == 0)
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), "", "", INFORMATION_SCHEMA_NAME.c_str());
    return true;
  }

  /*
    Do not drop database if another thread is holding read lock.
    Wait for global read lock before acquiring LOCK_create_db.
    After wait_if_global_read_lock() we have protection against another
    global read lock. If we would acquire LOCK_create_db first,
    another thread could step in and get the global read lock before we
    reach wait_if_global_read_lock(). If this thread tries the same as we
    (admin a db), it would then go and wait on LOCK_create_db...
    Furthermore wait_if_global_read_lock() checks if the current thread
    has the global read lock and refuses the operation with
    ER_CANT_UPDATE_WITH_READLOCK if applicable.
  */
  if (wait_if_global_read_lock(session, 0, 1))
  {
    return -1;
  }

  pthread_mutex_lock(&LOCK_create_db);

  length= build_table_filename(path, sizeof(path),
                               database_name.to_string().c_str(), "", false);
  strcpy(path+length, MY_DB_OPT_FILE);         // Append db option file name
  unlink(path);
  path[length]= '\0';				// Remove file name

  /* See if the directory exists */
  CachedDirectory dirp(path);
  if (dirp.fail())
  {
    if (!if_exists)
    {
      error= -1;
      my_error(ER_DB_DROP_EXISTS, MYF(0), database_name.to_string().c_str());
      goto exit;
    }
    else
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
			  ER_DB_DROP_EXISTS, ER(ER_DB_DROP_EXISTS),
                          database_name.to_string().c_str());
  }
  else
  {
    pthread_mutex_lock(&LOCK_open); /* After deleting database, remove all cache entries related to schema */
    remove_db_from_cache(database_name.to_string().c_str());
    pthread_mutex_unlock(&LOCK_open);


    error= -1;
    deleted= mysql_rm_known_files(session, dirp,
                                  database_name.to_string(),
                                  path, &dropped_tables);
    if (deleted >= 0)
    {
      plugin::StorageEngine::dropDatabase(path);
      error = 0;
    }
  }
  if (deleted >= 0)
  {
    const char *query;
    uint32_t query_length;

    assert(session->query);

    query= session->query;
    query_length= session->query_length;

    ReplicationServices &replication_services= ReplicationServices::singleton();
    replication_services.rawStatement(session, session->getQueryString(), session->getQueryLength());
    session->clear_error();
    session->server_status|= SERVER_STATUS_DB_DROPPED;
    session->my_ok((uint32_t) deleted);
    session->server_status&= ~SERVER_STATUS_DB_DROPPED;
  }
  else
  {
    char *query, *query_pos, *query_end, *query_data_start;
    TableList *tbl;
    uint32_t db_len;

    if (!(query= (char*) session->alloc(MAX_DROP_TABLE_Q_LEN)))
      goto exit; /* not much else we can do */
    query_pos= query_data_start= strcpy(query,"drop table ")+11;
    query_end= query + MAX_DROP_TABLE_Q_LEN;
    db_len= database_name.to_string().length();

    ReplicationServices &replication_services= ReplicationServices::singleton();
    for (tbl= dropped_tables; tbl; tbl= tbl->next_local)
    {
      uint32_t tbl_name_len;

      /* 3 for the quotes and the comma*/
      tbl_name_len= strlen(tbl->table_name) + 3;
      if (query_pos + tbl_name_len + 1 >= query_end)
      {
        /* These DDL methods and logging protected with LOCK_create_db */
        replication_services.rawStatement(session, query, (size_t) (query_pos -1 - query));
        query_pos= query_data_start;
      }

      *query_pos++ = '`';
      query_pos= strcpy(query_pos,tbl->table_name) + (tbl_name_len-3);
      *query_pos++ = '`';
      *query_pos++ = ',';
    }

    if (query_pos != query_data_start)
    {
      /* These DDL methods and logging protected with LOCK_create_db */
      replication_services.rawStatement(session, query, (size_t) (query_pos -1 - query));
    }
  }

exit:
  /*
    If this database was the client's selected database, we silently
    change the client's selected database to nothing (to have an empty
    SELECT DATABASE() in the future). For this we free() session->db and set
    it to 0.
  */
  if (! session->db.empty() && session->db.compare(database_name.to_string()) == 0)
    session->clear_db();
  pthread_mutex_unlock(&LOCK_create_db);
  start_waiting_global_read_lock(session);
  return error;
}


static int rm_table_part2(Session *session, TableList *tables)
{
  TableList *table;
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

  if (lock_table_names_exclusively(session, tables))
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
    }

    TableIdentifier identifier(db, table->table_name, table->internal_tmp_table ? INTERNAL_TMP_TABLE : NO_TMP_TABLE);

    if ((table_type == NULL
          && (plugin::StorageEngine::getTableDefinition(*session,
                                                        identifier) != EEXIST)))
    {
      // Table was not found on disk and table can't be created from engine
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
                          table->table_name);
    }
    else
    {
      error= plugin::StorageEngine::dropTable(*session,
                                              identifier,
                                              false);

      if ((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE))
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

    if (error == 0 || (foreign_key_error == false))
        write_bin_log_drop_table(session, true, db, table->table_name);

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
  Removes files with known extensions plus.
  session MUST be set when calling this function!
*/

static long mysql_rm_known_files(Session *session, CachedDirectory &dirp,
                                 const string &db,
				 const char *org_path,
                                 TableList **dropped_tables)
{
  long deleted= 0;
  char filePath[FN_REFLEN];
  TableList *tot_list= NULL, **tot_list_next;

  tot_list_next= &tot_list;

  for (CachedDirectory::Entries::const_iterator iter= dirp.getEntries().begin();
       iter != dirp.getEntries().end() && !session->killed;
       ++iter)
  {
    const CachedDirectory::Entry *file= *iter;
    char *extension;

    /* skiping . and .. */
    if (file->filename[0] == '.' && (!file->filename[1] ||
       (file->filename[1] == '.' &&  !file->filename[2])))
      continue;

    if (!(extension= strrchr(file->filename.c_str(), '.')))
      extension= strchr(file->filename.c_str(), '\0');
    if (find_type(extension, &deletable_extentions,1+2) <= 0)
    {
      /*
        ass ass ass.

        strange checking for magic extensions that are then deleted if
        not reg_ext (i.e. .frm).

        and (previously) we'd err out on drop database if files not matching
        engine ha_known_exts() or deletable_extensions were present.

        presumably this was to avoid deleting other user data... except if that
        data happened to be in files ending in .BAK, .opt or .TMD. *fun*
       */
      continue;
    }
    /* just for safety we use files_charset_info */
    if (!my_strcasecmp(files_charset_info, extension, ".dfe"))
    {
      size_t db_len= db.size();

      /* Drop the table nicely */
      *extension= 0;			// Remove extension
      TableList *table_list=(TableList*)
                              session->calloc(sizeof(*table_list) +
                                          db_len + 1 +
                                          file->filename.size() + 1);

      if (!table_list)
        return -1;
      table_list->db= (char*) (table_list+1);
      table_list->table_name= strcpy(table_list->db, db.c_str()) + db_len + 1;
      filename_to_tablename(file->filename.c_str(), table_list->table_name,
                            file->filename.size() + 1);
      table_list->alias= table_list->table_name;  // If lower_case_table_names=2
      table_list->internal_tmp_table= (strncmp(file->filename.c_str(),
                                               TMP_FILE_PREFIX,
                                               strlen(TMP_FILE_PREFIX)) == 0);
      /* Link into list */
      (*tot_list_next)= table_list;
      tot_list_next= &table_list->next_local;
      deleted++;
    }
    else
    {
      sprintf(filePath, "%s/%s", org_path, file->filename.c_str());
      if (my_delete_with_symlink(filePath,MYF(MY_WME)))
      {
	return -1;
      }
    }
  }
  if (session->killed)
    return -1;

  if (tot_list)
  {
    if (rm_table_part2(session, tot_list))
      return -1;
  }

  if (dropped_tables)
    *dropped_tables= tot_list;

  if (rmdir(org_path))
  {
    my_error(ER_DB_DROP_RMDIR, MYF(0), org_path, errno);
    return -1;
  }

  return deleted;
}

/**
  @brief Change the current database and its attributes unconditionally.

  @param session          thread handle
  @param new_db_name  database name
  @param force_switch if force_switch is false, then the operation will fail if

                        - new_db_name is NULL or empty;

                        - OR new database name is invalid
                          (check_db_name() failed);

                        - OR user has no privilege on the new database;

                        - OR new database does not exist;

                      if force_switch is true, then

                        - if new_db_name is NULL or empty, the current
                          database will be NULL, @@collation_database will
                          be set to @@collation_server, the operation will
                          succeed.

                        - if new database name is invalid
                          (check_db_name() failed), the current database
                          will be NULL, @@collation_database will be set to
                          @@collation_server, but the operation will fail;

                        - user privileges will not be checked
                          (Session::db_access however is updated);

                          TODO: is this really the intention?
                                (see sp-security.test).

                        - if new database does not exist,the current database
                          will be NULL, @@collation_database will be set to
                          @@collation_server, a warning will be thrown, the
                          operation will succeed.

  @details The function checks that the database name corresponds to a
  valid and existent database, checks access rights and changes the current
  database with database attributes (@@collation_database session variable,
  Session::db_access).

  This function is not the only way to switch the database that is
  currently employed. When the replication slave thread switches the
  database before executing a query, it calls session->set_db directly.
  However, if the query, in turn, uses a stored routine, the stored routine
  will use this function, even if it's run on the slave.

  This function allocates the name of the database on the system heap: this
  is necessary to be able to uniformly change the database from any module
  of the server. Up to 5.0 different modules were using different memory to
  store the name of the database, and this led to memory corruption:
  a stack pointer set by Stored Procedures was used by replication after
  the stack address was long gone.

  @return Operation status
    @retval false Success
    @retval true  Error
*/

bool mysql_change_db(Session *session, const NormalisedDatabaseName &normalised_database_name, bool force_switch)
{
  const CHARSET_INFO *db_default_cl;

  if (my_strcasecmp(system_charset_info, normalised_database_name.to_string().c_str(),
                    INFORMATION_SCHEMA_NAME.c_str()) == 0)
  {
    NonNormalisedDatabaseName non_normalised_i_s(INFORMATION_SCHEMA_NAME);
    NormalisedDatabaseName is_name(non_normalised_i_s);

    session->set_db(is_name);
    return false;
  }

  /*
    NOTE: if check_db_name() fails, we should throw an error in any case,
    even if we are called from sp_head::execute().

    It's next to impossible however to get this error when we are called
    from sp_head::execute(). But let's switch the current database to NULL
    in this case to be sure.
  */
  if (! normalised_database_name.isValid())
  {
    my_error(ER_WRONG_DB_NAME, MYF(0),
             normalised_database_name.to_string().c_str());

    if (force_switch)
      session->clear_db();

    return true;
  }

  DatabasePathName database_path(normalised_database_name);

  if (! database_path.exists())
  {
    if (force_switch)
    {
      /* Throw a warning and free new_db_file_name. */

      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_BAD_DB_ERROR, ER(ER_BAD_DB_ERROR),
                          normalised_database_name.to_string().c_str());

      /* Change db to NULL. */

      session->clear_db();

      /* The operation succeed. */

      return false;
    }
    else
    {
      /* Report an error and free new_db_file_name. */

      my_error(ER_BAD_DB_ERROR, MYF(0),
               normalised_database_name.to_string().c_str());

      /* The operation failed. */

      return true;
    }
  }

  db_default_cl= get_default_db_collation(normalised_database_name.to_string().c_str());

  session->set_db(normalised_database_name);

  return false;
}

NormalisedDatabaseName::NormalisedDatabaseName(const NonNormalisedDatabaseName &dbname)
{
  const std::string &non_norm_string= dbname.to_string();
  database_name= (char*)malloc(non_norm_string.size()+1);

  assert(database_name); /* FIXME: should throw exception */

  strncpy(database_name, non_norm_string.c_str(), non_norm_string.size()+1);

  my_casedn_str(files_charset_info, database_name);
}

NormalisedDatabaseName::~NormalisedDatabaseName()
{
  free(database_name);
}

bool NormalisedDatabaseName::isValid() const
{
  LEX_STRING db_lexstring;

  db_lexstring.str= database_name;
  db_lexstring.length= strlen(database_name);

  if (db_lexstring.length == 0
      || db_lexstring.length > NAME_LEN
      || database_name[db_lexstring.length - 1] == ' ')
    return false;

  return (! check_identifier_name(&db_lexstring));
}

DatabasePathName::DatabasePathName(const NormalisedDatabaseName &database_name)
{
  char db_dir_path[FN_REFLEN];
  uint32_t db_dir_path_len;

  db_dir_path_len= build_table_filename(db_dir_path, sizeof(db_dir_path),
                                        database_name.to_string().c_str(),
                                        "", false);

  if (db_dir_path_len && db_dir_path[db_dir_path_len - 1] == FN_LIBCHAR)
    db_dir_path[db_dir_path_len - 1]= 0;

  database_path.assign(db_dir_path);
}

bool DatabasePathName::exists() const
{
  /* TODO: handle EIO and other fun errors */
  return access(database_path.c_str(), F_OK) == 0;
}
