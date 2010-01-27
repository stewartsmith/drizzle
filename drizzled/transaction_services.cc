/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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
#include "drizzled/lock.h"
#include "drizzled/item/int.h"
#include "drizzled/item/empty_string.h"
#include "drizzled/field/timestamp.h"
#include "drizzled/plugin/client.h"
#include "drizzled/internal/my_sys.h"

using namespace std;

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
  Session_TRANS. These members correspond to the statement and
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
  any, is rolled back.

  Roles and responsibilities
  --------------------------

  The server has no way to know that an engine participates in
  the statement and a transaction has been started
  in it unless the engine says so. Thus, in order to be
  a part of a transaction, the engine must "register" itself.
  This is done by invoking trans_register_ha() server call.
  Normally the engine registers itself whenever Cursor::external_lock()
  is called. trans_register_ha() can be invoked many times: if
  an engine is already registered, the call does nothing.
  In case autocommit is not set, the engine must register itself
  twice -- both in the statement list and in the normal transaction
  list.
  In which list to register is a parameter of trans_register_ha().

  Note, that although the registration interface in itself is
  fairly clear, the current usage practice often leads to undesired
  effects. E.g. since a call to trans_register_ha() in most engines
  is embedded into implementation of Cursor::external_lock(), some
  DDL statements start a transaction (at least from the server
  point of view) even though they are not expected to. E.g.
  CREATE TABLE does not start a transaction, since
  Cursor::external_lock() is never called during CREATE TABLE. But
  CREATE TABLE ... SELECT does, since Cursor::external_lock() is
  called for the table that is being selected from. This has no
  practical effects currently, but must be kept in mind
  nevertheless.

  Once an engine is registered, the server will do the rest
  of the work.

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

  DDLs and operations with non-transactional engines
  do not "register" in session->transaction lists, and thus do not
  modify the transaction state. Besides, each DDL in
  MySQL is prefixed with an implicit normal transaction commit
  (a call to Session::endActiveTransaction()), and thus leaves nothing
  to modify.
  However, as it has been pointed out with CREATE TABLE .. SELECT,
  some DDL statements can start a *new* transaction.

  Behaviour of the server in this case is currently badly
  defined.
  DDL statements use a form of "semantic" logging
  to maintain atomicity: if CREATE TABLE .. SELECT failed,
  the newly created table is deleted.
  In addition, some DDL statements issue interim transaction
  commits: e.g. ALTER Table issues a commit after data is copied
  from the original table to the internal temporary table. Other
  statements, e.g. CREATE TABLE ... SELECT do not always commit
  after itself.
  And finally there is a group of DDL statements such as
  RENAME/DROP Table that doesn't start a new transaction
  and doesn't commit.

  This diversity makes it hard to say what will happen if
  by chance a stored function is invoked during a DDL --
  whether any modifications it makes will be committed or not
  is not clear. Fortunately, SQL grammar of few DDLs allows
  invocation of a stored function.

  A consistent behaviour is perhaps to always commit the normal
  transaction after all DDLs, just like the statement transaction
  is always committed at the end of all statements.
*/

/**
  Register a storage engine for a transaction.

  Every storage engine MUST call this function when it starts
  a transaction or a statement (that is it must be called both for the
  "beginning of transaction" and "beginning of statement").
  Only storage engines registered for the transaction/statement
  will know when to commit/rollback it.

  @note
    trans_register_ha is idempotent - storage engine may register many
    times per transaction.

*/
void TransactionServices::trans_register_ha(Session *session, bool all, plugin::StorageEngine *engine)
{
  Session_TRANS *trans;
  Ha_trx_info *ha_info;

  if (all)
  {
    trans= &session->transaction.all;
    session->server_status|= SERVER_STATUS_IN_TRANS;
  }
  else
    trans= &session->transaction.stmt;

  ha_info= session->getEngineInfo(engine, all ? 1 : 0);

  if (ha_info->is_started())
    return; /* already registered, return */

  ha_info->register_ha(trans, engine);

  trans->no_2pc|= not engine->has_2pc();
  if (session->transaction.xid_state.xid.is_null())
    session->transaction.xid_state.xid.set(session->getQueryId());
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
ha_check_and_coalesce_trx_read_only(Session *session, Ha_trx_info *ha_list,
                                    bool all)
{
  /* The number of storage engines that have actual changes. */
  unsigned rw_ha_count= 0;
  Ha_trx_info *ha_info;

  for (ha_info= ha_list; ha_info; ha_info= ha_info->next())
  {
    if (ha_info->is_trx_read_write())
      ++rw_ha_count;

    if (! all)
    {
      Ha_trx_info *ha_info_all= session->getEngineInfo(ha_info->engine(), 1);
      assert(ha_info != ha_info_all);
      /*
        Merge read-only/read-write information about statement
        transaction to its enclosing normal transaction. Do this
        only if in a real transaction -- that is, if we know
        that ha_info_all is registered in session->transaction.all.
        Since otherwise we only clutter the normal transaction flags.
      */
      if (ha_info_all->is_started()) /* false if autocommit. */
        ha_info_all->coalesce_trx_with(ha_info);
    }
    else if (rw_ha_count > 1)
    {
      /*
        It is a normal transaction, so we don't need to merge read/write
        information up, and the need for two-phase commit has been
        already established. Break the loop prematurely.
      */
      break;
    }
  }
  return rw_ha_count > 1;
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
int TransactionServices::ha_commit_trans(Session *session, bool all)
{
  int error= 0, cookie= 0;
  /*
    'all' means that this is either an explicit commit issued by
    user, or an implicit commit issued by a DDL.
  */
  Session_TRANS *trans= all ? &session->transaction.all : &session->transaction.stmt;
  bool is_real_trans= all || session->transaction.all.ha_list == 0;
  Ha_trx_info *ha_info= trans->ha_list;

  /*
    We must not commit the normal transaction if a statement
    transaction is pending. Otherwise statement transaction
    flags will not get propagated to its normal transaction's
    counterpart.
  */
  assert(session->transaction.stmt.ha_list == NULL ||
              trans == &session->transaction.stmt);

  if (ha_info)
  {
    bool must_2pc;

    if (is_real_trans && wait_if_global_read_lock(session, 0, 0))
    {
      ha_rollback_trans(session, all);
      return 1;
    }

    must_2pc= ha_check_and_coalesce_trx_read_only(session, ha_info, all);

    if (!trans->no_2pc && must_2pc)
    {
      for (; ha_info && !error; ha_info= ha_info->next())
      {
        int err;
        plugin::StorageEngine *engine= ha_info->engine();
        /*
          Do not call two-phase commit if this particular
          transaction is read-only. This allows for simpler
          implementation in engines that are always read-only.
        */
        if (! ha_info->is_trx_read_write())
          continue;
        /*
          Sic: we know that prepare() is not NULL since otherwise
          trans->no_2pc would have been set.
        */
        if ((err= engine->prepare(session, all)))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          error= 1;
        }
        status_var_increment(session->status_var.ha_prepare_count);
      }
      if (error)
      {
        ha_rollback_trans(session, all);
        error= 1;
        goto end;
      }
    }
    error=ha_commit_one_phase(session, all) ? (cookie ? 2 : 1) : 0;
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
int TransactionServices::ha_commit_one_phase(Session *session, bool all)
{
  int error=0;
  Session_TRANS *trans=all ? &session->transaction.all : &session->transaction.stmt;
  bool is_real_trans=all || session->transaction.all.ha_list == 0;
  Ha_trx_info *ha_info= trans->ha_list, *ha_info_next;
  if (ha_info)
  {
    for (; ha_info; ha_info= ha_info_next)
    {
      int err;
      plugin::StorageEngine *engine= ha_info->engine();
      if ((err= engine->commit(session, all)))
      {
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
        error=1;
      }
      status_var_increment(session->status_var.ha_commit_count);
      ha_info_next= ha_info->next();
      ha_info->reset(); /* keep it conveniently zero-filled */
    }
    trans->ha_list= 0;
    trans->no_2pc=0;
    if (is_real_trans)
      session->transaction.xid_state.xid.null();
    if (all)
    {
      session->variables.tx_isolation=session->session_tx_isolation;
      session->transaction.cleanup();
    }
  }
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


int TransactionServices::ha_rollback_trans(Session *session, bool all)
{
  int error=0;
  Session_TRANS *trans=all ? &session->transaction.all : &session->transaction.stmt;
  Ha_trx_info *ha_info= trans->ha_list, *ha_info_next;
  bool is_real_trans=all || session->transaction.all.ha_list == 0;

  /*
    We must not rollback the normal transaction if a statement
    transaction is pending.
  */
  assert(session->transaction.stmt.ha_list == NULL ||
              trans == &session->transaction.stmt);

  if (ha_info)
  {
    for (; ha_info; ha_info= ha_info_next)
    {
      int err;
      plugin::StorageEngine *engine= ha_info->engine();
      if ((err= engine->rollback(session, all)))
      { // cannot happen
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error=1;
      }
      status_var_increment(session->status_var.ha_rollback_count);
      ha_info_next= ha_info->next();
      ha_info->reset(); /* keep it conveniently zero-filled */
    }
    trans->ha_list= 0;
    trans->no_2pc=0;
    
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
    if (all)
    {
      session->variables.tx_isolation=session->session_tx_isolation;
      session->transaction.cleanup();
    }
  }
  if (all)
    session->transaction_rollback_request= false;

  /*
    If a non-transactional table was updated, warn; don't warn if this is a
    slave thread (because when a slave thread executes a ROLLBACK, it has
    been read from the binary log, so it's 100% sure and normal to produce
    error ER_WARNING_NOT_COMPLETE_ROLLBACK. If we sent the warning to the
    slave SQL thread, it would not stop the thread but just be printed in
    the error log; but we don't want users to wonder why they have this
    message in the error log, so we don't send it.
  */
  if (is_real_trans && session->transaction.all.modified_non_trans_table && session->killed != Session::KILL_CONNECTION)
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
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
  if (session->transaction.stmt.ha_list)
  {
    if (!error)
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

    session->variables.tx_isolation=session->session_tx_isolation;
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
  field_list.push_back(new Item_empty_string("data",XIDDATASIZE));

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


int TransactionServices::ha_rollback_to_savepoint(Session *session, SAVEPOINT *sv)
{
  int error= 0;
  Session_TRANS *trans= &session->transaction.all;
  Ha_trx_info *ha_info, *ha_info_next;

  trans->no_2pc=0;
  /*
    rolling back to savepoint in all storage engines that were part of the
    transaction when the savepoint was set
  */
  for (ha_info= sv->ha_list; ha_info; ha_info= ha_info->next())
  {
    int err;
    plugin::StorageEngine *engine= ha_info->engine();
    assert(engine);
    if ((err= engine->savepoint_rollback(session,
                                         (void *)(sv+1))))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error= 1;
    }
    status_var_increment(session->status_var.ha_savepoint_rollback_count);
    trans->no_2pc|= not engine->has_2pc();
  }
  /*
    rolling back the transaction in all storage engines that were not part of
    the transaction when the savepoint was set
  */
  for (ha_info= trans->ha_list; ha_info != sv->ha_list;
       ha_info= ha_info_next)
  {
    int err;
    plugin::StorageEngine *engine= ha_info->engine();
    if ((err= engine->rollback(session, !(0))))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error= 1;
    }
    status_var_increment(session->status_var.ha_rollback_count);
    ha_info_next= ha_info->next();
    ha_info->reset(); /* keep it conveniently zero-filled */
  }
  trans->ha_list= sv->ha_list;
  return error;
}

/**
  @note
  according to the sql standard (ISO/IEC 9075-2:2003)
  section "4.33.4 SQL-statements and transaction states",
  SAVEPOINT is *not* transaction-initiating SQL-statement
*/
int TransactionServices::ha_savepoint(Session *session, SAVEPOINT *sv)
{
  int error= 0;
  Session_TRANS *trans= &session->transaction.all;
  Ha_trx_info *ha_info= trans->ha_list;
  for (; ha_info; ha_info= ha_info->next())
  {
    int err;
    plugin::StorageEngine *engine= ha_info->engine();
    assert(engine);
#ifdef NOT_IMPLEMENTED /*- TODO (examine this againt the original code base) */
    if (! engine->savepoint_set)
    {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "SAVEPOINT");
      error= 1;
      break;
    } 
#endif
    if ((err= engine->savepoint_set(session, (void *)(sv+1))))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error= 1;
    }
    status_var_increment(session->status_var.ha_savepoint_count);
  }
  /*
    Remember the list of registered storage engines. All new
    engines are prepended to the beginning of the list.
  */
  sv->ha_list= trans->ha_list;
  return error;
}

int TransactionServices::ha_release_savepoint(Session *session, SAVEPOINT *sv)
{
  int error= 0;
  Ha_trx_info *ha_info= sv->ha_list;

  for (; ha_info; ha_info= ha_info->next())
  {
    int err;
    plugin::StorageEngine *engine= ha_info->engine();
    /* Savepoint life time is enclosed into transaction life time. */
    assert(engine);
    if ((err= engine->savepoint_release(session,
                                        (void *)(sv+1))))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error= 1;
    }
  }
  return error;
}

} /* namespace drizzled */
