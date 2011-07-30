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
#include <config.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <set>
#include <string>
#include <fstream>

#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/session.h>
#include <drizzled/schema.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/transaction_services.h>
#include <drizzled/message/schema.pb.h>
#include <drizzled/sql_table.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/charset.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/catalog/instance.h>
#include <boost/thread/mutex.hpp>

using namespace std;

namespace drizzled {
namespace schema {

/*
  Create a database

  SYNOPSIS
  create_db()
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

bool create(Session &session, const message::Schema &schema_message, const bool is_if_not_exists)
{
  bool error= false;

  /*
    Do not create database if another thread is holding read lock.
    Wait for global read lock before acquiring session->catalog()->schemaLock().
    After wait_if_global_read_lock() we have protection against another
    global read lock. If we would acquire session->catalog()->schemaLock() first,
    another thread could step in and get the global read lock before we
    reach wait_if_global_read_lock(). If this thread tries the same as we
    (admin a db), it would then go and wait on session->catalog()->schemaLock()...
    Furthermore wait_if_global_read_lock() checks if the current thread
    has the global read lock and refuses the operation with
    ER_CANT_UPDATE_WITH_READLOCK if applicable.
  */
  if (session.wait_if_global_read_lock(false, true))
  {
    return false;
  }

  assert(schema_message.has_name());
  assert(schema_message.has_collation());

  // @todo push this lock down into the engine
  {
    boost::mutex::scoped_lock scopedLock(session.catalog().schemaLock());

    // Check to see if it exists already.  
    identifier::Schema schema_identifier(schema_message.name());
    if (plugin::StorageEngine::doesSchemaExist(schema_identifier))
    {
      if (not is_if_not_exists)
      {
        my_error(ER_DB_CREATE_EXISTS, schema_identifier);
        error= true;
      }
      else
      {
        push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                            ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS),
                            schema_message.name().c_str());
        session.my_ok();
      }
    }
    else if (not plugin::StorageEngine::createSchema(schema_message)) // Try to create it 
    {
      my_error(ER_CANT_CREATE_DB, MYF(0), schema_message.name().c_str(), errno);
      error= true;
    }
    else // Created !
    {
      TransactionServices::createSchema(session, schema_message);
      session.my_ok(1);
    }
  }
  session.startWaitingGlobalReadLock();

  return error;
}


/* db-name is already validated when we come here */

bool alter(Session &session,
           const message::Schema &schema_message,
           const message::Schema &original_schema)
{
  /*
    Do not alter database if another thread is holding read lock.
    Wait for global read lock before acquiring session->catalog()->schemaLock().
    After wait_if_global_read_lock() we have protection against another
    global read lock. If we would acquire session->catalog()->schemaLock() first,
    another thread could step in and get the global read lock before we
    reach wait_if_global_read_lock(). If this thread tries the same as we
    (admin a db), it would then go and wait on session->catalog()->schemaLock()...
    Furthermore wait_if_global_read_lock() checks if the current thread
    has the global read lock and refuses the operation with
    ER_CANT_UPDATE_WITH_READLOCK if applicable.
  */
  if ((session.wait_if_global_read_lock(false, true)))
    return false;

  bool success;
  {
    boost::mutex::scoped_lock scopedLock(session.catalog().schemaLock());

    identifier::Schema schema_idenifier(schema_message.name());
    if (not plugin::StorageEngine::doesSchemaExist(schema_idenifier))
    {
      my_error(ER_SCHEMA_DOES_NOT_EXIST, schema_idenifier);
      return false;
    }

    /* Change options if current database is being altered. */
    success= plugin::StorageEngine::alterSchema(schema_message);

    if (success)
    {
      TransactionServices::alterSchema(session, original_schema, schema_message);
      session.my_ok(1);
    }
    else
    {
      my_error(ER_ALTER_SCHEMA, schema_idenifier);
    }
  }
  session.startWaitingGlobalReadLock();

  return success;
}


/*
  Drop all tables in a database and the database itself

  SYNOPSIS
    rm_db()
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

bool drop(Session &session, const identifier::Schema &schema_identifier, bool if_exists)
{
  /*
    Do not drop database if another thread is holding read lock.
    Wait for global read lock before acquiring session->catalog()->schemaLock().
    After wait_if_global_read_lock() we have protection against another
    global read lock. If we would acquire session->catalog()->schemaLock() first,
    another thread could step in and get the global read lock before we
    reach wait_if_global_read_lock(). If this thread tries the same as we
    (admin a db), it would then go and wait on session->catalog()->schemaLock()...
    Furthermore wait_if_global_read_lock() checks if the current thread
    has the global read lock and refuses the operation with
    ER_CANT_UPDATE_WITH_READLOCK if applicable.
  */
  if (session.wait_if_global_read_lock(false, true))
  {
    return true;
  }

  bool error= false;
  {
    boost::mutex::scoped_lock scopedLock(session.catalog().schemaLock());
    if (message::schema::shared_ptr message= plugin::StorageEngine::getSchemaDefinition(schema_identifier))
    {
      error= plugin::StorageEngine::dropSchema(session, schema_identifier, *message);
    }
    else if (if_exists)
    {
      push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_NOTE, ER_DB_DROP_EXISTS, ER(ER_DB_DROP_EXISTS),
                          schema_identifier.getSQLPath().c_str());
    }
    else
    {
      error= true;
      my_error(ER_DB_DROP_EXISTS, schema_identifier);
    }
  };

  /*
    If this database was the client's selected database, we silently
    change the client's selected database to nothing (to have an empty
    SELECT DATABASE() in the future). For this we free() session->db and set
    it to 0.
  */
  if (not error and schema_identifier.compare(*session.schema()))
    session.set_db("");

  session.startWaitingGlobalReadLock();

  return error;
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

bool change(Session &session, const identifier::Schema &schema_identifier)
{

  if (not plugin::Authorization::isAuthorized(*session.user(), schema_identifier))
  {
    /* Error message is set in isAuthorized */
    return true;
  }

  if (not check(session, schema_identifier))
  {
    my_error(ER_WRONG_DB_NAME, schema_identifier);

    return true;
  }

  if (not plugin::StorageEngine::doesSchemaExist(schema_identifier))
  {
    my_error(ER_BAD_DB_ERROR, schema_identifier);

    /* The operation failed. */

    return true;
  }

  session.set_db(schema_identifier.getSchemaName());

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


/*
  Check if database name is valid

  SYNPOSIS
    check()
    org_name		Name of database and length

  RETURN
    false error
    true ok
*/

bool check(Session &session, const identifier::Schema &schema_identifier)
{
  if (not plugin::Authorization::isAuthorized(*session.user(), schema_identifier))
    return false;
  return schema_identifier.isValid();
}

} /* namespace schema */

} /* namespace drizzled */
