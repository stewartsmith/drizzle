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
#include <drizzled/global.h>
#include CSTDINT_H
#include CINTTYPES_H
#include <string>
#include <fstream>
#include <drizzled/message/schema.pb.h>
using namespace std;
#include <drizzled/server_includes.h>
#include <mysys/mysys_err.h>
#include <mysys/my_dir.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <mysys/hash.h>
#include <drizzled/session.h>
#include <drizzled/db.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/replication_services.h>
#include <drizzled/message/schema.pb.h>

using namespace drizzled;

#define MY_DB_OPT_FILE "db.opt"
#define MAX_DROP_TABLE_Q_LEN      1024

const char *del_exts[]= {".dfe", ".BAK", ".TMD",".opt", NULL};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts", del_exts, NULL};

static long mysql_rm_known_files(Session *session, MY_DIR *dirp,
                                 const char *db, const char *path,
                                 TableList **dropped_tables);

static void mysql_change_db_impl(Session *session, LEX_STRING *new_db_name);
            

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
static int write_schema_file(Session *session,
			     const char *path, const char *name,
			     HA_CREATE_INFO *create)
{
  message::Schema db;
  char schema_file_tmp[FN_REFLEN];
  string schema_file(path);

  assert(path);
  assert(name);
  assert(create);

  snprintf(schema_file_tmp, FN_REFLEN, "%s%c%s.tmpXXXXXX", path, FN_LIBCHAR, MY_DB_OPT_FILE);

  schema_file.append(1, FN_LIBCHAR);
  schema_file.append(MY_DB_OPT_FILE);

  int fd= mkstemp(schema_file_tmp);

  if (fd==-1)
    return errno;

  if (!create->default_table_charset)
    create->default_table_charset= session->variables.collation_server;

  db.set_name(name);
  db.set_collation(create->default_table_charset->name);

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

bool mysql_create_db(Session *session, const char *db, HA_CREATE_INFO *create_info, bool is_if_not_exists)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  char	 path[FN_REFLEN+16];
  long result= 1;
  int error_erno;
  bool error= false;
  uint32_t path_len;

  /* do not create 'information_schema' db */
  if (!my_strcasecmp(system_charset_info, db, INFORMATION_SCHEMA_NAME.c_str()))
  {
    my_error(ER_DB_CREATE_EXISTS, MYF(0), db);
    return(-1);
  }

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

  /* Check directory */
  path_len= build_table_filename(path, sizeof(path), db, "", false);
  path[path_len-1]= 0;                    // Remove last '/' from path

  if (mkdir(path,0777) == -1)
  {
    if (errno == EEXIST)
    {
      if (! is_if_not_exists)
      {
	my_error(ER_DB_CREATE_EXISTS, MYF(0), db);
	error= true;
	goto exit;
      }
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
			  ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS), db);
      session->my_ok();
      error= false;
      goto exit;
    }

    my_error(ER_CANT_CREATE_DB, MYF(0), db, my_errno);
    error= true;
    goto exit;
  }

  error_erno= write_schema_file(session, path, db, create_info);
  if (error_erno && error_erno != EEXIST)
  {
    if (rmdir(path) >= 0)
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

bool mysql_alter_db(Session *session, const char *db, HA_CREATE_INFO *create_info)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  long result=1;
  int error= 0;
  char	 path[FN_REFLEN+16];
  uint32_t path_len;

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

  pthread_mutex_lock(&LOCK_create_db);

  /* Change options if current database is being altered. */
  path_len= build_table_filename(path, sizeof(path), db, "", false);
  path[path_len-1]= 0;                    // Remove last '/' from path

  error= write_schema_file(session, path, db, create_info);
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

bool mysql_rm_db(Session *session,char *db,bool if_exists)
{
  long deleted=0;
  int error= false;
  char	path[FN_REFLEN+16];
  MY_DIR *dirp;
  uint32_t length;
  TableList *dropped_tables= NULL;

  if (db && (strcmp(db, "information_schema") == 0))
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
    error= -1;
    goto exit2;
  }

  pthread_mutex_lock(&LOCK_create_db);

  length= build_table_filename(path, sizeof(path), db, "", false);
  strcpy(path+length, MY_DB_OPT_FILE);         // Append db option file name
  unlink(path);
  path[length]= '\0';				// Remove file name

  /* See if the directory exists */
  if (!(dirp= my_dir(path,MYF(MY_DONT_SORT))))
  {
    if (!if_exists)
    {
      error= -1;
      my_error(ER_DB_DROP_EXISTS, MYF(0), db);
      goto exit;
    }
    else
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
			  ER_DB_DROP_EXISTS, ER(ER_DB_DROP_EXISTS), db);
  }
  else
  {
    pthread_mutex_lock(&LOCK_open); /* After deleting database, remove all cache entries related to schema */
    remove_db_from_cache(db);
    pthread_mutex_unlock(&LOCK_open);


    error= -1;
    if ((deleted= mysql_rm_known_files(session, dirp, db, path, &dropped_tables)) >= 0)
    {
      plugin::StorageEngine::dropDatabase(path);
      error = 0;
    }
  }
  if (deleted >= 0)
  {
    const char *query;
    uint32_t query_length;
    if (!session->query)
    {
      /* The client used the old obsolete mysql_drop_db() call */
      query= path;
      query_length= sprintf(path, "drop database `%s`", db);
    }
    else
    {
      query= session->query;
      query_length= session->query_length;
    }
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
    db_len= strlen(db);

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
  if (! session->db.empty() && ! strcmp(session->db.c_str(), db))
    mysql_change_db_impl(session, NULL);
  pthread_mutex_unlock(&LOCK_create_db);
  start_waiting_global_read_lock(session);
exit2:
  return(error);
}

/*
  Removes files with known extensions plus.
  session MUST be set when calling this function!
*/

static long mysql_rm_known_files(Session *session, MY_DIR *dirp, const char *db,
				 const char *org_path,
                                 TableList **dropped_tables)
{
  long deleted= 0;
  char filePath[FN_REFLEN];
  TableList *tot_list= NULL, **tot_list_next;

  tot_list_next= &tot_list;

  for (uint32_t idx= 0;
       idx < (uint32_t) dirp->number_off_files && !session->killed ;
       idx++)
  {
    FILEINFO *file=dirp->dir_entry+idx;
    char *extension;

    /* skiping . and .. */
    if (file->name[0] == '.' && (!file->name[1] ||
       (file->name[1] == '.' &&  !file->name[2])))
      continue;

    if (!(extension= strrchr(file->name, '.')))
      extension= strchr(file->name, '\0');
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
    if (db && !my_strcasecmp(files_charset_info,
                             extension, ".dfe"))
    {
      uint32_t db_len= strlen(db);

      /* Drop the table nicely */
      *extension= 0;			// Remove extension
      TableList *table_list=(TableList*)
                              session->calloc(sizeof(*table_list) +
                                          db_len + 1 +
                                          strlen(file->name) + 1);

      if (!table_list)
        goto err;
      table_list->db= (char*) (table_list+1);
      table_list->table_name= strcpy(table_list->db, db) + db_len + 1;
      filename_to_tablename(file->name, table_list->table_name,
                            strlen(file->name) + 1);
      table_list->alias= table_list->table_name;	// If lower_case_table_names=2
      table_list->internal_tmp_table= is_prefix(file->name, TMP_FILE_PREFIX);
      /* Link into list */
      (*tot_list_next)= table_list;
      tot_list_next= &table_list->next_local;
      deleted++;
    }
    else
    {
      sprintf(filePath, "%s/%s", org_path, file->name);
      if (my_delete_with_symlink(filePath,MYF(MY_WME)))
      {
	goto err;
      }
    }
  }
  if (session->killed ||
      (tot_list && mysql_rm_table_part2(session, tot_list, true, false, true)))
    goto err;

  my_dirend(dirp);

  if (dropped_tables)
    *dropped_tables= tot_list;

  if (rmdir(org_path))
  {
    my_error(ER_DB_DROP_RMDIR, MYF(0), org_path, errno);
    return -1;
  }

  return deleted;

err:
  my_dirend(dirp);
  return -1;
}

/**
  @brief Internal implementation: switch current database to a valid one.

  @param session            Thread context.
  @param new_db_name    Name of the database to switch to. The function will
                        take ownership of the name (the caller must not free
                        the allocated memory). If the name is NULL, we're
                        going to switch to NULL db.
  @param new_db_charset Character set of the new database.
*/

static void mysql_change_db_impl(Session *session, LEX_STRING *new_db_name)
{
  /* 1. Change current database in Session. */

  if (new_db_name == NULL)
  {
    /*
      Session::set_db() does all the job -- it frees previous database name and
      sets the new one.
    */

    session->set_db(NULL, 0);
  }
  else
  {
    /*
      Here we already have a copy of database name to be used in Session. So,
      we just call Session::reset_db(). Since Session::reset_db() does not releases
      the previous database name, we should do it explicitly.
    */

    session->set_db(new_db_name->str, new_db_name->length);
  }
}

/**
  Return true if db1_name is equal to db2_name, false otherwise.

  The function allows to compare database names according to the MySQL
  rules. The database names db1 and db2 are equal if:
     - db1 is NULL and db2 is NULL;
     or
     - db1 is not-NULL, db2 is not-NULL, db1 is equal (ignoring case) to
       db2 in system character set (UTF8).
*/

static inline bool
cmp_db_names(const char *db1_name,
             const char *db2_name)
{
  return
         /* db1 is NULL and db2 is NULL */
         (!db1_name && !db2_name) ||

         /* db1 is not-NULL, db2 is not-NULL, db1 == db2. */
         (db1_name && db2_name && my_strcasecmp(system_charset_info, db1_name, db2_name) == 0);
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

bool mysql_change_db(Session *session, const LEX_STRING *new_db_name, bool force_switch)
{
  LEX_STRING new_db_file_name;
  const CHARSET_INFO *db_default_cl;

  assert(new_db_name);
  assert(new_db_name->length);

  if (my_strcasecmp(system_charset_info, new_db_name->str,
                    INFORMATION_SCHEMA_NAME.c_str()) == 0)
  {
    /* Switch the current database to INFORMATION_SCHEMA. */
    /* const_cast<> is safe here: mysql_change_db_impl does a copy */
    LEX_STRING is_name= { const_cast<char *>(INFORMATION_SCHEMA_NAME.c_str()),
                          INFORMATION_SCHEMA_NAME.length() };
    mysql_change_db_impl(session, &is_name);

    return false;
  }

  /*
    Now we need to make a copy because check_db_name requires a
    non-constant argument. Actually, it takes database file name.

    TODO: fix check_db_name().
  */

  new_db_file_name.length= new_db_name->length;
  new_db_file_name.str= (char *)malloc(new_db_name->length + 1);
  if (new_db_file_name.str == NULL)
    return true;                             /* the error is set */
  memcpy(new_db_file_name.str, new_db_name->str, new_db_name->length);
  new_db_file_name.str[new_db_name->length]= 0;


  /*
    NOTE: if check_db_name() fails, we should throw an error in any case,
    even if we are called from sp_head::execute().

    It's next to impossible however to get this error when we are called
    from sp_head::execute(). But let's switch the current database to NULL
    in this case to be sure.
  */

  if (check_db_name(&new_db_file_name))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), new_db_file_name.str);
    free(new_db_file_name.str);

    if (force_switch)
      mysql_change_db_impl(session, NULL);

    return true;
  }

  if (check_db_dir_existence(new_db_file_name.str))
  {
    if (force_switch)
    {
      /* Throw a warning and free new_db_file_name. */

      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_BAD_DB_ERROR, ER(ER_BAD_DB_ERROR),
                          new_db_file_name.str);

      free(new_db_file_name.str);

      /* Change db to NULL. */

      mysql_change_db_impl(session, NULL);

      /* The operation succeed. */

      return false;
    }
    else
    {
      /* Report an error and free new_db_file_name. */

      my_error(ER_BAD_DB_ERROR, MYF(0), new_db_file_name.str);
      free(new_db_file_name.str);

      /* The operation failed. */

      return true;
    }
  }

  db_default_cl= get_default_db_collation(new_db_file_name.str);

  mysql_change_db_impl(session, &new_db_file_name);
  free(new_db_file_name.str);

  return false;
}

/*
  Check if there is directory for the database name.

  SYNOPSIS
    check_db_dir_existence()
    db_name   database name

  RETURN VALUES
    false   There is directory for the specified database name.
    true    The directory does not exist.
*/

bool check_db_dir_existence(const char *db_name)
{
  char db_dir_path[FN_REFLEN];
  uint32_t db_dir_path_len;

  db_dir_path_len= build_table_filename(db_dir_path, sizeof(db_dir_path),
                                        db_name, "", false);

  if (db_dir_path_len && db_dir_path[db_dir_path_len - 1] == FN_LIBCHAR)
    db_dir_path[db_dir_path_len - 1]= 0;

  /* Check access. */

  return my_access(db_dir_path, F_OK);
}
