/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (c) 2010 Jay Pipes <jaypipes@gmail.com>
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

/**
 * @file Transaction processing code
 */

#include "config.h"
#include "drizzled/my_hash.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"
#include "drizzled/probes.h"
#include "drizzled/sql_parse.h"
#include "drizzled/session.h"
#include "drizzled/sql_base.h"
#include "drizzled/replication_services.h"
#include "drizzled/transaction_services.h"
#include "drizzled/transaction_context.h"
#include "drizzled/resource_context.h"
#include "drizzled/lock.h"
#include "drizzled/item/int.h"
#include "drizzled/item/empty_string.h"
#include "drizzled/field/timestamp.h"
#include "drizzled/plugin/client.h"
#include "drizzled/plugin/monitored_in_transaction.h"
#include "drizzled/plugin/transactional_storage_engine.h"
#include "drizzled/plugin/xa_resource_manager.h"
#include "drizzled/internal/my_sys.h"

using namespace std;

#include <vector>
#include <algorithm>
#include <functional>

namespace drizzled
{

/**
 * @defgroup Transactions
 *
 * @brief
 *
 * Transaction handling in the server
 *
 * @detail
 *
 * In each client connection, Drizzle maintains two transaction
 * contexts representing the state of the:
 *
 * 1) Statement Transaction
 * 2) Normal Transaction
 *
 * These two transaction contexts represent the transactional
 * state of a Session's SQL and XA transactions for a single
 * SQL statement or a series of SQL statements.
 *
 * When the Session's connection is in AUTOCOMMIT mode, there
 * is no practical difference between the statement and the
 * normal transaction, as each SQL statement is committed or
 * rolled back depending on the success or failure of the
 * indvidual SQL statement.
 *
 * When the Session's connection is NOT in AUTOCOMMIT mode, OR
 * the Session has explicitly begun a normal SQL transaction using
 * a BEGIN WORK/START TRANSACTION statement, then the normal
 * transaction context tracks the aggregate transaction state of
 * the SQL transaction's individual statements, and the SQL
 * transaction's commit or rollback is done atomically for all of
 * the SQL transaction's statement's data changes.
 *
 * Technically, a statement transaction can be viewed as a savepoint 
 * which is maintained automatically in order to make effects of one
 * statement atomic.
 *
 * The normal transaction is started by the user and is typically
 * ended (COMMIT or ROLLBACK) upon an explicity user request as well.
 * The exception to this is that DDL statements implicitly COMMIT
 * any previously active normal transaction before they begin executing.
 *
 * In Drizzle, unlike MySQL, plugins other than a storage engine
 * may participate in a transaction.  All plugin::TransactionalStorageEngine
 * plugins will automatically be monitored by Drizzle's transaction 
 * manager (implemented in this source file), as will all plugins which
 * implement plugin::XaResourceManager and register with the transaction
 * manager.
 *
 * If Drizzle's transaction manager sees that more than one resource
 * manager (transactional storage engine or XA resource manager) has modified
 * data state during a statement or normal transaction, the transaction
 * manager will automatically use a two-phase commit protocol for all
 * resources which support XA's distributed transaction protocol.  Unlike
 * MySQL, storage engines need not manually register with the transaction
 * manager during a statement's execution.  Previously, in MySQL, all
 * handlertons would have to call trans_register_ha() at some point after
 * modifying data state in order to have MySQL include that handler in
 * an XA transaction.  Drizzle does all of this grunt work behind the
 * scenes for the storage engine implementers.
 *
 * When a connection is closed, the current normal transaction, if
 * any is currently active, is rolled back.
 *
 * Transaction life cycle
 * ----------------------
 *
 * When a new connection is established, session->transaction
 * members are initialized to an empty state. If a statement uses any tables, 
 * all affected engines are registered in the statement engine list automatically
 * in plugin::StorageEngine::startStatement() and 
 * plugin::TransactionalStorageEngine::startTransaction().
 *
 * You can view the lifetime of a normal transaction in the following
 * call-sequence:
 *
 * drizzled::statement::Statement::execute()
 *   drizzled::plugin::TransactionalStorageEngine::startTransaction()
 *     drizzled::TransactionServices::registerResourceForTransaction()
 *     drizzled::TransactionServices::registerResourceForStatement()
 *     drizzled::plugin::StorageEngine::startStatement()
 *       drizzled::Cursor::write_row() <-- example...could be update_row(), etc
 *     drizzled::plugin::StorageEngine::endStatement()
 *   drizzled::TransactionServices::autocommitOrRollback()
 *     drizzled::TransactionalStorageEngine::commit() <-- or ::rollback()
 *     drizzled::XaResourceManager::xaCommit() <-- or rollback()
 *
 * Roles and responsibilities
 * --------------------------
 *
 * Beginning of SQL Statement (and Statement Transaction)
 * ------------------------------------------------------
 *
 * At the start of each SQL statement, for each storage engine
 * <strong>that is involved in the SQL statement</strong>, the kernel 
 * calls the engine's plugin::StoragEngine::startStatement() method.  If the
 * engine needs to track some data for the statement, it should use
 * this method invocation to initialize this data.  This is the
 * beginning of what is called the "statement transaction".
 *
 * <strong>For transaction storage engines (those storage engines
 * that inherit from plugin::TransactionalStorageEngine)</strong>, the
 * kernel automatically determines if the start of the SQL statement 
 * transaction should <em>also</em> begin the normal SQL transaction.
 * This occurs when the connection is in NOT in autocommit mode. If
 * the kernel detects this, then the kernel automatically starts the
 * normal transaction w/ plugin::TransactionalStorageEngine::startTransaction()
 * method and then calls plugin::StorageEngine::startStatement()
 * afterwards.
 *
 * Beginning of an SQL "Normal" Transaction
 * ----------------------------------------
 *
 * As noted above, a "normal SQL transaction" may be started when
 * an SQL statement is started in a connection and the connection is
 * NOT in AUTOCOMMIT mode.  This is automatically done by the kernel.
 *
 * In addition, when a user executes a START TRANSACTION or
 * BEGIN WORK statement in a connection, the kernel explicitly
 * calls each transactional storage engine's startTransaction() method.
 *
 * Ending of an SQL Statement (and Statement Transaction)
 * ------------------------------------------------------
 *
 * At the end of each SQL statement, for each of the aforementioned
 * involved storage engines, the kernel calls the engine's
 * plugin::StorageEngine::endStatement() method.  If the engine
 * has initialized or modified some internal data about the
 * statement transaction, it should use this method to reset or destroy
 * this data appropriately.
 *
 * Ending of an SQL "Normal" Transaction
 * -------------------------------------
 *
 * The end of a normal transaction is either a ROLLBACK or a COMMIT, 
 * depending on the success or failure of the statement transaction(s) 
 * it encloses.
 *
 * The end of a "normal transaction" occurs when any of the following
 * occurs:
 *
 * 1) If a statement transaction has completed and AUTOCOMMIT is ON,
 *    then the normal transaction which encloses the statement
 *    transaction ends
 * 2) If a COMMIT or ROLLBACK statement occurs on the connection
 * 3) Just before a DDL operation occurs, the kernel will implicitly
 *    commit the active normal transaction
 *
 * Transactions and Non-transactional Storage Engines
 * --------------------------------------------------
 *
 * For non-transactional engines, this call can be safely ignored, an
 * the kernel tracks whether a non-transactional engine has changed
 * any data state, and warns the user appropriately if a transaction
 * (statement or normal) is rolled back after such non-transactional
 * data changes have been made.
 *
 * XA Two-phase Commit Protocol
 * ----------------------------
 *
 * During statement execution, whenever any of data-modifying
 * PSEA API methods is used, e.g. Cursor::write_row() or
 * Cursor::update_row(), the read-write flag is raised in the
 * statement transaction for the involved engine.
 * Currently All PSEA calls are "traced", and the data can not be
 * changed in a way other than issuing a PSEA call. Important:
 * unless this invariant is preserved the server will not know that
 * a transaction in a given engine is read-write and will not
 * involve the two-phase commit protocol!
 *
 * At the end of a statement, TransactionServices::autocommitOrRollback()
 * is invoked. This call in turn
 * invokes plugin::XaResourceManager::xapPepare() for every involved XA
 * resource manager.
 *
 * Prepare is followed by a call to plugin::TransactionalStorageEngine::commit()
 * or plugin::XaResourceManager::xaCommit() (depending on what the resource
 * is...)
 * 
 * If a one-phase commit will suffice, plugin::StorageEngine::prepare() is not
 * invoked and the server only calls plugin::StorageEngine::commit_one_phase().
 * At statement commit, the statement-related read-write engine
 * flag is propagated to the corresponding flag in the normal
 * transaction.  When the commit is complete, the list of registered
 * engines is cleared.
 *
 * Rollback is handled in a similar fashion.
 *
 * Additional notes on DDL and the normal transaction.
 * ---------------------------------------------------
 *
 * CREATE TABLE .. SELECT can start a *new* normal transaction
 * because of the fact that SELECTs on a transactional storage
 * engine participate in the normal SQL transaction (due to
 * isolation level issues and consistent read views).
 *
 * Behaviour of the server in this case is currently badly
 * defined.
 *
 * DDL statements use a form of "semantic" logging
 * to maintain atomicity: if CREATE TABLE .. SELECT failed,
 * the newly created table is deleted.
 * 
 * In addition, some DDL statements issue interim transaction
 * commits: e.g. ALTER TABLE issues a COMMIT after data is copied
 * from the original table to the internal temporary table. Other
 * statements, e.g. CREATE TABLE ... SELECT do not always commit
 * after itself.
 *
 * And finally there is a group of DDL statements such as
 * RENAME/DROP TABLE that doesn't start a new transaction
 * and doesn't commit.
 *
 * A consistent behaviour is perhaps to always commit the normal
 * transaction after all DDLs, just like the statement transaction
 * is always committed at the end of all statements.
 */
void TransactionServices::registerResourceForStatement(Session *session,
                                                       plugin::MonitoredInTransaction *monitored,
                                                       plugin::TransactionalStorageEngine *engine)
{
  if (session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    /* 
     * Now we automatically register this resource manager for the
     * normal transaction.  This is fine because a statement
     * transaction registration should always enlist the resource
     * in the normal transaction which contains the statement
     * transaction.
     */
    registerResourceForTransaction(session, monitored, engine);
  }

  TransactionContext *trans= &session->transaction.stmt;
  ResourceContext *resource_context= session->getResourceContext(monitored, 0);

  if (resource_context->isStarted())
    return; /* already registered, return */

  assert(monitored->participatesInSqlTransaction());
  assert(not monitored->participatesInXaTransaction());

  resource_context->setMonitored(monitored);
  resource_context->setTransactionalStorageEngine(engine);
  trans->registerResource(resource_context);

  trans->no_2pc|= true;
}

void TransactionServices::registerResourceForStatement(Session *session,
                                                       plugin::MonitoredInTransaction *monitored,
                                                       plugin::TransactionalStorageEngine *engine,
                                                       plugin::XaResourceManager *resource_manager)
{
  if (session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    /* 
     * Now we automatically register this resource manager for the
     * normal transaction.  This is fine because a statement
     * transaction registration should always enlist the resource
     * in the normal transaction which contains the statement
     * transaction.
     */
    registerResourceForTransaction(session, monitored, engine, resource_manager);
  }

  TransactionContext *trans= &session->transaction.stmt;
  ResourceContext *resource_context= session->getResourceContext(monitored, 0);

  if (resource_context->isStarted())
    return; /* already registered, return */

  assert(monitored->participatesInXaTransaction());
  assert(monitored->participatesInSqlTransaction());

  resource_context->setMonitored(monitored);
  resource_context->setTransactionalStorageEngine(engine);
  resource_context->setXaResourceManager(resource_manager);
  trans->registerResource(resource_context);

  trans->no_2pc|= false;
}

void TransactionServices::registerResourceForTransaction(Session *session,
                                                         plugin::MonitoredInTransaction *monitored,
                                                         plugin::TransactionalStorageEngine *engine)
{
  TransactionContext *trans= &session->transaction.all;
  ResourceContext *resource_context= session->getResourceContext(monitored, 1);

  if (resource_context->isStarted())
    return; /* already registered, return */

  session->server_status|= SERVER_STATUS_IN_TRANS;

  trans->registerResource(resource_context);

  assert(monitored->participatesInSqlTransaction());
  assert(not monitored->participatesInXaTransaction());

  resource_context->setMonitored(monitored);
  resource_context->setTransactionalStorageEngine(engine);
  trans->no_2pc|= true;

  if (session->transaction.xid_state.xid.is_null())
    session->transaction.xid_state.xid.set(session->getQueryId());

  engine->startTransaction(session, START_TRANS_NO_OPTIONS);

  /* Only true if user is executing a BEGIN WORK/START TRANSACTION */
  if (! session->getResourceContext(monitored, 0)->isStarted())
    registerResourceForStatement(session, monitored, engine);
}

void TransactionServices::registerResourceForTransaction(Session *session,
                                                         plugin::MonitoredInTransaction *monitored,
                                                         plugin::TransactionalStorageEngine *engine,
                                                         plugin::XaResourceManager *resource_manager)
{
  TransactionContext *trans= &session->transaction.all;
  ResourceContext *resource_context= session->getResourceContext(monitored, 1);

  if (resource_context->isStarted())
    return; /* already registered, return */

  session->server_status|= SERVER_STATUS_IN_TRANS;

  trans->registerResource(resource_context);

  assert(monitored->participatesInSqlTransaction());

  resource_context->setMonitored(monitored);
  resource_context->setXaResourceManager(resource_manager);
  resource_context->setTransactionalStorageEngine(engine);
  trans->no_2pc|= true;

  if (session->transaction.xid_state.xid.is_null())
    session->transaction.xid_state.xid.set(session->getQueryId());

  engine->startTransaction(session, START_TRANS_NO_OPTIONS);

  /* Only true if user is executing a BEGIN WORK/START TRANSACTION */
  if (! session->getResourceContext(monitored, 0)->isStarted())
    registerResourceForStatement(session, monitored, engine, resource_manager);
}

/**
  Check if we can skip the two-phase commit.

  A helper function to evaluate if two-phase commit is mandatory.
  As a side effect, propagates the read-only/read-write flags
  of the statement transaction to its enclosing normal transaction.

  @retval true   we must run a two-phase commit. Returned
                 if we have at least two engines with read-write changes.
  @retval false  Don't need two-phase commit. Even if we have two
                 transactional engines, we can run two independent
                 commits if changes in one of the engines are read-only.
*/
static
bool
ha_check_and_coalesce_trx_read_only(Session *session,
                                    TransactionContext::ResourceContexts &resource_contexts,
                                    bool normal_transaction)
{
  /* The number of storage engines that have actual changes. */
  unsigned num_resources_modified_data= 0;
  ResourceContext *resource_context;

  for (TransactionContext::ResourceContexts::iterator it= resource_contexts.begin();
       it != resource_contexts.end();
       ++it)
  {
    resource_context= *it;
    if (resource_context->hasModifiedData())
      ++num_resources_modified_data;

    if (! normal_transaction)
    {
      ResourceContext *resource_context_normal= session->getResourceContext(resource_context->getMonitored(), true);
      assert(resource_context != resource_context_normal);
      /*
        Merge read-only/read-write information about statement
        transaction to its enclosing normal transaction. Do this
        only if in a real transaction -- that is, if we know
        that resource_context_all is registered in session->transaction.all.
        Since otherwise we only clutter the normal transaction flags.
      */
      if (resource_context_normal->isStarted()) /* false if autocommit. */
        resource_context_normal->coalesceWith(resource_context);
    }
    else if (num_resources_modified_data > 1)
    {
      /*
        It is a normal transaction, so we don't need to merge read/write
        information up, and the need for two-phase commit has been
        already established. Break the loop prematurely.
      */
      break;
    }
  }
  return num_resources_modified_data > 1;
}


/**
  @retval
    0   ok
  @retval
    1   transaction was rolled back
  @retval
    2   error during commit, data may be inconsistent

  @todo
    Since we don't support nested statement transactions in 5.0,
    we can't commit or rollback stmt transactions while we are inside
    stored functions or triggers. So we simply do nothing now.
    TODO: This should be fixed in later ( >= 5.1) releases.
*/
int TransactionServices::ha_commit_trans(Session *session, bool normal_transaction)
{
  int error= 0, cookie= 0;
  /*
    'all' means that this is either an explicit commit issued by
    user, or an implicit commit issued by a DDL.
  */
  TransactionContext *trans= normal_transaction ? &session->transaction.all : &session->transaction.stmt;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  bool is_real_trans= normal_transaction || session->transaction.all.getResourceContexts().empty();

  /*
    We must not commit the normal transaction if a statement
    transaction is pending. Otherwise statement transaction
    flags will not get propagated to its normal transaction's
    counterpart.
  */
  assert(session->transaction.stmt.getResourceContexts().empty() ||
              trans == &session->transaction.stmt);

  if (resource_contexts.empty() == false)
  {
    bool must_2pc;

    if (is_real_trans && wait_if_global_read_lock(session, 0, 0))
    {
      ha_rollback_trans(session, normal_transaction);
      return 1;
    }

    must_2pc= ha_check_and_coalesce_trx_read_only(session, resource_contexts, normal_transaction);

    if (! trans->no_2pc && must_2pc)
    {
      for (TransactionContext::ResourceContexts::iterator it= resource_contexts.begin();
           it != resource_contexts.end() && ! error;
           ++it)
      {
        ResourceContext *resource_context= *it;
        int err;
        /*
          Do not call two-phase commit if this particular
          transaction is read-only. This allows for simpler
          implementation in engines that are always read-only.
        */
        if (! resource_context->hasModifiedData())
          continue;

        plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

        if (resource->participatesInXaTransaction())
        {
          if ((err= resource_context->getXaResourceManager()->xaPrepare(session, normal_transaction)))
          {
            my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
            error= 1;
          }
          else
          {
            status_var_increment(session->status_var.ha_prepare_count);
          }
        }
      }
      if (error)
      {
        ha_rollback_trans(session, normal_transaction);
        error= 1;
        goto end;
      }
    }
    error= ha_commit_one_phase(session, normal_transaction) ? (cookie ? 2 : 1) : 0;
end:
    if (is_real_trans)
      start_waiting_global_read_lock(session);
  }
  return error;
}

/**
  @note
  This function does not care about global read lock. A caller should.
*/
int TransactionServices::ha_commit_one_phase(Session *session, bool normal_transaction)
{
  int error=0;
  TransactionContext *trans= normal_transaction ? &session->transaction.all : &session->transaction.stmt;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  bool is_real_trans= normal_transaction || session->transaction.all.getResourceContexts().empty();

  if (resource_contexts.empty() == false)
  {
    for (TransactionContext::ResourceContexts::iterator it= resource_contexts.begin();
         it != resource_contexts.end();
         ++it)
    {
      int err;
      ResourceContext *resource_context= *it;

      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInXaTransaction())
      {
        if ((err= resource_context->getXaResourceManager()->xaCommit(session, normal_transaction)))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          status_var_increment(session->status_var.ha_commit_count);
        }
      }
      else if (resource->participatesInSqlTransaction())
      {
        if ((err= resource_context->getTransactionalStorageEngine()->commit(session, normal_transaction)))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          status_var_increment(session->status_var.ha_commit_count);
        }
      }
      resource_context->reset(); /* keep it conveniently zero-filled */
    }

    if (is_real_trans)
      session->transaction.xid_state.xid.null();

    if (normal_transaction)
    {
      session->variables.tx_isolation= session->session_tx_isolation;
      session->transaction.cleanup();
    }
  }
  trans->reset();
  if (error == 0)
  {
    if (is_real_trans)
    {
      /* 
       * We commit the normal transaction by finalizing the transaction message
       * and propogating the message to all registered replicators.
       */
      ReplicationServices &replication_services= ReplicationServices::singleton();
      replication_services.commitTransaction(session);
    }
  }
  return error;
}

int TransactionServices::ha_rollback_trans(Session *session, bool normal_transaction)
{
  int error= 0;
  TransactionContext *trans= normal_transaction ? &session->transaction.all : &session->transaction.stmt;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  bool is_real_trans= normal_transaction || session->transaction.all.getResourceContexts().empty();

  /*
    We must not rollback the normal transaction if a statement
    transaction is pending.
  */
  assert(session->transaction.stmt.getResourceContexts().empty() ||
              trans == &session->transaction.stmt);

  if (resource_contexts.empty() == false)
  {
    for (TransactionContext::ResourceContexts::iterator it= resource_contexts.begin();
         it != resource_contexts.end();
         ++it)
    {
      int err;
      ResourceContext *resource_context= *it;

      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInXaTransaction())
      {
        if ((err= resource_context->getXaResourceManager()->xaRollback(session, normal_transaction)))
        {
          my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          status_var_increment(session->status_var.ha_rollback_count);
        }
      }
      else if (resource->participatesInSqlTransaction())
      {
        if ((err= resource_context->getTransactionalStorageEngine()->rollback(session, normal_transaction)))
        {
          my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          status_var_increment(session->status_var.ha_rollback_count);
        }
      }
      resource_context->reset(); /* keep it conveniently zero-filled */
    }
    
    /* 
     * We need to signal the ROLLBACK to ReplicationServices here
     * BEFORE we set the transaction ID to NULL.  This is because
     * if a bulk segment was sent to replicators, we need to send
     * a rollback statement with the corresponding transaction ID
     * to rollback.
     */
    ReplicationServices &replication_services= ReplicationServices::singleton();
    replication_services.rollbackTransaction(session);

    if (is_real_trans)
      session->transaction.xid_state.xid.null();
    if (normal_transaction)
    {
      session->variables.tx_isolation=session->session_tx_isolation;
      session->transaction.cleanup();
    }
  }
  if (normal_transaction)
    session->transaction_rollback_request= false;

  /*
   * If a non-transactional table was updated, warn the user
   */
  if (is_real_trans &&
      session->transaction.all.hasModifiedNonTransData() &&
      session->killed != Session::KILL_CONNECTION)
  {
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
  }
  trans->reset();
  return error;
}

/**
  This is used to commit or rollback a single statement depending on
  the value of error.

  @note
    Note that if the autocommit is on, then the following call inside
    InnoDB will commit or rollback the whole transaction (= the statement). The
    autocommit mechanism built into InnoDB is based on counting locks, but if
    the user has used LOCK TABLES then that mechanism does not know to do the
    commit.
*/
int TransactionServices::ha_autocommit_or_rollback(Session *session, int error)
{
  if (session->transaction.stmt.getResourceContexts().empty() == false)
  {
    if (! error)
    {
      if (ha_commit_trans(session, false))
        error= 1;
    }
    else
    {
      (void) ha_rollback_trans(session, false);
      if (session->transaction_rollback_request)
        (void) ha_rollback_trans(session, true);
    }

    session->variables.tx_isolation= session->session_tx_isolation;
  }
  return error;
}

/**
  return the list of XID's to a client, the same way SHOW commands do.

  @note
    I didn't find in XA specs that an RM cannot return the same XID twice,
    so mysql_xa_recover does not filter XID's to ensure uniqueness.
    It can be easily fixed later, if necessary.
*/
bool TransactionServices::mysql_xa_recover(Session *session)
{
  List<Item> field_list;
  int i= 0;
  XID_STATE *xs;

  field_list.push_back(new Item_int("formatID", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int("gtrid_length", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int("bqual_length", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("data", DRIZZLE_XIDDATASIZE));

  if (session->client->sendFields(&field_list))
    return 1;

  pthread_mutex_lock(&LOCK_xid_cache);
  while ((xs= (XID_STATE*)hash_element(&xid_cache, i++)))
  {
    if (xs->xa_state==XA_PREPARED)
    {
      session->client->store((int64_t)xs->xid.formatID);
      session->client->store((int64_t)xs->xid.gtrid_length);
      session->client->store((int64_t)xs->xid.bqual_length);
      session->client->store(xs->xid.data,
                             xs->xid.gtrid_length+xs->xid.bqual_length);
      if (session->client->flush())
      {
        pthread_mutex_unlock(&LOCK_xid_cache);
        return 1;
      }
    }
  }

  pthread_mutex_unlock(&LOCK_xid_cache);
  session->my_eof();
  return 0;
}

struct ResourceContextCompare : public std::binary_function<ResourceContext *, ResourceContext *, bool>
{
  result_type operator()(const ResourceContext *lhs, const ResourceContext *rhs) const
  {
    /* The below is perfectly fine, since we're simply comparing addresses for the underlying
     * resources aren't the same... */
    return reinterpret_cast<uint64_t>(lhs->getMonitored()) < reinterpret_cast<uint64_t>(rhs->getMonitored());
  }
};

int TransactionServices::ha_rollback_to_savepoint(Session *session, NamedSavepoint &sv)
{
  int error= 0;
  TransactionContext *trans= &session->transaction.all;
  TransactionContext::ResourceContexts &tran_resource_contexts= trans->getResourceContexts();
  TransactionContext::ResourceContexts &sv_resource_contexts= sv.getResourceContexts();

  trans->no_2pc= false;
  /*
    rolling back to savepoint in all storage engines that were part of the
    transaction when the savepoint was set
  */
  for (TransactionContext::ResourceContexts::iterator it= sv_resource_contexts.begin();
       it != sv_resource_contexts.end();
       ++it)
  {
    int err;
    ResourceContext *resource_context= *it;

    plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

    if (resource->participatesInSqlTransaction())
    {
      if ((err= resource_context->getTransactionalStorageEngine()->rollbackToSavepoint(session, sv)))
      {
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error= 1;
      }
      else
      {
        status_var_increment(session->status_var.ha_savepoint_rollback_count);
      }
    }
    trans->no_2pc|= not resource->participatesInXaTransaction();
  }
  /*
    rolling back the transaction in all storage engines that were not part of
    the transaction when the savepoint was set
  */
  {
    TransactionContext::ResourceContexts sorted_tran_resource_contexts(tran_resource_contexts);
    TransactionContext::ResourceContexts sorted_sv_resource_contexts(sv_resource_contexts);
    TransactionContext::ResourceContexts set_difference_contexts;

    sort(sorted_tran_resource_contexts.begin(),
         sorted_tran_resource_contexts.end(),
         ResourceContextCompare());
    sort(sorted_sv_resource_contexts.begin(),
         sorted_sv_resource_contexts.end(),
         ResourceContextCompare());
    set_difference(sorted_tran_resource_contexts.begin(),
                   sorted_tran_resource_contexts.end(),
                   sorted_sv_resource_contexts.begin(),
                   sorted_sv_resource_contexts.end(),
                   set_difference_contexts.begin(),
                   ResourceContextCompare());
    /* 
     * set_difference_contexts now contains all resource contexts
     * which are in the transaction context but were NOT in the
     * savepoint's resource contexts.
     */
        
    for (TransactionContext::ResourceContexts::iterator it= set_difference_contexts.begin();
         it != set_difference_contexts.end();
         ++it)
    {
      ResourceContext *resource_context= *it;
      int err;

      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInSqlTransaction())
      {
        if ((err= resource_context->getTransactionalStorageEngine()->rollback(session, !(0))))
        {
          my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
          error= 1;
        }
        else
        {
          status_var_increment(session->status_var.ha_rollback_count);
        }
      }
      resource_context->reset(); /* keep it conveniently zero-filled */
    }
  }
  trans->setResourceContexts(sv_resource_contexts);
  return error;
}

/**
  @note
  according to the sql standard (ISO/IEC 9075-2:2003)
  section "4.33.4 SQL-statements and transaction states",
  NamedSavepoint is *not* transaction-initiating SQL-statement
*/
int TransactionServices::ha_savepoint(Session *session, NamedSavepoint &sv)
{
  int error= 0;
  TransactionContext *trans= &session->transaction.all;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  if (resource_contexts.empty() == false)
  {
    for (TransactionContext::ResourceContexts::iterator it= resource_contexts.begin();
         it != resource_contexts.end();
         ++it)
    {
      ResourceContext *resource_context= *it;
      int err;

      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInSqlTransaction())
      {
        if ((err= resource_context->getTransactionalStorageEngine()->setSavepoint(session, sv)))
        {
          my_error(ER_GET_ERRNO, MYF(0), err);
          error= 1;
        }
        else
        {
          status_var_increment(session->status_var.ha_savepoint_count);
        }
      }
    }
  }
  /*
    Remember the list of registered storage engines.
  */
  sv.setResourceContexts(resource_contexts);
  return error;
}

int TransactionServices::ha_release_savepoint(Session *session, NamedSavepoint &sv)
{
  int error= 0;

  TransactionContext::ResourceContexts &resource_contexts= sv.getResourceContexts();

  for (TransactionContext::ResourceContexts::iterator it= resource_contexts.begin();
       it != resource_contexts.end();
       ++it)
  {
    int err;
    ResourceContext *resource_context= *it;

    plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

    if (resource->participatesInSqlTransaction())
    {
      if ((err= resource_context->getTransactionalStorageEngine()->releaseSavepoint(session, sv)))
      {
        my_error(ER_GET_ERRNO, MYF(0), err);
        error= 1;
      }
    }
  }
  return error;
}

} /* namespace drizzled */
