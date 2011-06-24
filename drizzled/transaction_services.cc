/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
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

#include <config.h>
#include <drizzled/current_session.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/probes.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/sql_base.h>
#include <drizzled/replication_services.h>
#include <drizzled/transaction_services.h>
#include <drizzled/transaction_context.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/message/statement_transform.h>
#include <drizzled/resource_context.h>
#include <drizzled/lock.h>
#include <drizzled/item/int.h>
#include <drizzled/item/empty_string.h>
#include <drizzled/field/epoch.h>
#include <drizzled/plugin/client.h>
#include <drizzled/plugin/monitored_in_transaction.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/plugin/xa_resource_manager.h>
#include <drizzled/plugin/xa_storage_engine.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>
#include <drizzled/session/transactions.h>

#include <vector>
#include <algorithm>
#include <functional>
#include <google/protobuf/repeated_field.h>

using namespace std;
using namespace google;

namespace drizzled {

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

static plugin::XaStorageEngine& xa_storage_engine()
{
  static plugin::XaStorageEngine& engine= static_cast<plugin::XaStorageEngine&>(*plugin::StorageEngine::findByName("InnoDB"));
  return engine;
}

void TransactionServices::registerResourceForStatement(Session& session,
                                                       plugin::MonitoredInTransaction *monitored,
                                                       plugin::TransactionalStorageEngine *engine)
{
  if (session_test_options(&session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
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

  TransactionContext& trans= session.transaction.stmt;
  ResourceContext& resource_context= session.getResourceContext(*monitored, 0);

  if (resource_context.isStarted())
    return; /* already registered, return */

  assert(monitored->participatesInSqlTransaction());
  assert(not monitored->participatesInXaTransaction());

  resource_context.setMonitored(monitored);
  resource_context.setTransactionalStorageEngine(engine);
  trans.registerResource(&resource_context);
  trans.no_2pc= true;
}

void TransactionServices::registerResourceForStatement(Session& session,
                                                       plugin::MonitoredInTransaction *monitored,
                                                       plugin::TransactionalStorageEngine *engine,
                                                       plugin::XaResourceManager *resource_manager)
{
  if (session_test_options(&session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
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

  TransactionContext& trans= session.transaction.stmt;
  ResourceContext& resource_context= session.getResourceContext(*monitored, 0);

  if (resource_context.isStarted())
    return; /* already registered, return */

  assert(monitored->participatesInXaTransaction());
  assert(monitored->participatesInSqlTransaction());

  resource_context.setMonitored(monitored);
  resource_context.setTransactionalStorageEngine(engine);
  resource_context.setXaResourceManager(resource_manager);
  trans.registerResource(&resource_context);
}

void TransactionServices::registerResourceForTransaction(Session& session,
                                                         plugin::MonitoredInTransaction *monitored,
                                                         plugin::TransactionalStorageEngine *engine)
{
  TransactionContext& trans= session.transaction.all;
  ResourceContext& resource_context= session.getResourceContext(*monitored, 1);

  if (resource_context.isStarted())
    return; /* already registered, return */

  session.server_status|= SERVER_STATUS_IN_TRANS;

  trans.registerResource(&resource_context);

  assert(monitored->participatesInSqlTransaction());
  assert(not monitored->participatesInXaTransaction());

  resource_context.setMonitored(monitored);
  resource_context.setTransactionalStorageEngine(engine);
  trans.no_2pc= true;

  if (session.transaction.xid_state.xid.is_null())
    session.transaction.xid_state.xid.set(session.getQueryId());

  /* Only true if user is executing a BEGIN WORK/START TRANSACTION */
  if (not session.getResourceContext(*monitored, 0).isStarted())
    registerResourceForStatement(session, monitored, engine);
}

void TransactionServices::registerResourceForTransaction(Session& session,
                                                         plugin::MonitoredInTransaction *monitored,
                                                         plugin::TransactionalStorageEngine *engine,
                                                         plugin::XaResourceManager *resource_manager)
{
  TransactionContext *trans= &session.transaction.all;
  ResourceContext& resource_context= session.getResourceContext(*monitored, 1);

  if (resource_context.isStarted())
    return; /* already registered, return */

  session.server_status|= SERVER_STATUS_IN_TRANS;

  trans->registerResource(&resource_context);

  assert(monitored->participatesInSqlTransaction());

  resource_context.setMonitored(monitored);
  resource_context.setXaResourceManager(resource_manager);
  resource_context.setTransactionalStorageEngine(engine);
  trans->no_2pc= true;

  if (session.transaction.xid_state.xid.is_null())
    session.transaction.xid_state.xid.set(session.getQueryId());

  engine->startTransaction(&session, START_TRANS_NO_OPTIONS);

  /* Only true if user is executing a BEGIN WORK/START TRANSACTION */
  if (! session.getResourceContext(*monitored, 0).isStarted())
    registerResourceForStatement(session, monitored, engine, resource_manager);
}

void TransactionServices::allocateNewTransactionId()
{
  if (! ReplicationServices::isActive())
  {
    return;
  }

  Session *my_session= current_session;
  uint64_t xa_id= xa_storage_engine().getNewTransactionId(my_session);
  my_session->setXaId(xa_id);
}

uint64_t TransactionServices::getCurrentTransactionId(Session& session)
{
  if (session.getXaId() == 0)
  {
    session.setXaId(xa_storage_engine().getNewTransactionId(&session)); 
  }

  return session.getXaId();
}

int TransactionServices::commitTransaction(Session& session,
                                           bool normal_transaction)
{
  int error= 0, cookie= 0;
  /*
    'all' means that this is either an explicit commit issued by
    user, or an implicit commit issued by a DDL.
  */
  TransactionContext *trans= normal_transaction ? &session.transaction.all : &session.transaction.stmt;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  bool is_real_trans= normal_transaction || session.transaction.all.getResourceContexts().empty();

  /*
    We must not commit the normal transaction if a statement
    transaction is pending. Otherwise statement transaction
    flags will not get propagated to its normal transaction's
    counterpart.
  */
  assert(session.transaction.stmt.getResourceContexts().empty() ||
              trans == &session.transaction.stmt);

  if (resource_contexts.empty() == false)
  {
    if (is_real_trans && session.wait_if_global_read_lock(false, false))
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
      BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, resource_contexts)
      {
        if (error)
          break;
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
          if (int err= resource_context->getXaResourceManager()->xaPrepare(&session, normal_transaction))
          {
            my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
            error= 1;
          }
          else
          {
            session.status_var.ha_prepare_count++;
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
      session.startWaitingGlobalReadLock();
  }
  return error;
}

/**
  @note
  This function does not care about global read lock. A caller should.
*/
int TransactionServices::commitPhaseOne(Session& session,
                                        bool normal_transaction)
{
  int error=0;
  TransactionContext *trans= normal_transaction ? &session.transaction.all : &session.transaction.stmt;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  bool is_real_trans= normal_transaction || session.transaction.all.getResourceContexts().empty();
  bool all= normal_transaction;

  /* If we're in autocommit then we have a real transaction to commit
     (except if it's BEGIN)
  */
  if (! session_test_options(&session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
    all= true;

  if (resource_contexts.empty() == false)
  {
    BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, resource_contexts)
    {
      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInXaTransaction())
      {
        if (int err= resource_context->getXaResourceManager()->xaCommit(&session, all))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          session.status_var.ha_commit_count++;
        }
      }
      else if (resource->participatesInSqlTransaction())
      {
        if (int err= resource_context->getTransactionalStorageEngine()->commit(&session, all))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          session.status_var.ha_commit_count++;
        }
      }
      resource_context->reset(); /* keep it conveniently zero-filled */
    }

    if (is_real_trans)
      session.transaction.xid_state.xid.set_null();

    if (normal_transaction)
    {
      session.variables.tx_isolation= session.session_tx_isolation;
      session.transaction.cleanup();
    }
  }
  trans->reset();
  return error;
}

int TransactionServices::rollbackTransaction(Session& session,
                                             bool normal_transaction)
{
  int error= 0;
  TransactionContext *trans= normal_transaction ? &session.transaction.all : &session.transaction.stmt;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  bool is_real_trans= normal_transaction || session.transaction.all.getResourceContexts().empty();
  bool all = normal_transaction || !session_test_options(&session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN);

  /*
    We must not rollback the normal transaction if a statement
    transaction is pending.
  */
  assert(session.transaction.stmt.getResourceContexts().empty() || trans == &session.transaction.stmt);

  if (resource_contexts.empty() == false)
  {
    BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, resource_contexts)
    {
      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInXaTransaction())
      {
        if (int err= resource_context->getXaResourceManager()->xaRollback(&session, all))
        {
          my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          session.status_var.ha_rollback_count++;
        }
      }
      else if (resource->participatesInSqlTransaction())
      {
        if (int err= resource_context->getTransactionalStorageEngine()->rollback(&session, all))
        {
          my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
          error= 1;
        }
        else if (normal_transaction)
        {
          session.status_var.ha_rollback_count++;
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
    if (all)
      rollbackTransactionMessage(session);
    else
      rollbackStatementMessage(session);

    if (is_real_trans)
      session.transaction.xid_state.xid.set_null();
    if (normal_transaction)
    {
      session.variables.tx_isolation=session.session_tx_isolation;
      session.transaction.cleanup();
    }
  }
  if (normal_transaction)
    session.transaction_rollback_request= false;

  /*
   * If a non-transactional table was updated, warn the user
   */
  if (is_real_trans &&
      session.transaction.all.hasModifiedNonTransData() &&
      session.getKilled() != Session::KILL_CONNECTION)
  {
    push_warning(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
  }
  trans->reset();
  return error;
}

int TransactionServices::autocommitOrRollback(Session& session,
                                              int error)
{
  /* One GPB Statement message per SQL statement */
  message::Statement *statement= session.getStatementMessage();
  if ((statement != NULL) && (! error))
    finalizeStatementMessage(*statement, session);

  if (session.transaction.stmt.getResourceContexts().empty() == false)
  {
    TransactionContext *trans = &session.transaction.stmt;
    TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();
    BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, resource_contexts)
    {
      resource_context->getTransactionalStorageEngine()->endStatement(&session);
    }

    if (! error)
    {
      if (commitTransaction(session, false))
        error= 1;
    }
    else
    {
      (void) rollbackTransaction(session, false);
      if (session.transaction_rollback_request)
      {
        (void) rollbackTransaction(session, true);
        session.server_status&= ~SERVER_STATUS_IN_TRANS;
      }
    }

    session.variables.tx_isolation= session.session_tx_isolation;
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

int TransactionServices::rollbackToSavepoint(Session& session,
                                             NamedSavepoint &sv)
{
  int error= 0;
  TransactionContext *trans= &session.transaction.all;
  TransactionContext::ResourceContexts &tran_resource_contexts= trans->getResourceContexts();
  TransactionContext::ResourceContexts &sv_resource_contexts= sv.getResourceContexts();

  trans->no_2pc= false;
  /*
    rolling back to savepoint in all storage engines that were part of the
    transaction when the savepoint was set
  */
  BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, sv_resource_contexts)
  {
    plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

    if (resource->participatesInSqlTransaction())
    {
      if (int err= resource_context->getTransactionalStorageEngine()->rollbackToSavepoint(&session, sv))
      {
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error= 1;
      }
      else
      {
        session.status_var.ha_savepoint_rollback_count++;
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
        
    BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, set_difference_contexts)
    {
      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInSqlTransaction())
      {
        if (int err= resource_context->getTransactionalStorageEngine()->rollback(&session, true))
        {
          my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
          error= 1;
        }
        else
        {
          session.status_var.ha_rollback_count++;
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
        session.setStatementMessage(NULL);
      }    
      else 
      {
        session.setStatementMessage(savepoint_transaction_copy->mutable_statement(num_statements - 1));    
      }    
      session.setTransactionMessage(savepoint_transaction_copy);
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
int TransactionServices::setSavepoint(Session& session,
                                      NamedSavepoint &sv)
{
  int error= 0;
  TransactionContext *trans= &session.transaction.all;
  TransactionContext::ResourceContexts &resource_contexts= trans->getResourceContexts();

  if (resource_contexts.empty() == false)
  {
    BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, resource_contexts)
    {
      plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

      if (resource->participatesInSqlTransaction())
      {
        if (int err= resource_context->getTransactionalStorageEngine()->setSavepoint(&session, sv))
        {
          my_error(ER_GET_ERRNO, MYF(0), err);
          error= 1;
        }
        else
        {
          session.status_var.ha_savepoint_count++;
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
    message::Transaction *transaction= session.getTransactionMessage();
                  
    if (transaction != NULL)
    {
      message::Transaction *transaction_savepoint= 
        new message::Transaction(*transaction);
      sv.setTransactionMessage(transaction_savepoint);
    }
  } 

  return error;
}

int TransactionServices::releaseSavepoint(Session& session,
                                          NamedSavepoint &sv)
{
  int error= 0;

  TransactionContext::ResourceContexts &resource_contexts= sv.getResourceContexts();

  BOOST_FOREACH(TransactionContext::ResourceContexts::reference resource_context, resource_contexts)
  {
    plugin::MonitoredInTransaction *resource= resource_context->getMonitored();

    if (resource->participatesInSqlTransaction())
    {
      if (int err= resource_context->getTransactionalStorageEngine()->releaseSavepoint(&session, sv))
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
  return ReplicationServices::isActive();
}

message::Transaction *TransactionServices::getActiveTransactionMessage(Session& session,
                                                                       bool should_inc_trx_id)
{
  message::Transaction *transaction= session.getTransactionMessage();

  if (unlikely(transaction == NULL))
  {
    /* 
     * Allocate and initialize a new transaction message 
     * for this Session object.  Session is responsible for
     * deleting transaction message when done with it.
     */
    transaction= new message::Transaction();
    initTransactionMessage(*transaction, session, should_inc_trx_id);
    session.setTransactionMessage(transaction);
  }
  return transaction;
}

void TransactionServices::initTransactionMessage(message::Transaction &transaction,
                                                 Session& session,
                                                 bool should_inc_trx_id)
{
  message::TransactionContext *trx= transaction.mutable_transaction_context();
  trx->set_server_id(session.getServerId());

  if (should_inc_trx_id)
  {
    trx->set_transaction_id(getCurrentTransactionId(session));
    session.setXaId(0);
  }
  else
  {
    /* trx and seg id will get set properly elsewhere */
    trx->set_transaction_id(0);
  }

  trx->set_start_timestamp(session.times.getCurrentTimestamp());
  
  /* segment info may get set elsewhere as needed */
  transaction.set_segment_id(1);
  transaction.set_end_segment(true);
}

void TransactionServices::finalizeTransactionMessage(message::Transaction &transaction,
                                                     const Session& session)
{
  message::TransactionContext *trx= transaction.mutable_transaction_context();
  trx->set_end_timestamp(session.times.getCurrentTimestamp());
}

void TransactionServices::cleanupTransactionMessage(message::Transaction *transaction,
                                                    Session& session)
{
  delete transaction;
  session.setStatementMessage(NULL);
  session.setTransactionMessage(NULL);
  session.setXaId(0);
}

int TransactionServices::commitTransactionMessage(Session& session)
{
  if (! ReplicationServices::isActive())
    return 0;

  /*
   * If no Transaction message was ever created, then no data modification
   * occurred inside the transaction, so nothing to do.
   */
  if (session.getTransactionMessage() == NULL)
    return 0;
  
  /* If there is an active statement message, finalize it. */
  message::Statement *statement= session.getStatementMessage();

  if (statement != NULL)
  {
    finalizeStatementMessage(*statement, session);
  }

  message::Transaction* transaction= getActiveTransactionMessage(session);

  /*
   * It is possible that we could have a Transaction without any Statements
   * if we had created a Statement but had to roll it back due to it failing
   * mid-execution, and no subsequent Statements were added to the Transaction
   * message. In this case, we simply clean up the message and not push it.
   */
  if (transaction->statement_size() == 0)
  {
    cleanupTransactionMessage(transaction, session);
    return 0;
  }
  
  finalizeTransactionMessage(*transaction, session);
  
  plugin::ReplicationReturnCode result= ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);

  return static_cast<int>(result);
}

void TransactionServices::initStatementMessage(message::Statement &statement,
                                               message::Statement::Type type,
                                               const Session& session)
{
  statement.set_type(type);
  statement.set_start_timestamp(session.times.getCurrentTimestamp());

  if (session.variables.replicate_query)
    statement.set_sql(session.getQueryString()->c_str());
}

void TransactionServices::finalizeStatementMessage(message::Statement &statement,
                                                   Session& session)
{
  statement.set_end_timestamp(session.times.getCurrentTimestamp());
  session.setStatementMessage(NULL);
}

void TransactionServices::rollbackTransactionMessage(Session& session)
{
  if (! ReplicationServices::isActive())
    return;
  
  message::Transaction *transaction= getActiveTransactionMessage(session);

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
    uint32_t seg_id= transaction->segment_id();

    /*
     * Clear the transaction, create a Rollback statement message, 
     * attach it to the transaction, and push it to replicators.
     */
    transaction->Clear();
    initTransactionMessage(*transaction, session, false);

    /* Set the transaction ID to match the previous messages */
    transaction->mutable_transaction_context()->set_transaction_id(trx_id);
    transaction->set_segment_id(seg_id);
    transaction->set_end_segment(true);

    message::Statement *statement= transaction->add_statement();

    initStatementMessage(*statement, message::Statement::ROLLBACK, session);
    finalizeStatementMessage(*statement, session);

    finalizeTransactionMessage(*transaction, session);
    
    (void) ReplicationServices::pushTransactionMessage(session, *transaction);
  }

  cleanupTransactionMessage(transaction, session);
}

void TransactionServices::rollbackStatementMessage(Session& session)
{
  if (! ReplicationServices::isActive())
    return;

  message::Statement *current_statement= session.getStatementMessage();

  /* If we never added a Statement message, nothing to undo. */
  if (current_statement == NULL)
    return;

  /*
   * If the Statement has been segmented, then we've already pushed a portion
   * of this Statement's row changes through the replication stream and we
   * need to send a ROLLBACK_STATEMENT message. Otherwise, we can simply
   * delete the current Statement message.
   */
  bool is_segmented= false;

  switch (current_statement->type())
  {
    case message::Statement::INSERT:
      if (current_statement->insert_data().segment_id() > 1)
        is_segmented= true;
      break;

    case message::Statement::UPDATE:
      if (current_statement->update_data().segment_id() > 1)
        is_segmented= true;
      break;

    case message::Statement::DELETE:
      if (current_statement->delete_data().segment_id() > 1)
        is_segmented= true;
      break;

    default:
      break;
  }

  /*
   * Remove the Statement message we've been working with (same as
   * current_statement).
   */
  message::Transaction *transaction= getActiveTransactionMessage(session);
  google::protobuf::RepeatedPtrField<message::Statement> *statements_in_txn;
  statements_in_txn= transaction->mutable_statement();
  statements_in_txn->RemoveLast();
  session.setStatementMessage(NULL);
  
  /*
   * Create the ROLLBACK_STATEMENT message, if we need to. This serves as
   * an indicator to cancel the previous Statement message which should have
   * had its end_segment attribute set to false.
   */
  if (is_segmented)
  {
    current_statement= transaction->add_statement();
    initStatementMessage(*current_statement,
                         message::Statement::ROLLBACK_STATEMENT,
                         session);
    finalizeStatementMessage(*current_statement, session);
  }
}

message::Transaction *TransactionServices::segmentTransactionMessage(Session& session,
                                                                     message::Transaction *transaction)
{
  uint64_t trx_id= transaction->transaction_context().transaction_id();
  uint32_t seg_id= transaction->segment_id();
  
  transaction->set_end_segment(false);
  commitTransactionMessage(session);
  transaction= getActiveTransactionMessage(session, false);
  
  /* Set the transaction ID to match the previous messages */
  transaction->mutable_transaction_context()->set_transaction_id(trx_id);
  transaction->set_segment_id(seg_id + 1);
  transaction->set_end_segment(true);

  return transaction;
}

message::Statement &TransactionServices::getInsertStatement(Session& session,
                                                            Table &table,
                                                            uint32_t *next_segment_id)
{
  message::Statement *statement= session.getStatementMessage();
  message::Transaction *transaction= NULL;
  
  /*
   * If statement is NULL, this is a new statement.
   * If statement is NOT NULL, this a continuation of the same statement.
   * This is because autocommitOrRollback() finalizes the statement so that
   * we guarantee only one Statement message per statement (i.e., we no longer
   * share a single GPB message for multiple statements).
   */
  if (statement == NULL)
  {
    transaction= getActiveTransactionMessage(session);

    if (static_cast<size_t>(transaction->ByteSize()) >= 
        transaction_message_threshold)
    {
      transaction= segmentTransactionMessage(session, transaction);
    }

    statement= transaction->add_statement();
    setInsertHeader(*statement, session, table);
    session.setStatementMessage(statement);
  }
  else
  {
    transaction= getActiveTransactionMessage(session);
    
    /*
     * If we've passed our threshold for the statement size (possible for
     * a bulk insert), we'll finalize the Statement and Transaction (doing
     * the Transaction will keep it from getting huge).
     */
    if (static_cast<size_t>(transaction->ByteSize()) >= 
        transaction_message_threshold)
    {
      /* Remember the transaction ID so we can re-use it */
      uint64_t trx_id= transaction->transaction_context().transaction_id();
      uint32_t seg_id= transaction->segment_id();
      
      message::InsertData *current_data= statement->mutable_insert_data();
      
      /* Caller should use this value when adding a new record */
      *next_segment_id= current_data->segment_id() + 1;
      
      current_data->set_end_segment(false);
      transaction->set_end_segment(false);
      
      /* 
       * Send the trx message to replicators after finalizing the 
       * statement and transaction. This will also set the Transaction
       * and Statement objects in Session to NULL.
       */
      commitTransactionMessage(session);
      
      /*
       * Statement and Transaction should now be NULL, so new ones will get
       * created. We reuse the transaction id since we are segmenting
       * one transaction.
       */
      transaction= getActiveTransactionMessage(session, false);
      assert(transaction != NULL);

      statement= transaction->add_statement();
      setInsertHeader(*statement, session, table);
      session.setStatementMessage(statement);
            
      /* Set the transaction ID to match the previous messages */
      transaction->mutable_transaction_context()->set_transaction_id(trx_id);
      transaction->set_segment_id(seg_id + 1);
      transaction->set_end_segment(true);
    }
    else
    {
      /*
       * Continuation of the same statement. Carry forward the existing
       * segment id.
       */
      const message::InsertData &current_data= statement->insert_data();
      *next_segment_id= current_data.segment_id();
    }
  }
  
  return *statement;
}

void TransactionServices::setInsertHeader(message::Statement &statement,
                                          const Session& session,
                                          Table &table)
{
  initStatementMessage(statement, message::Statement::INSERT, session);

  /* 
   * Now we construct the specialized InsertHeader message inside
   * the generalized message::Statement container...
   */
  /* Set up the insert header */
  message::InsertHeader *header= statement.mutable_insert_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  string schema_name;
  (void) table.getShare()->getSchemaName(schema_name);
  string table_name;
  (void) table.getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  Field *current_field;
  Field **table_fields= table.getFields();

  message::FieldMetadata *field_metadata;

  /* We will read all the table's fields... */
  table.setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    field_metadata= header->add_field_metadata();
    field_metadata->set_name(current_field->field_name);
    field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
  }
}

bool TransactionServices::insertRecord(Session& session,
                                       Table &table)
{
  if (! ReplicationServices::isActive())
    return false;

  if (not table.getShare()->is_replicated())
    return false;

  /**
   * We do this check here because we don't want to even create a 
   * statement if there isn't a primary key on the table...
   *
   * @todo
   *
   * Multi-column primary keys are handled how exactly?
   */
  if (not table.getShare()->hasPrimaryKey())
  {
    my_error(ER_NO_PRIMARY_KEY_ON_REPLICATED_TABLE, MYF(0));
    return true;
  }

  uint32_t next_segment_id= 1;
  message::Statement &statement= getInsertStatement(session, table, &next_segment_id);

  message::InsertData *data= statement.mutable_insert_data();
  data->set_segment_id(next_segment_id);
  data->set_end_segment(true);
  message::InsertRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= table.getFields();

  String *string_value= new (session.mem_root) String(TransactionServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  /* We will read all the table's fields... */
  table.setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    if (current_field->is_null())
    {
      record->add_is_null(true);
      record->add_insert_value("", 0);
    } 
    else 
    {
      string_value= current_field->val_str_internal(string_value);
      record->add_is_null(false);
      record->add_insert_value(string_value->c_ptr(), string_value->length());
      string_value->free();
    }
  }
  return false;
}

message::Statement &TransactionServices::getUpdateStatement(Session& session,
                                                            Table &table,
                                                            const unsigned char *old_record, 
                                                            const unsigned char *new_record,
                                                            uint32_t *next_segment_id)
{
  message::Statement *statement= session.getStatementMessage();
  message::Transaction *transaction= NULL;

  /*
   * If statement is NULL, this is a new statement.
   * If statement is NOT NULL, this a continuation of the same statement.
   * This is because autocommitOrRollback() finalizes the statement so that
   * we guarantee only one Statement message per statement (i.e., we no longer
   * share a single GPB message for multiple statements).
   */
  if (statement == NULL)
  {
    transaction= getActiveTransactionMessage(session);
    
    if (static_cast<size_t>(transaction->ByteSize()) >= 
        transaction_message_threshold)
    {
      transaction= segmentTransactionMessage(session, transaction);
    }
    
    statement= transaction->add_statement();
    setUpdateHeader(*statement, session, table, old_record, new_record);
    session.setStatementMessage(statement);
  }
  else
  {
    transaction= getActiveTransactionMessage(session);
    
    /*
     * If we've passed our threshold for the statement size (possible for
     * a bulk insert), we'll finalize the Statement and Transaction (doing
     * the Transaction will keep it from getting huge).
     */
    if (static_cast<size_t>(transaction->ByteSize()) >= 
        transaction_message_threshold)
    {
      /* Remember the transaction ID so we can re-use it */
      uint64_t trx_id= transaction->transaction_context().transaction_id();
      uint32_t seg_id= transaction->segment_id();
      
      message::UpdateData *current_data= statement->mutable_update_data();
      
      /* Caller should use this value when adding a new record */
      *next_segment_id= current_data->segment_id() + 1;
      
      current_data->set_end_segment(false);
      transaction->set_end_segment(false);
      
      /* 
       * Send the trx message to replicators after finalizing the 
       * statement and transaction. This will also set the Transaction
       * and Statement objects in Session to NULL.
       */
      commitTransactionMessage(session);
      
      /*
       * Statement and Transaction should now be NULL, so new ones will get
       * created. We reuse the transaction id since we are segmenting
       * one transaction.
       */
      transaction= getActiveTransactionMessage(session, false);
      assert(transaction != NULL);
      
      statement= transaction->add_statement();
      setUpdateHeader(*statement, session, table, old_record, new_record);
      session.setStatementMessage(statement);
      
      /* Set the transaction ID to match the previous messages */
      transaction->mutable_transaction_context()->set_transaction_id(trx_id);
      transaction->set_segment_id(seg_id + 1);
      transaction->set_end_segment(true);
    }
    else
    {
      /*
       * Continuation of the same statement. Carry forward the existing
       * segment id.
       */
      const message::UpdateData &current_data= statement->update_data();
      *next_segment_id= current_data.segment_id();
    }
  }
  
  return *statement;
}

void TransactionServices::setUpdateHeader(message::Statement &statement,
                                          const Session& session,
                                          Table &table,
                                          const unsigned char *old_record, 
                                          const unsigned char *new_record)
{
  initStatementMessage(statement, message::Statement::UPDATE, session);

  /* 
   * Now we construct the specialized UpdateHeader message inside
   * the generalized message::Statement container...
   */
  /* Set up the update header */
  message::UpdateHeader *header= statement.mutable_update_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  string schema_name;
  (void) table.getShare()->getSchemaName(schema_name);
  string table_name;
  (void) table.getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  Field *current_field;
  Field **table_fields= table.getFields();

  message::FieldMetadata *field_metadata;

  /* We will read all the table's fields... */
  table.setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    /*
     * We add the "key field metadata" -- i.e. the fields which is
     * the primary key for the table.
     */
    if (table.getShare()->fieldInPrimaryKey(current_field))
    {
      field_metadata= header->add_key_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
    }

    if (isFieldUpdated(current_field, table, old_record, new_record))
    {
      /* Field is changed from old to new */
      field_metadata= header->add_set_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
    }
  }
}

void TransactionServices::updateRecord(Session& session,
                                       Table &table, 
                                       const unsigned char *old_record, 
                                       const unsigned char *new_record)
{
  if (! ReplicationServices::isActive())
    return;

  if (not table.getShare()->is_replicated())
    return;

  uint32_t next_segment_id= 1;
  message::Statement &statement= getUpdateStatement(session, table, old_record, new_record, &next_segment_id);

  message::UpdateData *data= statement.mutable_update_data();
  data->set_segment_id(next_segment_id);
  data->set_end_segment(true);
  message::UpdateRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= table.getFields();
  String *string_value= new (session.mem_root) String(TransactionServices::DEFAULT_RECORD_SIZE);
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
    if (isFieldUpdated(current_field, table, old_record, new_record))
    {
      /* Store the original "read bit" for this field */
      bool is_read_set= current_field->isReadSet();

      /* We need to mark that we will "read" this field... */
      table.setReadSet(current_field->position());

      /* Read the string value of this field's contents */
      string_value= current_field->val_str_internal(string_value);

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
    if (table.getShare()->fieldInPrimaryKey(current_field))
    {
      /**
       * To say the below is ugly is an understatement. But it works.
       * 
       * @todo Move this crap into a real Record API.
       */
      string_value= current_field->val_str_internal(string_value,
                                                    old_record + 
                                                    current_field->offset(const_cast<unsigned char *>(new_record)));
      record->add_key_value(string_value->c_ptr(), string_value->length());
      string_value->free();
    }

  }
}

bool TransactionServices::isFieldUpdated(Field *current_field,
                                         Table &table,
                                         const unsigned char *old_record,
                                         const unsigned char *new_record)
{
  /*
   * The below really should be moved into the Field API and Record API.  But for now
   * we do this crazy pointer fiddling to figure out if the current field
   * has been updated in the supplied record raw byte pointers.
   */
  const unsigned char *old_ptr= (const unsigned char *) old_record + (ptrdiff_t) (current_field->ptr - table.getInsertRecord());
  const unsigned char *new_ptr= (const unsigned char *) new_record + (ptrdiff_t) (current_field->ptr - table.getInsertRecord());

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

message::Statement &TransactionServices::getDeleteStatement(Session& session,
                                                            Table &table,
                                                            uint32_t *next_segment_id)
{
  message::Statement *statement= session.getStatementMessage();
  message::Transaction *transaction= NULL;

  /*
   * If statement is NULL, this is a new statement.
   * If statement is NOT NULL, this a continuation of the same statement.
   * This is because autocommitOrRollback() finalizes the statement so that
   * we guarantee only one Statement message per statement (i.e., we no longer
   * share a single GPB message for multiple statements).
   */
  if (statement == NULL)
  {
    transaction= getActiveTransactionMessage(session);
    
    if (static_cast<size_t>(transaction->ByteSize()) >= 
        transaction_message_threshold)
    {
      transaction= segmentTransactionMessage(session, transaction);
    }
    
    statement= transaction->add_statement();
    setDeleteHeader(*statement, session, table);
    session.setStatementMessage(statement);
  }
  else
  {
    transaction= getActiveTransactionMessage(session);
    
    /*
     * If we've passed our threshold for the statement size (possible for
     * a bulk insert), we'll finalize the Statement and Transaction (doing
     * the Transaction will keep it from getting huge).
     */
    if (static_cast<size_t>(transaction->ByteSize()) >= 
        transaction_message_threshold)
    {
      /* Remember the transaction ID so we can re-use it */
      uint64_t trx_id= transaction->transaction_context().transaction_id();
      uint32_t seg_id= transaction->segment_id();
      
      message::DeleteData *current_data= statement->mutable_delete_data();
      
      /* Caller should use this value when adding a new record */
      *next_segment_id= current_data->segment_id() + 1;
      
      current_data->set_end_segment(false);
      transaction->set_end_segment(false);
      
      /* 
       * Send the trx message to replicators after finalizing the 
       * statement and transaction. This will also set the Transaction
       * and Statement objects in Session to NULL.
       */
      commitTransactionMessage(session);
      
      /*
       * Statement and Transaction should now be NULL, so new ones will get
       * created. We reuse the transaction id since we are segmenting
       * one transaction.
       */
      transaction= getActiveTransactionMessage(session, false);
      assert(transaction != NULL);
      
      statement= transaction->add_statement();
      setDeleteHeader(*statement, session, table);
      session.setStatementMessage(statement);
      
      /* Set the transaction ID to match the previous messages */
      transaction->mutable_transaction_context()->set_transaction_id(trx_id);
      transaction->set_segment_id(seg_id + 1);
      transaction->set_end_segment(true);
    }
    else
    {
      /*
       * Continuation of the same statement. Carry forward the existing
       * segment id.
       */
      const message::DeleteData &current_data= statement->delete_data();
      *next_segment_id= current_data.segment_id();
    }
  }
  
  return *statement;
}

void TransactionServices::setDeleteHeader(message::Statement &statement,
                                          const Session& session,
                                          Table &table)
{
  initStatementMessage(statement, message::Statement::DELETE, session);

  /* 
   * Now we construct the specialized DeleteHeader message inside
   * the generalized message::Statement container...
   */
  message::DeleteHeader *header= statement.mutable_delete_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  string schema_name;
  (void) table.getShare()->getSchemaName(schema_name);
  string table_name;
  (void) table.getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  Field *current_field;
  Field **table_fields= table.getFields();

  message::FieldMetadata *field_metadata;

  while ((current_field= *table_fields++) != NULL) 
  {
    /* 
     * Add the WHERE clause values now...for now, this means the
     * primary key field value.  Replication only supports tables
     * with a primary key.
     */
    if (table.getShare()->fieldInPrimaryKey(current_field))
    {
      field_metadata= header->add_key_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(message::internalFieldTypeToFieldProtoType(current_field->type()));
    }
  }
}

void TransactionServices::deleteRecord(Session& session,
                                       Table &table,
                                       bool use_update_record)
{
  if (! ReplicationServices::isActive())
    return;

  if (not table.getShare()->is_replicated())
    return;

  uint32_t next_segment_id= 1;
  message::Statement &statement= getDeleteStatement(session, table, &next_segment_id);

  message::DeleteData *data= statement.mutable_delete_data();
  data->set_segment_id(next_segment_id);
  data->set_end_segment(true);
  message::DeleteRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= table.getFields();
  String *string_value= new (session.mem_root) String(TransactionServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  while ((current_field= *table_fields++) != NULL) 
  {
    /* 
     * Add the WHERE clause values now...for now, this means the
     * primary key field value.  Replication only supports tables
     * with a primary key.
     */
    if (table.getShare()->fieldInPrimaryKey(current_field))
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
        current_field->ptr= table.getUpdateRecord() + static_cast<ptrdiff_t>(old_ptr - table.getInsertRecord());
        string_value= current_field->val_str_internal(string_value);
        current_field->ptr= const_cast<unsigned char *>(old_ptr);
      }
      else
      {
        string_value= current_field->val_str_internal(string_value);
        /**
         * @TODO Store optional old record value in the before data member
         */
      }
      record->add_key_value(string_value->c_ptr(), string_value->length());
      string_value->free();
    }
  }
}

void TransactionServices::createTable(Session& session,
                                      const message::Table &table)
{
  if (not ReplicationServices::isActive())
    return;

  if (not message::is_replicated(table))
    return;

  message::Transaction *transaction= getActiveTransactionMessage(session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::CREATE_TABLE, session);

  /* 
   * Construct the specialized CreateTableStatement message and attach
   * it to the generic Statement message
   */
  message::CreateTableStatement *create_table_statement= statement->mutable_create_table_statement();
  message::Table *new_table_message= create_table_statement->mutable_table();
  *new_table_message= table;

  finalizeStatementMessage(*statement, session);

  finalizeTransactionMessage(*transaction, session);
  
  (void) ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);

}

void TransactionServices::createSchema(Session& session,
                                       const message::Schema &schema)
{
  if (! ReplicationServices::isActive())
    return;

  if (not message::is_replicated(schema))
    return;

  message::Transaction *transaction= getActiveTransactionMessage(session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::CREATE_SCHEMA, session);

  /* 
   * Construct the specialized CreateSchemaStatement message and attach
   * it to the generic Statement message
   */
  message::CreateSchemaStatement *create_schema_statement= statement->mutable_create_schema_statement();
  message::Schema *new_schema_message= create_schema_statement->mutable_schema();
  *new_schema_message= schema;

  finalizeStatementMessage(*statement, session);

  finalizeTransactionMessage(*transaction, session);
  
  (void) ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);

}

void TransactionServices::dropSchema(Session& session,
                                     const identifier::Schema& identifier,
                                     message::schema::const_reference schema)
{
  if (not ReplicationServices::isActive())
    return;

  if (not message::is_replicated(schema))
    return;

  message::Transaction *transaction= getActiveTransactionMessage(session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::DROP_SCHEMA, session);

  /* 
   * Construct the specialized DropSchemaStatement message and attach
   * it to the generic Statement message
   */
  message::DropSchemaStatement *drop_schema_statement= statement->mutable_drop_schema_statement();

  drop_schema_statement->set_schema_name(identifier.getSchemaName());

  finalizeStatementMessage(*statement, session);

  finalizeTransactionMessage(*transaction, session);
  
  (void) ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);
}

void TransactionServices::alterSchema(Session& session,
                                      const message::Schema &old_schema,
                                      const message::Schema &new_schema)
{
  if (! ReplicationServices::isActive())
    return;

  if (not message::is_replicated(old_schema))
    return;

  message::Transaction *transaction= getActiveTransactionMessage(session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::ALTER_SCHEMA, session);

  /* 
   * Construct the specialized AlterSchemaStatement message and attach
   * it to the generic Statement message
   */
  message::AlterSchemaStatement *alter_schema_statement= statement->mutable_alter_schema_statement();

  message::Schema *before= alter_schema_statement->mutable_before();
  message::Schema *after= alter_schema_statement->mutable_after();

  *before= old_schema;
  *after= new_schema;

  finalizeStatementMessage(*statement, session);

  finalizeTransactionMessage(*transaction, session);
  
  (void) ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);
}

void TransactionServices::dropTable(Session& session,
                                    const identifier::Table& identifier,
                                    message::table::const_reference table,
                                    bool if_exists)
{
  if (! ReplicationServices::isActive())
    return;

  if (not message::is_replicated(table))
    return;

  message::Transaction *transaction= getActiveTransactionMessage(session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::DROP_TABLE, session);

  /* 
   * Construct the specialized DropTableStatement message and attach
   * it to the generic Statement message
   */
  message::DropTableStatement *drop_table_statement= statement->mutable_drop_table_statement();

  drop_table_statement->set_if_exists_clause(if_exists);

  message::TableMetadata *table_metadata= drop_table_statement->mutable_table_metadata();

  table_metadata->set_schema_name(identifier.getSchemaName());
  table_metadata->set_table_name(identifier.getTableName());

  finalizeStatementMessage(*statement, session);

  finalizeTransactionMessage(*transaction, session);
  
  (void) ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);
}

void TransactionServices::truncateTable(Session& session, Table &table)
{
  if (! ReplicationServices::isActive())
    return;

  if (not table.getShare()->is_replicated())
    return;

  message::Transaction *transaction= getActiveTransactionMessage(session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::TRUNCATE_TABLE, session);

  /* 
   * Construct the specialized TruncateTableStatement message and attach
   * it to the generic Statement message
   */
  message::TruncateTableStatement *truncate_statement= statement->mutable_truncate_table_statement();
  message::TableMetadata *table_metadata= truncate_statement->mutable_table_metadata();

  string schema_name;
  (void) table.getShare()->getSchemaName(schema_name);

  string table_name;
  (void) table.getShare()->getTableName(table_name);

  table_metadata->set_schema_name(schema_name.c_str(), schema_name.length());
  table_metadata->set_table_name(table_name.c_str(), table_name.length());

  finalizeStatementMessage(*statement, session);

  finalizeTransactionMessage(*transaction, session);
  
  (void) ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);
}

void TransactionServices::rawStatement(Session& session,
                                       const string &query,
                                       const string &schema)
{
  if (! ReplicationServices::isActive())
    return;
 
  message::Transaction *transaction= getActiveTransactionMessage(session);
  message::Statement *statement= transaction->add_statement();

  initStatementMessage(*statement, message::Statement::RAW_SQL, session);
  statement->set_sql(query);
  if (not schema.empty())
    statement->set_raw_sql_schema(schema);
  finalizeStatementMessage(*statement, session);

  finalizeTransactionMessage(*transaction, session);
  
  (void) ReplicationServices::pushTransactionMessage(session, *transaction);

  cleanupTransactionMessage(transaction, session);
}

int TransactionServices::sendEvent(Session& session, const message::Event &event)
{
  if (not ReplicationServices::isActive())
    return 0;
  message::Transaction transaction;

  // set server id, start timestamp
  initTransactionMessage(transaction, session, true);

  // set end timestamp
  finalizeTransactionMessage(transaction, session);

  message::Event *trx_event= transaction.mutable_event();
  trx_event->CopyFrom(event);
  plugin::ReplicationReturnCode result= ReplicationServices::pushTransactionMessage(session, transaction);
  return result;
}

bool TransactionServices::sendStartupEvent(Session& session)
{
  message::Event event;
  event.set_type(message::Event::STARTUP);
  return not sendEvent(session, event);
}

bool TransactionServices::sendShutdownEvent(Session& session)
{
  message::Event event;
  event.set_type(message::Event::SHUTDOWN);
  return not sendEvent(session, event);
}

} /* namespace drizzled */
