/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (c) Jay Pipes <jaypipes@gmail.com>
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
#include "drizzled/plugin/xa_storage_engine.h"
#include "drizzled/internal/my_sys.h"

using namespace std;

#include <vector>
#include <algorithm>
#include <functional>

namespace drizzled
{

/**
  Transaction handling in the server
  ==================================

  In each client connection, MySQL maintains two transactional
  states:
  - a statement transaction,
  - a standard, also called normal transaction.

  Historical note
  ---------------
  "Statement transaction" is a non-standard term that comes
  from the times when MySQL supported BerkeleyDB storage engine.

  First of all, it should be said that in BerkeleyDB auto-commit
  mode auto-commits operations that are atomic to the storage
  engine itself, such as a write of a record, and are too
  high-granular to be atomic from the application perspective
  (MySQL). One SQL statement could involve many BerkeleyDB
  auto-committed operations and thus BerkeleyDB auto-commit was of
  little use to MySQL.

  Secondly, instead of SQL standard savepoints, BerkeleyDB
  provided the concept of "nested transactions". In a nutshell,
  transactions could be arbitrarily nested, but when the parent
  transaction was committed or aborted, all its child (nested)
  transactions were handled committed or aborted as well.
  Commit of a nested transaction, in turn, made its changes
  visible, but not durable: it destroyed the nested transaction,
  all its changes would become available to the parent and
  currently active nested transactions of this parent.

  So the mechanism of nested transactions was employed to
  provide "all or nothing" guarantee of SQL statements
  required by the standard.
  A nested transaction would be created at start of each SQL
  statement, and destroyed (committed or aborted) at statement
  end. Such nested transaction was internally referred to as
  a "statement transaction" and gave birth to the term.

  <Historical note ends>

  Since then a statement transaction is started for each statement
  that accesses transactional tables or uses the binary log.  If
  the statement succeeds, the statement transaction is committed.
  If the statement fails, the transaction is rolled back. Commits
  of statement transactions are not durable -- each such
  transaction is nested in the normal transaction, and if the
  normal transaction is rolled back, the effects of all enclosed
  statement transactions are undone as well.  Technically,
  a statement transaction can be viewed as a savepoint which is
  maintained automatically in order to make effects of one
  statement atomic.

  The normal transaction is started by the user and is ended
  usually upon a user request as well. The normal transaction
  encloses transactions of all statements issued between
  its beginning and its end.
  In autocommit mode, the normal transaction is equivalent
  to the statement transaction.

  Since MySQL supports PSEA (pluggable storage engine
  architecture), more than one transactional engine can be
  active at a time. Hence transactions, from the server
  point of view, are always distributed. In particular,
  transactional state is maintained independently for each
  engine. In order to commit a transaction the two phase
  commit protocol is employed.

  Not all statements are executed in context of a transaction.
  Administrative and status information statements do not modify
  engine data, and thus do not start a statement transaction and
  also have no effect on the normal transaction. Examples of such
  statements are SHOW STATUS and RESET SLAVE.

  Similarly DDL statements are not transactional,
  and therefore a transaction is [almost] never started for a DDL
  statement. The difference between a DDL statement and a purely
  administrative statement though is that a DDL statement always
  commits the current transaction before proceeding, if there is
  any.

  At last, SQL statements that work with non-transactional
  engines also have no effect on the transaction state of the
  connection. Even though they are written to the binary log,
  and the binary log is, overall, transactional, the writes
  are done in "write-through" mode, directly to the binlog
  file, followed with a OS cache sync, in other words,
  bypassing the binlog undo log (translog).
  They do not commit the current normal transaction.
  A failure of a statement that uses non-transactional tables
  would cause a rollback of the statement transaction, but
  in case there no non-transactional tables are used,
  no statement transaction is started.

  Data layout
  -----------

  The server stores its transaction-related data in
  session->transaction. This structure has two members of type
  TransactionContext. These members correspond to the statement and
  normal transactions respectively:

  - session->transaction.stmt contains a list of engines
  that are participating in the given statement
  - session->transaction.all contains a list of engines that
  have participated in any of the statement transactions started
  within the context of the normal transaction.
  Each element of the list contains a pointer to the storage
  engine, engine-specific transactional data, and engine-specific
  transaction flags.

  In autocommit mode session->transaction.all is empty.
  Instead, data of session->transaction.stmt is
  used to commit/rollback the normal transaction.

  The list of registered engines has a few important properties:
  - no engine is registered in the list twice
  - engines are present in the list a reverse temporal order --
  new participants are always added to the beginning of the list.

  Transaction life cycle
  ----------------------

  When a new connection is established, session->transaction
  members are initialized to an empty state.
  If a statement uses any tables, all affected engines
  are registered in the statement engine list. In
  non-autocommit mode, the same engines are registered in
  the normal transaction list.
  At the end of the statement, the server issues a commit
  or a roll back for all engines in the statement list.
  At this point transaction flags of an engine, if any, are
  propagated from the statement list to the list of the normal
  transaction.
  When commit/rollback is finished, the statement list is
  cleared. It will be filled in again by the next statement,
  and emptied again at the next statement's end.

  The normal transaction is committed in a similar way
  (by going over all engines in session->transaction.all list)
  but at different times:
  - upon COMMIT SQL statement is issued by the user
  - implicitly, by the server, at the beginning of a DDL statement
  or SET AUTOCOMMIT={0|1} statement.

  The normal transaction can be rolled back as well:
  - if the user has requested so, by issuing ROLLBACK SQL
  statement
  - if one of the storage engines requested a rollback
  by setting session->transaction_rollback_request. This may
  happen in case, e.g., when the transaction in the engine was
  chosen a victim of the internal deadlock resolution algorithm
  and rolled back internally. When such a situation happens, there
  is little the server can do and the only option is to rollback
  transactions in all other participating engines.  In this case
  the rollback is accompanied by an error sent to the user.

  As follows from the use cases above, the normal transaction
  is never committed when there is an outstanding statement
  transaction. In most cases there is no conflict, since
  commits of the normal transaction are issued by a stand-alone
  administrative or DDL statement, thus no outstanding statement
  transaction of the previous statement exists. Besides,
  all statements that manipulate with the normal transaction
  are prohibited in stored functions and triggers, therefore
  no conflicting situation can occur in a sub-statement either.
  The remaining rare cases when the server explicitly has
  to commit the statement transaction prior to committing the normal
  one cover error-handling scenarios (see for example
  ?).

  When committing a statement or a normal transaction, the server
  either uses the two-phase commit protocol, or issues a commit
  in each engine independently. The two-phase commit protocol
  is used only if:
  - all participating engines support two-phase commit (provide
    plugin::StorageEngine::prepare PSEA API call) and
  - transactions in at least two engines modify data (i.e. are
  not read-only).

  Note that the two phase commit is used for
  statement transactions, even though they are not durable anyway.
  This is done to ensure logical consistency of data in a multiple-
  engine transaction.
  For example, imagine that some day MySQL supports unique
  constraint checks deferred till the end of statement. In such
  case a commit in one of the engines may yield ER_DUP_KEY,
  and MySQL should be able to gracefully abort statement
  transactions of other participants.

  After the normal transaction has been committed,
  session->transaction.all list is cleared.

  When a connection is closed, the current normal transaction, if
  any is currently active, is rolled back.

  Roles and responsibilities
  --------------------------

  Beginning of SQL Statement (and Statement Transaction)
  ------------------------------------------------------

  At the start of each SQL statement, for each storage engine
  <strong>that is involved in the SQL statement</strong>, the kernel 
  calls the engine's plugin::StoragEngine::startStatement() method.  If the
  engine needs to track some data for the statement, it should use
  this method invocation to initialize this data.  This is the
  beginning of what is called the "statement transaction".

  <strong>For transaction storage engines (those storage engines
  that inherit from plugin::TransactionalStorageEngine)</strong>, the
  kernel automatically determines if the start of the SQL statement 
  transaction should <em>also</em> begin the normal SQL transaction.
  This occurs when the connection is in NOT in autocommit mode. If
  the kernel detects this, then the kernel automatically starts the
  normal transaction w/ plugin::TransactionalStorageEngine::startTransaction()
  method and then calls plugin::StorageEngine::startStatement()
  afterwards.

  Beginning of an SQL "Normal" Transaction
  ----------------------------------------

  As noted above, a "normal SQL transaction" may be started when
  an SQL statement is started in a connection and the connection is
  NOT in AUTOCOMMIT mode.  This is automatically done by the kernel.

  In addition, when a user executes a START TRANSACTION or
  BEGIN WORK statement in a connection, the kernel explicitly
  calls each transactional storage engine's startTransaction() method.

  Ending of an SQL Statement (and Statement Transaction)
  ------------------------------------------------------

  At the end of each SQL statement, for each of the aforementioned
  involved storage engines, the kernel calls the engine's
  plugin::StorageEngine::endStatement() method.  If the engine
  has initialized or modified some internal data about the
  statement transaction, it should use this method to reset or destroy
  this data appropriately.

  Ending of an SQL "Normal" Transaction
  -------------------------------------
  
  The end of a normal transaction is either a ROLLBACK or a COMMIT, 
  depending on the success or failure of the statement transaction(s) 
  it encloses.
  
  The end of a "normal transaction" occurs when any of the following
  occurs:

  1) If a statement transaction has completed and AUTOCOMMIT is ON,
     then the normal transaction which encloses the statement
     transaction ends
  2) If a COMMIT or ROLLBACK statement occurs on the connection
  3) Just before a DDL operation occurs, the kernel will implicitly
     commit the active normal transaction
  
  Transactions and Non-transactional Storage Engines
  --------------------------------------------------
  
  For non-transactional engines, this call can be safely ignored, and
  the kernel tracks whether a non-transactional engine has changed
  any data state, and warns the user appropriately if a transaction
  (statement or normal) is rolled back after such non-transactional
  data changes have been made.

  XA Two-phase Commit Protocol
  ----------------------------

  During statement execution, whenever any of data-modifying
  PSEA API methods is used, e.g. Cursor::write_row() or
  Cursor::update_row(), the read-write flag is raised in the
  statement transaction for the involved engine.
  Currently All PSEA calls are "traced", and the data can not be
  changed in a way other than issuing a PSEA call. Important:
  unless this invariant is preserved the server will not know that
  a transaction in a given engine is read-write and will not
  involve the two-phase commit protocol!

  At the end of a statement, server call
  ha_autocommit_or_rollback() is invoked. This call in turn
  invokes plugin::StorageEngine::prepare() for every involved engine.
  Prepare is followed by a call to plugin::StorageEngine::commit_one_phase()
  If a one-phase commit will suffice, plugin::StorageEngine::prepare() is not
  invoked and the server only calls plugin::StorageEngine::commit_one_phase().
  At statement commit, the statement-related read-write engine
  flag is propagated to the corresponding flag in the normal
  transaction.  When the commit is complete, the list of registered
  engines is cleared.

  Rollback is handled in a similar fashion.

  Additional notes on DDL and the normal transaction.
  ---------------------------------------------------

  CREATE TABLE .. SELECT can start a *new* normal transaction
  because of the fact that SELECTs on a transactional storage
  engine participate in the normal SQL transaction (due to
  isolation level issues and consistent read views).

  Behaviour of the server in this case is currently badly
  defined.

  DDL statements use a form of "semantic" logging
  to maintain atomicity: if CREATE TABLE .. SELECT failed,
  the newly created table is deleted.

  In addition, some DDL statements issue interim transaction
  commits: e.g. ALTER TABLE issues a COMMIT after data is copied
  from the original table to the internal temporary table. Other
  statements, e.g. CREATE TABLE ... SELECT do not always commit
  after itself.

  And finally there is a group of DDL statements such as
  RENAME/DROP TABLE that doesn't start a new transaction
  and doesn't commit.

  A consistent behaviour is perhaps to always commit the normal
  transaction after all DDLs, just like the statement transaction
  is always committed at the end of all statements.
*/
void TransactionServices::registerResourceForStatement(Session *session,
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
    registerResourceForTransaction(session, engine);
  }

  TransactionContext *trans= &session->transaction.stmt;
  ResourceContext *resource_context= session->getResourceContext(engine, 0);

  if (resource_context->isStarted())
    return; /* already registered, return */

  resource_context->setResource(engine);
  trans->registerResource(resource_context);

  trans->no_2pc|= not engine->hasTwoPhaseCommit();
}

void TransactionServices::registerResourceForTransaction(Session *session,
                                                         plugin::TransactionalStorageEngine *engine)
{
  TransactionContext *trans= &session->transaction.all;
  ResourceContext *resource_context= session->getResourceContext(engine, 1);

  if (resource_context->isStarted())
    return; /* already registered, return */

  session->server_status|= SERVER_STATUS_IN_TRANS;

  resource_context->setResource(engine);
  trans->registerResource(resource_context);

  trans->no_2pc|= not engine->hasTwoPhaseCommit();

  if (session->transaction.xid_state.xid.is_null())
    session->transaction.xid_state.xid.set(session->getQueryId());

  /* Only true if user is executing a BEGIN WORK/START TRANSACTION */
  if (! session->getResourceContext(engine, 0)->isStarted())
    registerResourceForStatement(session, engine);
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
      ResourceContext *resource_context_normal= session->getResourceContext(resource_context->getResource(), true);
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

        plugin::StorageEngine *engine= resource_context->getResource();
        if ((err= static_cast<plugin::XaStorageEngine *>(engine)->prepare(session, normal_transaction)))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          error= 1;
        }
        status_var_increment(session->status_var.ha_prepare_count);
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

      plugin::TransactionalStorageEngine *engine= static_cast<plugin::TransactionalStorageEngine *>(resource_context->getResource());
      if ((err= engine->commit(session, normal_transaction)))
      {
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
        error= 1;
      }
      status_var_increment(session->status_var.ha_commit_count);
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

      plugin::TransactionalStorageEngine *engine= static_cast<plugin::TransactionalStorageEngine *>(resource_context->getResource());
      if ((err= engine->rollback(session, normal_transaction)))
      {
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error=1;
      }
      status_var_increment(session->status_var.ha_rollback_count);
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
    return lhs->getResource()->getSlot() < rhs->getResource()->getSlot();
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
    plugin::TransactionalStorageEngine *engine= static_cast<plugin::TransactionalStorageEngine *>(resource_context->getResource());
    assert(engine != NULL);
    if ((err= engine->rollbackToSavepoint(session, sv)))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error= 1;
    }
    status_var_increment(session->status_var.ha_savepoint_rollback_count);
    trans->no_2pc|= not engine->hasTwoPhaseCommit();
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
      plugin::TransactionalStorageEngine *engine= static_cast<plugin::TransactionalStorageEngine *>(resource_context->getResource());
      if ((err= engine->rollback(session, !(0))))
      { // cannot happen
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error= 1;
      }
      status_var_increment(session->status_var.ha_rollback_count);
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
      plugin::TransactionalStorageEngine *engine= static_cast<plugin::TransactionalStorageEngine *>(resource_context->getResource());
      assert(engine);
      if ((err= engine->setSavepoint(session, sv)))
      { // cannot happen
        my_error(ER_GET_ERRNO, MYF(0), err);
        error= 1;
      }
      status_var_increment(session->status_var.ha_savepoint_count);
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
    plugin::TransactionalStorageEngine *engine= static_cast<plugin::TransactionalStorageEngine *>(resource_context->getResource());
    assert(engine);
    if ((err= engine->releaseSavepoint(session, sv)))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error= 1;
    }
  }
  return error;
}

} /* namespace drizzled */
