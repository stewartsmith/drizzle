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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


/* create and drop of databases */
#include "config.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <set>
#include <string>
#include <fstream>

#include <drizzled/message/schema.pb.h>
#include "drizzled/error.h"
#include <drizzled/gettext.h>
#include <drizzled/my_hash.h>
#include "drizzled/internal/m_string.h"
#include <drizzled/session.h>
#include <drizzled/db.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/transaction_services.h>
#include <drizzled/message/schema.pb.h>
#include "drizzled/sql_table.h"
#include "drizzled/plugin/storage_engine.h"
#include "drizzled/plugin/authorization.h"
#include "drizzled/global_charset_info.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/charset.h"

#include <boost/thread/mutex.hpp>

boost::mutex LOCK_create_db;

#include "drizzled/internal/my_sys.h"

#define MAX_DROP_TABLE_Q_LEN      1024

using namespace std;

namespace drizzled
{

static long drop_tables_via_filenames(Session *session,
                                 SchemaIdentifier &schema_identifier,
                                 TableIdentifiers &dropped_tables);
static void mysql_change_db_impl(Session *session);
static void mysql_change_db_impl(Session *session, SchemaIdentifier &schema_identifier);

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

bool mysql_create_db(Session *session, const message::Schema &schema_message, const bool is_if_not_exists)
{
  TransactionServices &transaction_services= TransactionServices::singleton();
  bool error= false;

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
    return false;
  }

  assert(schema_message.has_name());
  assert(schema_message.has_collation());

  // @todo push this lock down into the engine
  {
    boost::mutex::scoped_lock scopedLock(LOCK_create_db);

    // Check to see if it exists already.  
    SchemaIdentifier schema_identifier(schema_message.name());
    if (plugin::StorageEngine::doesSchemaExist(schema_identifier))
    {
      if (not is_if_not_exists)
      {
        my_error(ER_DB_CREATE_EXISTS, MYF(0), schema_message.name().c_str());
        error= true;
      }
      else
      {
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS),
                            schema_message.name().c_str());
        session->my_ok();
      }
    }
    else if (not plugin::StorageEngine::createSchema(schema_message)) // Try to create it 
    {
      my_error(ER_CANT_CREATE_DB, MYF(0), schema_message.name().c_str(), errno);
      error= true;
    }
    else // Created !
    {
      transaction_services.createSchema(session, schema_message);
      session->my_ok(1);
    }
  }
  start_waiting_global_read_lock(session);

  return error;
}


/* db-name is already validated when we come here */

bool mysql_alter_db(Session *session, const message::Schema &schema_message)
{
  TransactionServices &transaction_services= TransactionServices::singleton();

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
  if ((wait_if_global_read_lock(session, 0, 1)))
    return false;

  bool success;
  {
    boost::mutex::scoped_lock scopedLock(LOCK_create_db);

    SchemaIdentifier schema_idenifier(schema_message.name());
    if (not plugin::StorageEngine::doesSchemaExist(schema_idenifier))
    {
      my_error(ER_SCHEMA_DOES_NOT_EXIST, MYF(0), schema_message.name().c_str());
      return false;
    }

    /* Change options if current database is being altered. */
    success= plugin::StorageEngine::alterSchema(schema_message);

    if (success)
    {
      transaction_services.rawStatement(session, session->getQueryString());
      session->my_ok(1);
    }
    else
    {
      my_error(ER_ALTER_SCHEMA, MYF(0), schema_message.name().c_str());
    }
  }
  start_waiting_global_read_lock(session);

  return success;
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

bool mysql_rm_db(Session *session, SchemaIdentifier &schema_identifier, const bool if_exists)
{
  long deleted=0;
  int error= false;
  TableIdentifiers dropped_tables;
  message::Schema schema_proto;

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

  // Lets delete the temporary tables first outside of locks.  
  set<string> set_of_names;
  session->doGetTableNames(schema_identifier, set_of_names);

  for (set<string>::iterator iter= set_of_names.begin(); iter != set_of_names.end(); iter++)
  {
    TableIdentifier identifier(schema_identifier, *iter, message::Table::TEMPORARY);
    Table *table= session->find_temporary_table(identifier);
    session->close_temporary_table(table);
  }

  {
    boost::mutex::scoped_lock scopedLock(LOCK_create_db);

    /* See if the schema exists */
    if (not plugin::StorageEngine::doesSchemaExist(schema_identifier))
    {
      if (if_exists)
      {
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_DB_DROP_EXISTS, ER(ER_DB_DROP_EXISTS),
                            schema_identifier.getSQLPath().c_str());
      }
      else
      {
        error= -1;
        my_error(ER_DB_DROP_EXISTS, MYF(0), schema_identifier.getSQLPath().c_str());
        goto exit;
      }
    }
    else
    {
      LOCK_open.lock(); /* After deleting database, remove all cache entries related to schema */
      remove_db_from_cache(schema_identifier);
      LOCK_open.unlock();


      error= -1;
      deleted= drop_tables_via_filenames(session, schema_identifier, dropped_tables);
      if (deleted >= 0)
      {
        error= 0;
      }
    }
    if (deleted >= 0)
    {
      assert(! session->query.empty());

      TransactionServices &transaction_services= TransactionServices::singleton();
      transaction_services.dropSchema(session, schema_identifier.getSchemaName());
      session->clear_error();
      session->server_status|= SERVER_STATUS_DB_DROPPED;
      session->my_ok((uint32_t) deleted);
      session->server_status&= ~SERVER_STATUS_DB_DROPPED;
    }
    else
    {
      char *query, *query_pos, *query_end, *query_data_start;

      if (!(query= (char*) session->alloc(MAX_DROP_TABLE_Q_LEN)))
        goto exit; /* not much else we can do */
      query_pos= query_data_start= strcpy(query,"drop table ")+11;
      query_end= query + MAX_DROP_TABLE_Q_LEN;

      TransactionServices &transaction_services= TransactionServices::singleton();
      for (TableIdentifiers::iterator it= dropped_tables.begin();
           it != dropped_tables.end();
           it++)
      {
        uint32_t tbl_name_len;

        /* 3 for the quotes and the comma*/
        tbl_name_len= (*it).getTableName().length() + 3;
        if (query_pos + tbl_name_len + 1 >= query_end)
        {
          /* These DDL methods and logging protected with LOCK_create_db */
          transaction_services.rawStatement(session, query);
          query_pos= query_data_start;
        }

        *query_pos++ = '`';
        query_pos= strcpy(query_pos, (*it).getTableName().c_str()) + (tbl_name_len-3);
        *query_pos++ = '`';
        *query_pos++ = ',';
      }

      if (query_pos != query_data_start)
      {
        /* These DDL methods and logging protected with LOCK_create_db */
        transaction_services.rawStatement(session, query);
      }
    }

exit:
    /*
      If this database was the client's selected database, we silently
      change the client's selected database to nothing (to have an empty
      SELECT DATABASE() in the future). For this we free() session->db and set
      it to 0.
    */
    if (schema_identifier.compare(session->db))
      mysql_change_db_impl(session);
  }

  start_waiting_global_read_lock(session);

  return error;
}


static int rm_table_part2(Session *session, TableList *tables)
{
  TransactionServices &transaction_services= TransactionServices::singleton();

  TableList *table;
  String wrong_tables;
  int error= 0;
  bool foreign_key_error= false;

  LOCK_open.lock(); /* Part 2 of rm a table */

  if (lock_table_names_exclusively(session, tables))
  {
    LOCK_open.unlock();
    return 1;
  }

  /* Don't give warnings for not found errors, as we already generate notes */
  session->no_warnings_for_error= 1;

  for (table= tables; table; table= table->next_local)
  {
    char *db=table->db;
    TableIdentifier identifier(table->db, table->table_name);

    plugin::StorageEngine *table_type;

    error= session->drop_temporary_table(identifier);

    switch (error) {
    case  0:
      // removed temporary table
      continue;
    case -1:
      error= 1;
      unlock_table_names(tables, NULL);
      LOCK_open.unlock();
      session->no_warnings_for_error= 0;

      return(error);
    default:
      // temporary table not found
      error= 0;
    }

    table_type= table->getDbType();

    {
      Table *locked_table;
      abort_locked_tables(session, identifier);
      remove_table_from_cache(session, identifier,
                              RTFC_WAIT_OTHER_THREAD_FLAG |
                              RTFC_CHECK_KILLED_FLAG);
      /*
        If the table was used in lock tables, remember it so that
        unlock_table_names can free it
      */
      if ((locked_table= drop_locked_tables(session, identifier)))
        table->table= locked_table;

      if (session->killed)
      {
        error= -1;
        unlock_table_names(tables, NULL);
        LOCK_open.unlock();
        session->no_warnings_for_error= 0;

        return(error);
      }
    }
    identifier.getPath();

    if (table_type == NULL && not plugin::StorageEngine::doesTableExist(*session, identifier))
    {
      // Table was not found on disk and table can't be created from engine
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
                          table->table_name);
    }
    else
    {
      error= plugin::StorageEngine::dropTable(*session, identifier);

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
    {
      transaction_services.dropTable(session, string(db), string(table->table_name), true);
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
  LOCK_open.unlock();
  error= 0;
  if (wrong_tables.length())
  {
    if (not foreign_key_error)
      my_printf_error(ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR), MYF(0),
                      wrong_tables.c_ptr());
    else
    {
      my_message(ER_ROW_IS_REFERENCED, ER(ER_ROW_IS_REFERENCED), MYF(0));
    }
    error= 1;
  }

  LOCK_open.lock(); /* final bit in rm table lock */
  unlock_table_names(tables, NULL);
  LOCK_open.unlock();
  session->no_warnings_for_error= 0;

  return(error);
}

/*
  Removes files with known extensions plus.
  session MUST be set when calling this function!
*/

static long drop_tables_via_filenames(Session *session,
                                      SchemaIdentifier &schema_identifier,
                                      TableIdentifiers &dropped_tables)
{
  long deleted= 0;
  TableList *tot_list= NULL, **tot_list_next;

  tot_list_next= &tot_list;

  plugin::StorageEngine::getIdentifiers(*session, schema_identifier, dropped_tables);

  for (TableIdentifiers::iterator it= dropped_tables.begin();
       it != dropped_tables.end();
       it++)
  {
    size_t db_len= schema_identifier.getSchemaName().size();

    /* Drop the table nicely */
    TableList *table_list=(TableList*)
      session->calloc(sizeof(*table_list) +
                      db_len + 1 +
                      (*it).getTableName().length() + 1);

    if (not table_list)
      return -1;

    table_list->db= (char*) (table_list+1);
    table_list->table_name= strcpy(table_list->db, schema_identifier.getSchemaName().c_str()) + db_len + 1;
    TableIdentifier::filename_to_tablename((*it).getTableName().c_str(), table_list->table_name, (*it).getTableName().size() + 1);
    table_list->alias= table_list->table_name;  // If lower_case_table_names=2
    table_list->setInternalTmpTable((strncmp((*it).getTableName().c_str(),
                                             TMP_FILE_PREFIX,
                                             strlen(TMP_FILE_PREFIX)) == 0));
    /* Link into list */
    (*tot_list_next)= table_list;
    tot_list_next= &table_list->next_local;
    deleted++;
  }
  if (session->killed)
    return -1;

  if (tot_list)
  {
    if (rm_table_part2(session, tot_list))
      return -1;
  }


  if (not plugin::StorageEngine::dropSchema(schema_identifier))
  {
    my_error(ER_DROP_SCHEMA, MYF(0), schema_identifier.getSQLPath().c_str());
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

bool mysql_change_db(Session *session, SchemaIdentifier &schema_identifier)
{

  if (not plugin::Authorization::isAuthorized(session->getSecurityContext(), schema_identifier))
  {
    /* Error message is set in isAuthorized */
    return true;
  }

  if (not check_db_name(session, schema_identifier))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), schema_identifier.getSQLPath().c_str());

    return true;
  }

  if (not plugin::StorageEngine::doesSchemaExist(schema_identifier))
  {
    /* Report an error and free new_db_file_name. */

    my_error(ER_BAD_DB_ERROR, MYF(0), schema_identifier.getSQLPath().c_str());

    /* The operation failed. */

    return true;
  }

  mysql_change_db_impl(session, schema_identifier);

  return false;
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

static void mysql_change_db_impl(Session *session, SchemaIdentifier &schema_identifier)
{
  /* 1. Change current database in Session. */

#if 0
  if (new_db_name == NULL)
  {
    /*
      Session::set_db() does all the job -- it frees previous database name and
      sets the new one.
    */

    session->set_db(NULL, 0);
  }
  else
#endif
  {
    /*
      Here we already have a copy of database name to be used in Session. So,
      we just call Session::reset_db(). Since Session::reset_db() does not releases
      the previous database name, we should do it explicitly.
    */

    session->set_db(schema_identifier.getSchemaName());
  }
}

static void mysql_change_db_impl(Session *session)
{
  session->set_db(string());
}

} /* namespace drizzled */
