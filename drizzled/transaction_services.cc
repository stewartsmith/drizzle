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
 *
 * @note
 *
 * The TransactionServices component takes internal events (for instance the start of a 
 * transaction, the changing of a record, or the rollback of a transaction) 
 * and constructs GPB Messages that are passed to the ReplicationServices
 * component and used during replication.
 *
 * The reason for this functionality is to encapsulate all communication
 * between the kernel and the replicator/applier plugins into GPB Messages.
 * Instead of the plugin having to understand the (often fluidly changing)
 * mechanics of the kernel, all the plugin needs to understand is the message
 * format, and GPB messages provide a nice, clear, and versioned format for 
 * these messages.
 *
 * @see /drizzled/message/transaction.proto
 *
 * @todo
 *
 * We really should store the raw bytes in the messages, not the
 * String value of the Field.  But, to do that, the
 * statement_transform library needs first to be updated
 * to include the transformation code to convert raw
 * Drizzle-internal Field byte representation into something
 * plugins can understand.
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
#include "drizzled/message/transaction.pb.h"
#include "drizzled/message/statement_transform.h"
#include "drizzled/resource_context.h"
#include "drizzled/lock.h"
#include "drizzled/item/int.h"
#include "drizzled/item/empty_string.h"
#include "drizzled/field/timestamp.h"
#include "drizzled/plugin/client.h"
#include "drizzled/plugin/monitored_in_transaction.h"
#include "drizzled/plugin/transactional_storage_engine.h"
#include "drizzled/plugin/xa_resource_manager.h"
#include "drizzled/plugin/xa_storage_engine.h"
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
TransactionServices::TransactionServices()
{
  plugin::StorageEngine *engine= plugin::StorageEngine::findByName("InnoDB");
  if (engine)
  {
    xa_storage_engine= (plugin::XaStorageEngine*)engine; 
  }
  else 
  {
    xa_storage_engine= NULL;
  }
}

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

void TransactionServices::allocateNewTransactionId()
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
  {
    return;
  }

  Session *my_session= current_session;
  uint64_t xa_id= xa_storage_engine->getNewTransactionId(my_session);
  my_session->setXaId(xa_id);
}

uint64_t TransactionServices::getCurrentTransactionId(Session *session)
{
  if (session->getXaId() == 0)
  {
    session->setXaId(xa_storage_engine->getNewTransactionId(session)); 
  }

  return session->getXaId();
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
int TransactionServices::commitTransaction(Session *session, bool normal_transaction)
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
    if (is_real_trans && wait_if_global_read_lock(session, 0, 0))
    {
      rollbackTransaction(session, normal_transaction);
      return 1;
    }

    /*
     * If replication is on, we do a PREPARE on the resource managers, push the
     * Transaction message across the replication stream, and then COMMIT if the
     * replication stream returned successfully.
     */
    if (shouldConstructMessages())
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
            session->status_var.ha_prepare_count++;
          }
        }
      }
      if (error == 0 && is_real_trans)
      {
        /*
         * Push the constructed Transaction messages across to
         * replicators and appliers.
         */
        error= commitTransactionMessage(session);
      }
      if (error)
      {
        rollbackTransaction(session, normal_transaction);
        error= 1;
        goto end;
      }
    }
    error= commitPhaseOne(session, normal_transaction) ? (cookie ? 2 : 1) : 0;
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
int TransactionServices::commitPhaseOne(Session *session, bool normal_transaction)
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
          session->status_var.ha_commit_count++;
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
          session->status_var.ha_commit_count++;
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
  return error;
}

int TransactionServices::rollbackTransaction(Session *session, bool normal_transaction)
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
          session->status_var.ha_rollback_count++;
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
          session->status_var.ha_rollback_count++;
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
    if (normal_transaction)
      rollbackTransactionMessage(session);

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
int TransactionServices::autocommitOrRollback(Session *session, int error)
{
  if (session->transaction.stmt.getResourceContexts().empty() == false)
  {
    if (! error)
    {
      if (commitTransaction(session, false))
        error= 1;
    }
    else
    {
      (void) rollbackTransaction(session, false);
      if (session->transaction_rollback_request)
        (void) rollbackTransaction(session, true);
    }

    session->variables.tx_isolation= session->session_tx_isolation;
  }
  return error;
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

int TransactionServices::rollbackToSavepoint(Session *session, NamedSavepoint &sv)
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
        session->status_var.ha_savepoint_rollback_count++;
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

    /* 
     * Bug #542299: segfault during set_difference() below.  copy<>() requires pre-allocation
     * of all elements, including the target, which is why we pre-allocate the set_difference_contexts
     * here
     */
    set_difference_contexts.reserve(max(tran_resource_contexts.size(), sv_resource_contexts.size()));

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
          session->status_var.ha_rollback_count++;
        }
      }
      resource_context->reset(); /* keep it conveniently zero-filled */
    }
  }
  trans->setResourceContexts(sv_resource_contexts);

  if (shouldConstructMessages())
  {
    cleanupTransactionMessage(getActiveTransactionMessage(session), session);
    message::Transaction *savepoint_transaction= sv.getTransactionMessage();
    if (savepoint_transaction != NULL)
    {
      /* Make a copy of the savepoint transaction, this is necessary to assure proper cleanup. 
         Upon commit the savepoint_transaction_copy will be cleaned up by a call to 
         cleanupTransactionMessage(). The Transaction message in NamedSavepoint will be cleaned
         up when the savepoint is cleaned up. This avoids calling delete twice on the Transaction.
      */ 
      message::Transaction *savepoint_transaction_copy= new message::Transaction(*sv.getTransactionMessage());
      uint32_t num_statements = savepoint_transaction_copy->statement_size();
      if (num_statements == 0)
      {    
        session->setStatementMessage(NULL);
      }    
      else 
      {
        session->setStatementMessage(savepoint_transaction_copy->mutable_statement(num_statements - 1));    
      }    
      session->setTransactionMessage(savepoint_transaction_copy);
    }
  }

  return error;
}

/**
  @note
  according to the sql standard (ISO/IEC 9075-2:2003)
  section "4.33.4 SQL-statements and transaction states",
  NamedSavepoint is *not* transaction-initiating SQL-statement
*/
int TransactionServices::setSavepoint(Session *session, NamedSavepoint &sv)
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
          session->status_var.ha_savepoint_count++;
        }
      }
    }
  }
  /*
    Remember the list of registered storage engines.
  */
  sv.setResourceContexts(resource_contexts);

  if (shouldConstructMessages())
  {
    message::Transaction *transaction= session->getTransactionMessage();
                  
    if (transaction != NULL)
    {
      message::Transaction *transaction_savepoint= 
        new message::Transaction(*transaction);
      sv.setTransactionMessage(transaction_savepoint);
    }
  } 

  return error;
}

int TransactionServices::releaseSavepoint(Session *session, NamedSavepoint &sv)
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

bool TransactionServices::shouldConstructMessages()
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  return replication_services.isActive();
}

message::Transaction *TransactionServices::getActiveTransactionMessage(Session *in_session, bool should_inc_trx_id)
{
  message::Transaction *transaction= in_session->getTransactionMessage();

  if (unlikely(transaction == NULL))
  {
    /* 
     * Allocate and initialize a new transaction message 
     * for this Session object.  Session is responsible for
     * deleting transaction message when done with it.
     */
    transaction= new (nothrow) message::Transaction();
    initTransactionMessage(*transaction, in_session, should_inc_trx_id);
    in_session->setTransactionMessage(transaction);
    return transaction;
  }
  else
    return transaction;
}

void TransactionServices::initTransactionMessage(message::Transaction &in_transaction,
                                                 Session *in_session,
                                                 bool should_inc_trx_id)
{
  message::TransactionContext *trx= in_transaction.mutable_transaction_context();
  trx->set_server_id(in_session->getServerId());

  if (should_inc_trx_id)
  {
    trx->set_transaction_id(getCurrentTransactionId(in_session));
    in_session->setXaId(0);
  }  
  else
  { 
    trx->set_transaction_id(0);
  }

  trx->set_start_timestamp(in_session->getCurrentTimestamp());
}

void TransactionServices::finalizeTransactionMessage(message::Transaction &in_transaction,
                                              Session *in_session)
{
  message::TransactionContext *trx= in_transaction.mutable_transaction_context();
  trx->set_end_timestamp(in_session->getCurrentTimestamp());
}

void TransactionServices::cleanupTransactionMessage(message::Transaction *in_transaction,
                                             Session *in_session)
{
  delete in_transaction;
  in_session->setStatementMessage(NULL);
  in_session->setTransactionMessage(NULL);
}

int TransactionServices::commitTransactionMessage(Session *in_session)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return 0;

  /* If there is an active statement message, finalize it */
  message::Statement *statement= in_session->getStatementMessage();

  if (statement != NULL)
  {
    finalizeStatementMessage(*statement, in_session);
  }
  else
    return 0; /* No data modification occurred inside the transaction */
  
  message::Transaction* transaction= getActiveTransactionMessage(in_session);

  finalizeTransactionMessage(*transaction, in_session);
  
  plugin::ReplicationReturnCode result= replication_services.pushTransactionMessage(*in_session, *transaction);

  cleanupTransactionMessage(transaction, in_session);

  return static_cast<int>(result);
}

void TransactionServices::initStatementMessage(message::Statement &statement,
                                        message::Statement::Type in_type,
                                        Session *in_session)
{
  statement.set_type(in_type);
  statement.set_start_timestamp(in_session->getCurrentTimestamp());
}

void TransactionServices::finalizeStatementMessage(message::Statement &statement,
                                            Session *in_session)
{
  statement.set_end_timestamp(in_session->getCurrentTimestamp());
  in_session->setStatementMessage(NULL);
}

void TransactionServices::rollbackTransactionMessage(Session *in_session)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(in_session);

  /*
   * OK, so there are two situations that we need to deal with here:
   *
   * 1) We receive an instruction to ROLLBACK the current transaction
   *    and the currently-stored Transaction message is *self-contained*, 
   *    meaning that no Statement messages in the Transaction message
   *    contain a message having its segment_id member greater than 1.  If
   *    no non-segment ID 1 members are found, we can simply clear the
   *    current Transaction message and remove it from memory.
   *
   * 2) If the Transaction message does indeed have a non-end segment, that
   *    means that a bulk update/delete/insert Transaction message segment
   *    has previously been sent over the wire to replicators.  In this case, 
   *    we need to package a Transaction with a Statement message of type
   *    ROLLBACK to indicate to replicators that previously-transmitted
   *    messages must be un-applied.
   */
  if (unlikely(message::transactionContainsBulkSegment(*transaction)))
  {
    /* Remember the transaction ID so we can re-use it */
    uint64_t trx_id= transaction->transaction_context().transaction_id();

    /*
     * Clear the transaction, create a Rollback statement message, 
     * attach it to the transaction, and push it to replicators.
     */
    transaction->Clear();
    initTransactionMessage(*transaction, in_session, false);

    /* Set the transaction ID to match the previous messages */
    transaction->mutable_transaction_context()->set_transaction_id(trx_id);

    message::Statement *statement= transaction->add_statement();

    initStatementMessage(*statement, message::Statement::ROLLBACK, in_session);
    finalizeStatementMessage(*statement, in_session);

    finalizeTransactionMessage(*transaction, in_session);
    
    (void) replication_services.pushTransactionMessage(*in_session, *transaction);
  }
  cleanupTransactionMessage(transaction, in_session);
}

message::Statement &TransactionServices::getInsertStatement(Session *in_session,
                                                            Table *in_table,
                                                            uint32_t *next_segment_id)
{
  message::Statement *statement= in_session->getStatementMessage();
  message::Transaction *transaction= NULL;

  /* 
   * Check the type for the current Statement message, if it is anything
   * other then INSERT we need to call finalize, this will ensure a 
   * new InsertStatement is created. If it is of type INSERT check
   * what table the INSERT belongs to, if it is a different table
   * call finalize, so a new InsertStatement can be created. 
   */
  if (statement != NULL && statement->type() != message::Statement::INSERT)
  {
    finalizeStatementMessage(*statement, in_session);
    statement= in_session->getStatementMessage();
  } 
  else if (statement != NULL)
  {
    transaction= getActiveTransactionMessage(in_session);

    /*
     * If we've passed our threshold for the statement size (possible for
     * a bulk insert), we'll finalize the Statement and Transaction (doing
     * the Transaction will keep it from getting huge).
     */
    if (static_cast<size_t>(transaction->ByteSize()) >= 
      in_session->variables.transaction_message_threshold)
    {
      /* Remember the transaction ID so we can re-use it */
      uint64_t trx_id= transaction->transaction_context().transaction_id();

      message::InsertData *current_data= statement->mutable_insert_data();

      /* Caller should use this value when adding a new record */
      *next_segment_id= current_data->segment_id() + 1;

      current_data->set_end_segment(false);

      /* 
       * Send the trx message to replicators after finalizing the 
       * statement and transaction. This will also set the Transaction
       * and Statement objects in Session to NULL.
       */
      commitTransactionMessage(in_session);

      /*
       * Statement and Transaction should now be NULL, so new ones will get
       * created. We reuse the transaction id since we are segmenting
       * one transaction.
       */
      statement= in_session->getStatementMessage();
      transaction= getActiveTransactionMessage(in_session, false);
      assert(transaction != NULL);

      /* Set the transaction ID to match the previous messages */
      transaction->mutable_transaction_context()->set_transaction_id(trx_id);
    }
    else
    {
      const message::InsertHeader &insert_header= statement->insert_header();
      string old_table_name= insert_header.table_metadata().table_name();
     
      string current_table_name;
      (void) in_table->getShare()->getTableName(current_table_name);

      if (current_table_name.compare(old_table_name))
      {
        finalizeStatementMessage(*statement, in_session);
        statement= in_session->getStatementMessage();
      }
      else
      {
        /* carry forward the existing segment id */
        const message::InsertData &current_data= statement->insert_data();
        *next_segment_id= current_data.segment_id();
      }
    }
  } 

  if (statement == NULL)
  {
    /*
     * Transaction will be non-NULL only if we had to segment it due to
     * transaction size above.
     */
    if (transaction == NULL)
      transaction= getActiveTransactionMessage(in_session);

    /* 
     * Transaction message initialized and set, but no statement created
     * yet.  We construct one and initialize it, here, then return the
     * message after attaching the new Statement message pointer to the 
     * Session for easy retrieval later...
     */
    statement= transaction->add_statement();
    setInsertHeader(*statement, in_session, in_table);
    in_session->setStatementMessage(statement);
  }
  return *statement;
}

void TransactionServices::setInsertHeader(message::Statement &statement,
                                          Session *in_session,
                                          Table *in_table)
{
  initStatementMessage(statement, message::Statement::INSERT, in_session);

  /* 
   * Now we construct the specialized InsertHeader message inside
   * the generalized message::Statement container...
   */
  /* Set up the insert header */
  message::InsertHeader *header= statement.mutable_insert_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  string schema_name;
  (void) in_table->getShare()->getSchemaName(schema_name);
  string table_name;
  (void) in_table->getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  Field *current_field;
  Field **table_fields= in_table->getFields();

  message::FieldMetadata *field_metadata;

  /* We will read all the table's fields... */
  in_table->setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    field_metadata= header->add_field_metadata();
    field_metadata->set_name(current_field->field_name);
    field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
  }
}

bool TransactionServices::insertRecord(Session *in_session, Table *in_table)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return false;
  /**
   * We do this check here because we don't want to even create a 
   * statement if there isn't a primary key on the table...
   *
   * @todo
   *
   * Multi-column primary keys are handled how exactly?
   */
  if (not in_table->getShare()->hasPrimaryKey())
  {
    my_error(ER_NO_PRIMARY_KEY_ON_REPLICATED_TABLE, MYF(0));
    return true;
  }

  uint32_t next_segment_id= 1;
  message::Statement &statement= getInsertStatement(in_session, in_table, &next_segment_id);

  message::InsertData *data= statement.mutable_insert_data();
  data->set_segment_id(next_segment_id);
  data->set_end_segment(true);
  message::InsertRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= in_table->getFields();

  String *string_value= new (in_session->mem_root) String(TransactionServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  /* We will read all the table's fields... */
  in_table->setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    if (current_field->is_null())
    {
      record->add_is_null(true);
      record->add_insert_value("", 0);
    } 
    else 
    {
      string_value= current_field->val_str(string_value);
      record->add_is_null(false);
      record->add_insert_value(string_value->c_ptr(), string_value->length());
      string_value->free();
    }
  }
  return false;
}

message::Statement &TransactionServices::getUpdateStatement(Session *in_session,
                                                            Table *in_table,
                                                            const unsigned char *old_record, 
                                                            const unsigned char *new_record,
                                                            uint32_t *next_segment_id)
{
  message::Statement *statement= in_session->getStatementMessage();
  message::Transaction *transaction= NULL;

  /*
   * Check the type for the current Statement message, if it is anything
   * other then UPDATE we need to call finalize, this will ensure a
   * new UpdateStatement is created. If it is of type UPDATE check
   * what table the UPDATE belongs to, if it is a different table
   * call finalize, so a new UpdateStatement can be created.
   */
  if (statement != NULL && statement->type() != message::Statement::UPDATE)
  {
    finalizeStatementMessage(*statement, in_session);
    statement= in_session->getStatementMessage();
  }
  else if (statement != NULL)
  {
    transaction= getActiveTransactionMessage(in_session);

    /*
     * If we've passed our threshold for the statement size (possible for
     * a bulk insert), we'll finalize the Statement and Transaction (doing
     * the Transaction will keep it from getting huge).
     */
    if (static_cast<size_t>(transaction->ByteSize()) >= 
      in_session->variables.transaction_message_threshold)
    {
      /* Remember the transaction ID so we can re-use it */
      uint64_t trx_id= transaction->transaction_context().transaction_id();

      message::UpdateData *current_data= statement->mutable_update_data();

      /* Caller should use this value when adding a new record */
      *next_segment_id= current_data->segment_id() + 1;

      current_data->set_end_segment(false);

      /*
       * Send the trx message to replicators after finalizing the 
       * statement and transaction. This will also set the Transaction
       * and Statement objects in Session to NULL.
       */
      commitTransactionMessage(in_session);

      /*
       * Statement and Transaction should now be NULL, so new ones will get
       * created. We reuse the transaction id since we are segmenting
       * one transaction.
       */
      statement= in_session->getStatementMessage();
      transaction= getActiveTransactionMessage(in_session, false);
      assert(transaction != NULL);

      /* Set the transaction ID to match the previous messages */
      transaction->mutable_transaction_context()->set_transaction_id(trx_id);
    }
    else
    {
      if (useExistingUpdateHeader(*statement, in_table, old_record, new_record))
      {
        /* carry forward the existing segment id */
        const message::UpdateData &current_data= statement->update_data();
        *next_segment_id= current_data.segment_id();
      } 
      else 
      {
        finalizeStatementMessage(*statement, in_session);
        statement= in_session->getStatementMessage();
      }
    }
  }

  if (statement == NULL)
  {
    /*
     * Transaction will be non-NULL only if we had to segment it due to
     * transaction size above.
     */
    if (transaction == NULL)
      transaction= getActiveTransactionMessage(in_session);

    /* 
     * Transaction message initialized and set, but no statement created
     * yet.  We construct one and initialize it, here, then return the
     * message after attaching the new Statement message pointer to the 
     * Session for easy retrieval later...
     */
    statement= transaction->add_statement();
    setUpdateHeader(*statement, in_session, in_table, old_record, new_record);
    in_session->setStatementMessage(statement);
  }
  return *statement;
}

bool TransactionServices::useExistingUpdateHeader(message::Statement &statement,
                                                  Table *in_table,
                                                  const unsigned char *old_record,
                                                  const unsigned char *new_record)
{
  const message::UpdateHeader &update_header= statement.update_header();
  string old_table_name= update_header.table_metadata().table_name();

  string current_table_name;
  (void) in_table->getShare()->getTableName(current_table_name);
  if (current_table_name.compare(old_table_name))
  {
    return false;
  }
  else
  {
    /* Compare the set fields in the existing UpdateHeader and see if they
     * match the updated fields in the new record, if they do not we must
     * create a new UpdateHeader 
     */
    size_t num_set_fields= update_header.set_field_metadata_size();

    Field *current_field;
    Field **table_fields= in_table->getFields();
    in_table->setReadSet();

    size_t num_calculated_updated_fields= 0;
    bool found= false;
    while ((current_field= *table_fields++) != NULL)
    {
      if (num_calculated_updated_fields > num_set_fields)
      {
        break;
      }

      if (isFieldUpdated(current_field, in_table, old_record, new_record))
      {
        /* check that this field exists in the UpdateHeader record */
        found= false;

        for (size_t x= 0; x < num_set_fields; ++x)
        {
          const message::FieldMetadata &field_metadata= update_header.set_field_metadata(x);
          string name= field_metadata.name();
          if (name.compare(current_field->field_name) == 0)
          {
            found= true;
            ++num_calculated_updated_fields;
            break;
          } 
        }
        if (! found)
        {
          break;
        } 
      }
    }

    if ((num_calculated_updated_fields == num_set_fields) && found)
    {
      return true;
    } 
    else 
    {
      return false;
    }
  }
}  

void TransactionServices::setUpdateHeader(message::Statement &statement,
                                          Session *in_session,
                                          Table *in_table,
                                          const unsigned char *old_record, 
                                          const unsigned char *new_record)
{
  initStatementMessage(statement, message::Statement::UPDATE, in_session);

  /* 
   * Now we construct the specialized UpdateHeader message inside
   * the generalized message::Statement container...
   */
  /* Set up the update header */
  message::UpdateHeader *header= statement.mutable_update_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  string schema_name;
  (void) in_table->getShare()->getSchemaName(schema_name);
  string table_name;
  (void) in_table->getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  Field *current_field;
  Field **table_fields= in_table->getFields();

  message::FieldMetadata *field_metadata;

  /* We will read all the table's fields... */
  in_table->setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    /*
     * We add the "key field metadata" -- i.e. the fields which is
     * the primary key for the table.
     */
    if (in_table->getShare()->fieldInPrimaryKey(current_field))
    {
      field_metadata= header->add_key_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
    }

    if (isFieldUpdated(current_field, in_table, old_record, new_record))
    {
      /* Field is changed from old to new */
      field_metadata= header->add_set_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
    }
  }
}
void TransactionServices::updateRecord(Session *in_session,
                                       Table *in_table, 
                                       const unsigned char *old_record, 
                                       const unsigned char *new_record)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;

  uint32_t next_segment_id= 1;
  message::Statement &statement= getUpdateStatement(in_session, in_table, old_record, new_record, &next_segment_id);

  message::UpdateData *data= statement.mutable_update_data();
  data->set_segment_id(next_segment_id);
  data->set_end_segment(true);
  message::UpdateRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= in_table->getFields();
  String *string_value= new (in_session->mem_root) String(TransactionServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  while ((current_field= *table_fields++) != NULL) 
  {
    /*
     * Here, we add the SET field values.  We used to do this in the setUpdateHeader() method, 
     * but then realized that an UPDATE statement could potentially have different values for
     * the SET field.  For instance, imagine this SQL scenario:
     *
     * CREATE TABLE t1 (id INT NOT NULL PRIMARY KEY, count INT NOT NULL);
     * INSERT INTO t1 (id, counter) VALUES (1,1),(2,2),(3,3);
     * UPDATE t1 SET counter = counter + 1 WHERE id IN (1,2);
     *
     * We will generate two UpdateRecord messages with different set_value byte arrays.
     */
    if (isFieldUpdated(current_field, in_table, old_record, new_record))
    {
      /* Store the original "read bit" for this field */
      bool is_read_set= current_field->isReadSet();

      /* We need to mark that we will "read" this field... */
      in_table->setReadSet(current_field->field_index);

      /* Read the string value of this field's contents */
      string_value= current_field->val_str(string_value);

      /* 
       * Reset the read bit after reading field to its original state.  This 
       * prevents the field from being included in the WHERE clause
       */
      current_field->setReadSet(is_read_set);

      if (current_field->is_null())
      {
        record->add_is_null(true);
        record->add_after_value("", 0);
      }
      else
      {
        record->add_is_null(false);
        record->add_after_value(string_value->c_ptr(), string_value->length());
      }
      string_value->free();
    }

    /* 
     * Add the WHERE clause values now...for now, this means the
     * primary key field value.  Replication only supports tables
     * with a primary key.
     */
    if (in_table->getShare()->fieldInPrimaryKey(current_field))
    {
      /**
       * To say the below is ugly is an understatement. But it works.
       * 
       * @todo Move this crap into a real Record API.
       */
      string_value= current_field->val_str(string_value,
                                           old_record + 
                                           current_field->offset(const_cast<unsigned char *>(new_record)));
      record->add_key_value(string_value->c_ptr(), string_value->length());
      string_value->free();
    }

  }
}

bool TransactionServices::isFieldUpdated(Field *current_field,
                                         Table *in_table,
                                         const unsigned char *old_record,
                                         const unsigned char *new_record)
{
  /*
   * The below really should be moved into the Field API and Record API.  But for now
   * we do this crazy pointer fiddling to figure out if the current field
   * has been updated in the supplied record raw byte pointers.
   */
  const unsigned char *old_ptr= (const unsigned char *) old_record + (ptrdiff_t) (current_field->ptr - in_table->getInsertRecord());
  const unsigned char *new_ptr= (const unsigned char *) new_record + (ptrdiff_t) (current_field->ptr - in_table->getInsertRecord());

  uint32_t field_length= current_field->pack_length(); /** @TODO This isn't always correct...check varchar diffs. */

  bool old_value_is_null= current_field->is_null_in_record(old_record);
  bool new_value_is_null= current_field->is_null_in_record(new_record);

  bool isUpdated= false;
  if (old_value_is_null != new_value_is_null)
  {
    if ((old_value_is_null) && (! new_value_is_null)) /* old value is NULL, new value is non NULL */
    {
      isUpdated= true;
    }
    else if ((! old_value_is_null) && (new_value_is_null)) /* old value is non NULL, new value is NULL */
    {
      isUpdated= true;
    }
  }

  if (! isUpdated)
  {
    if (memcmp(old_ptr, new_ptr, field_length) != 0)
    {
      isUpdated= true;
    }
  }
  return isUpdated;
}  

message::Statement &TransactionServices::getDeleteStatement(Session *in_session,
                                                            Table *in_table,
                                                            uint32_t *next_segment_id)
{
  message::Statement *statement= in_session->getStatementMessage();
  message::Transaction *transaction= NULL;

  /*
   * Check the type for the current Statement message, if it is anything
   * other then DELETE we need to call finalize, this will ensure a
   * new DeleteStatement is created. If it is of type DELETE check
   * what table the DELETE belongs to, if it is a different table
   * call finalize, so a new DeleteStatement can be created.
   */
  if (statement != NULL && statement->type() != message::Statement::DELETE)
  {
    finalizeStatementMessage(*statement, in_session);
    statement= in_session->getStatementMessage();
  }
  else if (statement != NULL)
  {
    transaction= getActiveTransactionMessage(in_session);

    /*
     * If we've passed our threshold for the statement size (possible for
     * a bulk insert), we'll finalize the Statement and Transaction (doing
     * the Transaction will keep it from getting huge).
     */
    if (static_cast<size_t>(transaction->ByteSize()) >= 
      in_session->variables.transaction_message_threshold)
    {
      /* Remember the transaction ID so we can re-use it */
      uint64_t trx_id= transaction->transaction_context().transaction_id();

      message::DeleteData *current_data= statement->mutable_delete_data();

      /* Caller should use this value when adding a new record */
      *next_segment_id= current_data->segment_id() + 1;

      current_data->set_end_segment(false);

      /* 
       * Send the trx message to replicators after finalizing the 
       * statement and transaction. This will also set the Transaction
       * and Statement objects in Session to NULL.
       */
      commitTransactionMessage(in_session);

      /*
       * Statement and Transaction should now be NULL, so new ones will get
       * created. We reuse the transaction id since we are segmenting
       * one transaction.
       */
      statement= in_session->getStatementMessage();
      transaction= getActiveTransactionMessage(in_session, false);
      assert(transaction != NULL);

      /* Set the transaction ID to match the previous messages */
      transaction->mutable_transaction_context()->set_transaction_id(trx_id);
    }
    else
    {
      const message::DeleteHeader &delete_header= statement->delete_header();
      string old_table_name= delete_header.table_metadata().table_name();

      string current_table_name;
      (void) in_table->getShare()->getTableName(current_table_name);
      if (current_table_name.compare(old_table_name))
      {
        finalizeStatementMessage(*statement, in_session);
        statement= in_session->getStatementMessage();
      }
      else
      {
        /* carry forward the existing segment id */
        const message::DeleteData &current_data= statement->delete_data();
        *next_segment_id= current_data.segment_id();
      }
    }
  }

  if (statement == NULL)
  {
    /*
     * Transaction will be non-NULL only if we had to segment it due to
     * transaction size above.
     */
    if (transaction == NULL)
      transaction= getActiveTransactionMessage(in_session);

    /* 
     * Transaction message initialized and set, but no statement created
     * yet.  We construct one and initialize it, here, then return the
     * message after attaching the new Statement message pointer to the 
     * Session for easy retrieval later...
     */
    statement= transaction->add_statement();
    setDeleteHeader(*statement, in_session, in_table);
    in_session->setStatementMessage(statement);
  }
  return *statement;
}

void TransactionServices::setDeleteHeader(message::Statement &statement,
                                          Session *in_session,
                                          Table *in_table)
{
  initStatementMessage(statement, message::Statement::DELETE, in_session);

  /* 
   * Now we construct the specialized DeleteHeader message inside
   * the generalized message::Statement container...
   */
  message::DeleteHeader *header= statement.mutable_delete_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  string schema_name;
  (void) in_table->getShare()->getSchemaName(schema_name);
  string table_name;
  (void) in_table->getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  Field *current_field;
  Field **table_fields= in_table->getFields();

  message::FieldMetadata *field_metadata;

  while ((current_field= *table_fields++) != NULL) 
  {
    /* 
     * Add the WHERE clause values now...for now, this means the
     * primary key field value.  Replication only supports tables
     * with a primary key.
     */
    if (in_table->getShare()->fieldInPrimaryKey(current_field))
    {
      field_metadata= header->add_key_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
    }
  }
}

void TransactionServices::deleteRecord(Session *in_session, Table *in_table, bool use_update_record)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;

  uint32_t next_segment_id= 1;
  message::Statement &statement= getDeleteStatement(in_session, in_table, &next_segment_id);

  message::DeleteData *data= statement.mutable_delete_data();
  data->set_segment_id(next_segment_id);
  data->set_end_segment(true);
  message::DeleteRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= in_table->getFields();
  String *string_value= new (in_session->mem_root) String(TransactionServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  while ((current_field= *table_fields++) != NULL) 
  {
    /* 
     * Add the WHERE clause values now...for now, this means the
     * primary key field value.  Replication only supports tables
     * with a primary key.
     */
    if (in_table->getShare()->fieldInPrimaryKey(current_field))
    {
      if (use_update_record)
      {
        /*
         * Temporarily point to the update record to get its value.
         * This is pretty much a hack in order to get the PK value from
         * the update record rather than the insert record. Field::val_str()
         * should not change anything in Field::ptr, so this should be safe.
         * We are careful not to change anything in old_ptr.
         */
        const unsigned char *old_ptr= current_field->ptr;
        current_field->ptr= in_table->getUpdateRecord() + static_cast<ptrdiff_t>(old_ptr - in_table->getInsertRecord());
        string_value= current_field->val_str(string_value);
        current_field->ptr= const_cast<unsigned char *>(old_ptr);
      }
      else
      {
        string_value= current_field->val_str(string_value);
        /**
         * @TODO Store optional old record value in the before data member
         */
      }
      record->add_key_value(string_value->c_ptr(), string_value->length());
      string_value->free();
    }
  }
}

void TransactionServices::createTable(Session *in_session,
                                      const message::Table &table)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(in_session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::CREATE_TABLE, in_session);

  /* 
   * Construct the specialized CreateTableStatement message and attach
   * it to the generic Statement message
   */
  message::CreateTableStatement *create_table_statement= statement->mutable_create_table_statement();
  message::Table *new_table_message= create_table_statement->mutable_table();
  *new_table_message= table;

  finalizeStatementMessage(*statement, in_session);

  finalizeTransactionMessage(*transaction, in_session);
  
  (void) replication_services.pushTransactionMessage(*in_session, *transaction);

  cleanupTransactionMessage(transaction, in_session);

}

void TransactionServices::createSchema(Session *in_session,
                                       const message::Schema &schema)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(in_session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::CREATE_SCHEMA, in_session);

  /* 
   * Construct the specialized CreateSchemaStatement message and attach
   * it to the generic Statement message
   */
  message::CreateSchemaStatement *create_schema_statement= statement->mutable_create_schema_statement();
  message::Schema *new_schema_message= create_schema_statement->mutable_schema();
  *new_schema_message= schema;

  finalizeStatementMessage(*statement, in_session);

  finalizeTransactionMessage(*transaction, in_session);
  
  (void) replication_services.pushTransactionMessage(*in_session, *transaction);

  cleanupTransactionMessage(transaction, in_session);

}

void TransactionServices::dropSchema(Session *in_session, const string &schema_name)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(in_session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::DROP_SCHEMA, in_session);

  /* 
   * Construct the specialized DropSchemaStatement message and attach
   * it to the generic Statement message
   */
  message::DropSchemaStatement *drop_schema_statement= statement->mutable_drop_schema_statement();

  drop_schema_statement->set_schema_name(schema_name);

  finalizeStatementMessage(*statement, in_session);

  finalizeTransactionMessage(*transaction, in_session);
  
  (void) replication_services.pushTransactionMessage(*in_session, *transaction);

  cleanupTransactionMessage(transaction, in_session);
}

void TransactionServices::dropTable(Session *in_session,
                                    const string &schema_name,
                                    const string &table_name,
                                    bool if_exists)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(in_session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::DROP_TABLE, in_session);

  /* 
   * Construct the specialized DropTableStatement message and attach
   * it to the generic Statement message
   */
  message::DropTableStatement *drop_table_statement= statement->mutable_drop_table_statement();

  drop_table_statement->set_if_exists_clause(if_exists);

  message::TableMetadata *table_metadata= drop_table_statement->mutable_table_metadata();

  table_metadata->set_schema_name(schema_name);
  table_metadata->set_table_name(table_name);

  finalizeStatementMessage(*statement, in_session);

  finalizeTransactionMessage(*transaction, in_session);
  
  (void) replication_services.pushTransactionMessage(*in_session, *transaction);

  cleanupTransactionMessage(transaction, in_session);
}

void TransactionServices::truncateTable(Session *in_session, Table *in_table)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(in_session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::TRUNCATE_TABLE, in_session);

  /* 
   * Construct the specialized TruncateTableStatement message and attach
   * it to the generic Statement message
   */
  message::TruncateTableStatement *truncate_statement= statement->mutable_truncate_table_statement();
  message::TableMetadata *table_metadata= truncate_statement->mutable_table_metadata();

  string schema_name;
  (void) in_table->getShare()->getSchemaName(schema_name);
  string table_name;
  (void) in_table->getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  finalizeStatementMessage(*statement, in_session);

  finalizeTransactionMessage(*transaction, in_session);
  
  (void) replication_services.pushTransactionMessage(*in_session, *transaction);

  cleanupTransactionMessage(transaction, in_session);
}

void TransactionServices::rawStatement(Session *in_session, const string &query)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(in_session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::RAW_SQL, in_session);
  statement->set_sql(query);
  finalizeStatementMessage(*statement, in_session);

  finalizeTransactionMessage(*transaction, in_session);
  
  (void) replication_services.pushTransactionMessage(*in_session, *transaction);

  cleanupTransactionMessage(transaction, in_session);
}

int TransactionServices::sendEvent(Session *session, const message::Event &event)
{
  ReplicationServices &replication_services= ReplicationServices::singleton();
  if (! replication_services.isActive())
    return 0;

  message::Transaction *transaction= new (nothrow) message::Transaction();

  // set server id, start timestamp
  initTransactionMessage(*transaction, session, true);

  // set end timestamp
  finalizeTransactionMessage(*transaction, session);

  message::Event *trx_event= transaction->mutable_event();

  trx_event->CopyFrom(event);

  plugin::ReplicationReturnCode result= replication_services.pushTransactionMessage(*session, *transaction);

  delete transaction;

  return static_cast<int>(result);
}

bool TransactionServices::sendStartupEvent(Session *session)
{
  message::Event event;
  event.set_type(message::Event::STARTUP);
  if (sendEvent(session, event) != 0)
    return false;
  return true;
}

bool TransactionServices::sendShutdownEvent(Session *session)
{
  message::Event event;
  event.set_type(message::Event::SHUTDOWN);
  if (sendEvent(session, event) != 0)
    return false;
  return true;
}

} /* namespace drizzled */
