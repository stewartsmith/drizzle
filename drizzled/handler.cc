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
  @file handler.cc

  Handler-calling-functions
*/

#include "drizzled/server_includes.h"
#include "mysys/hash.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"
#include "drizzled/data_home.h"
#include "drizzled/probes.h"
#include "drizzled/sql_parse.h"
#include "drizzled/cost_vect.h"
#include "drizzled/session.h"
#include "drizzled/sql_base.h"
#include "drizzled/transaction_services.h"
#include "drizzled/lock.h"
#include "drizzled/item/int.h"
#include "drizzled/item/empty_string.h"
#include "drizzled/unireg.h" // for mysql_frm_type
#include "drizzled/field/timestamp.h"
#include "drizzled/message/table.pb.h"

using namespace std;

extern drizzled::TransactionServices transaction_services;

KEY_CREATE_INFO default_key_create_info= { HA_KEY_ALG_UNDEF, 0, {NULL,0}, {NULL,0} };

/* number of entries in storage_engines[] */
uint32_t total_ha= 0;
/* number of storage engines (from storage_engines[]) that support 2pc */
uint32_t total_ha_2pc= 0;
/* size of savepoint storage area (see ha_init) */
uint32_t savepoint_alloc_size= 0;

const char *ha_row_type[] = {
  "", "FIXED", "DYNAMIC", "COMPRESSED", "REDUNDANT", "COMPACT", "PAGE", "?","?","?"
};

const char *tx_isolation_names[] =
{ "READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE",
  NULL};

TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names)-1,"",
                               tx_isolation_names, NULL};


/**
  Register handler error messages for use with my_error().

  @retval
    0           OK
  @retval
    !=0         Error
*/

int ha_init_errors(void)
{
#define SETMSG(nr, msg) errmsgs[(nr) - HA_ERR_FIRST]= (msg)
  const char    **errmsgs;

  /* Allocate a pointer array for the error message strings. */
  /* Zerofill it to avoid uninitialized gaps. */
  if (! (errmsgs= (const char**) malloc(HA_ERR_ERRORS * sizeof(char*))))
    return 1;
  memset(errmsgs, 0, HA_ERR_ERRORS * sizeof(char *));

  /* Set the dedicated error messages. */
  SETMSG(HA_ERR_KEY_NOT_FOUND,          ER(ER_KEY_NOT_FOUND));
  SETMSG(HA_ERR_FOUND_DUPP_KEY,         ER(ER_DUP_KEY));
  SETMSG(HA_ERR_RECORD_CHANGED,         "Update wich is recoverable");
  SETMSG(HA_ERR_WRONG_INDEX,            "Wrong index given to function");
  SETMSG(HA_ERR_CRASHED,                ER(ER_NOT_KEYFILE));
  SETMSG(HA_ERR_WRONG_IN_RECORD,        ER(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_OUT_OF_MEM,             "Table handler out of memory");
  SETMSG(HA_ERR_NOT_A_TABLE,            "Incorrect file format '%.64s'");
  SETMSG(HA_ERR_WRONG_COMMAND,          "Command not supported");
  SETMSG(HA_ERR_OLD_FILE,               ER(ER_OLD_KEYFILE));
  SETMSG(HA_ERR_NO_ACTIVE_RECORD,       "No record read in update");
  SETMSG(HA_ERR_RECORD_DELETED,         "Intern record deleted");
  SETMSG(HA_ERR_RECORD_FILE_FULL,       ER(ER_RECORD_FILE_FULL));
  SETMSG(HA_ERR_INDEX_FILE_FULL,        "No more room in index file '%.64s'");
  SETMSG(HA_ERR_END_OF_FILE,            "End in next/prev/first/last");
  SETMSG(HA_ERR_UNSUPPORTED,            ER(ER_ILLEGAL_HA));
  SETMSG(HA_ERR_TO_BIG_ROW,             "Too big row");
  SETMSG(HA_WRONG_CREATE_OPTION,        "Wrong create option");
  SETMSG(HA_ERR_FOUND_DUPP_UNIQUE,      ER(ER_DUP_UNIQUE));
  SETMSG(HA_ERR_UNKNOWN_CHARSET,        "Can't open charset");
  SETMSG(HA_ERR_WRONG_MRG_TABLE_DEF,    ER(ER_WRONG_MRG_TABLE));
  SETMSG(HA_ERR_CRASHED_ON_REPAIR,      ER(ER_CRASHED_ON_REPAIR));
  SETMSG(HA_ERR_CRASHED_ON_USAGE,       ER(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_LOCK_WAIT_TIMEOUT,      ER(ER_LOCK_WAIT_TIMEOUT));
  SETMSG(HA_ERR_LOCK_TABLE_FULL,        ER(ER_LOCK_TABLE_FULL));
  SETMSG(HA_ERR_READ_ONLY_TRANSACTION,  ER(ER_READ_ONLY_TRANSACTION));
  SETMSG(HA_ERR_LOCK_DEADLOCK,          ER(ER_LOCK_DEADLOCK));
  SETMSG(HA_ERR_CANNOT_ADD_FOREIGN,     ER(ER_CANNOT_ADD_FOREIGN));
  SETMSG(HA_ERR_NO_REFERENCED_ROW,      ER(ER_NO_REFERENCED_ROW_2));
  SETMSG(HA_ERR_ROW_IS_REFERENCED,      ER(ER_ROW_IS_REFERENCED_2));
  SETMSG(HA_ERR_NO_SAVEPOINT,           "No savepoint with that name");
  SETMSG(HA_ERR_NON_UNIQUE_BLOCK_SIZE,  "Non unique key block size");
  SETMSG(HA_ERR_NO_SUCH_TABLE,          "No such table: '%.64s'");
  SETMSG(HA_ERR_TABLE_EXIST,            ER(ER_TABLE_EXISTS_ERROR));
  SETMSG(HA_ERR_NO_CONNECTION,          "Could not connect to storage engine");
  SETMSG(HA_ERR_TABLE_DEF_CHANGED,      ER(ER_TABLE_DEF_CHANGED));
  SETMSG(HA_ERR_FOREIGN_DUPLICATE_KEY,  "FK constraint would lead to duplicate key");
  SETMSG(HA_ERR_TABLE_NEEDS_UPGRADE,    ER(ER_TABLE_NEEDS_UPGRADE));
  SETMSG(HA_ERR_TABLE_READONLY,         ER(ER_OPEN_AS_READONLY));
  SETMSG(HA_ERR_AUTOINC_READ_FAILED,    ER(ER_AUTOINC_READ_FAILED));
  SETMSG(HA_ERR_AUTOINC_ERANGE,         ER(ER_WARN_DATA_OUT_OF_RANGE));

  /* Register the error messages for use with my_error(). */
  return my_error_register(errmsgs, HA_ERR_FIRST, HA_ERR_LAST);
}


/**
  Unregister handler error messages.

  @retval
    0           OK
  @retval
    !=0         Error
*/
static int ha_finish_errors(void)
{
  const char    **errmsgs;

  /* Allocate a pointer array for the error message strings. */
  if (! (errmsgs= my_error_unregister(HA_ERR_FIRST, HA_ERR_LAST)))
    return 1;
  free((unsigned char*) errmsgs);
  return 0;
}

int ha_init()
{
  int error= 0;

  assert(total_ha < MAX_HA);
  /*
    Check if there is a transaction-capable storage engine besides the
    binary log (which is considered a transaction-capable storage engine in
    counting total_ha)
  */
  savepoint_alloc_size+= sizeof(SAVEPOINT);
  return(error);
}

int ha_end()
{
  int error= 0;

  /*
    This should be eventualy based  on the graceful shutdown flag.
    So if flag is equal to HA_PANIC_CLOSE, the deallocate
    the errors.
  */
  if (ha_finish_errors())
    error= 1;

  return(error);
}



/* ========================================================================
 ======================= TRANSACTIONS ===================================*/

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
  SQLCOM_LOCK_TABLES).

  When committing a statement or a normal transaction, the server
  either uses the two-phase commit protocol, or issues a commit
  in each engine independently. The two-phase commit protocol
  is used only if:
  - all participating engines support two-phase commit (provide
    StorageEngine::prepare PSEA API call) and
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
  Normally the engine registers itself whenever handler::external_lock()
  is called. trans_register_ha() can be invoked many times: if
  an engine is already registered, the call does nothing.
  In case autocommit is not set, the engine must register itself
  twice -- both in the statement list and in the normal transaction
  list.
  In which list to register is a parameter of trans_register_ha().

  Note, that although the registration interface in itself is
  fairly clear, the current usage practice often leads to undesired
  effects. E.g. since a call to trans_register_ha() in most engines
  is embedded into implementation of handler::external_lock(), some
  DDL statements start a transaction (at least from the server
  point of view) even though they are not expected to. E.g.
  CREATE TABLE does not start a transaction, since
  handler::external_lock() is never called during CREATE TABLE. But
  CREATE TABLE ... SELECT does, since handler::external_lock() is
  called for the table that is being selected from. This has no
  practical effects currently, but must be kept in mind
  nevertheless.

  Once an engine is registered, the server will do the rest
  of the work.

  During statement execution, whenever any of data-modifying
  PSEA API methods is used, e.g. handler::write_row() or
  handler::update_row(), the read-write flag is raised in the
  statement transaction for the involved engine.
  Currently All PSEA calls are "traced", and the data can not be
  changed in a way other than issuing a PSEA call. Important:
  unless this invariant is preserved the server will not know that
  a transaction in a given engine is read-write and will not
  involve the two-phase commit protocol!

  At the end of a statement, server call
  ha_autocommit_or_rollback() is invoked. This call in turn
  invokes StorageEngine::prepare() for every involved engine.
  Prepare is followed by a call to StorageEngine::commit_one_phase()
  If a one-phase commit will suffice, StorageEngine::prepare() is not
  invoked and the server only calls StorageEngine::commit_one_phase().
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
void trans_register_ha(Session *session, bool all, StorageEngine *engine)
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

  ha_info= session->ha_data[engine->slot].ha_info + static_cast<unsigned>(all);

  if (ha_info->is_started())
    return; /* already registered, return */

  ha_info->register_ha(trans, engine);

  trans->no_2pc|= not engine->has_2pc();
  if (session->transaction.xid_state.xid.is_null())
    session->transaction.xid_state.xid.set(session->query_id);

  return;
}

/**
  @retval
    0   ok
  @retval
    1   error, transaction was rolled back
*/
int ha_prepare(Session *session)
{
  int error=0, all=1;
  Session_TRANS *trans=all ? &session->transaction.all : &session->transaction.stmt;
  Ha_trx_info *ha_info= trans->ha_list;
  if (ha_info)
  {
    for (; ha_info; ha_info= ha_info->next())
    {
      int err;
      StorageEngine *engine= ha_info->engine();
      status_var_increment(session->status_var.ha_prepare_count);
      if ((err= engine->prepare(session, all)))
      {
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
        ha_rollback_trans(session, all);
        error=1;
        break;
      }
      else
      {
        push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                            engine->getName().c_str());
      }
    }
  }
  return(error);
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
      Ha_trx_info *ha_info_all= &session->ha_data[ha_info->engine()->slot].ha_info[1];
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
int ha_commit_trans(Session *session, bool all)
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
      return(1);
    }

    must_2pc= ha_check_and_coalesce_trx_read_only(session, ha_info, all);

    if (!trans->no_2pc && must_2pc)
    {
      for (; ha_info && !error; ha_info= ha_info->next())
      {
        int err;
        StorageEngine *engine= ha_info->engine();
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
  return(error);
}

/**
  @note
  This function does not care about global read lock. A caller should.
*/
int ha_commit_one_phase(Session *session, bool all)
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
      StorageEngine *engine= ha_info->engine();
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
  return(error);
}


int ha_rollback_trans(Session *session, bool all)
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
      StorageEngine *engine= ha_info->engine();
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
  return(error);
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
int ha_autocommit_or_rollback(Session *session, int error)
{
  if (session->transaction.stmt.ha_list)
  {
    if (!error)
    {
      if (ha_commit_trans(session, 0))
        error=1;
    }
    else
    {
      (void) ha_rollback_trans(session, 0);
      if (session->transaction_rollback_request)
        (void) ha_rollback(session);
    }

    session->variables.tx_isolation=session->session_tx_isolation;
  }
  return(error);
}




/**
  return the list of XID's to a client, the same way SHOW commands do.

  @note
    I didn't find in XA specs that an RM cannot return the same XID twice,
    so mysql_xa_recover does not filter XID's to ensure uniqueness.
    It can be easily fixed later, if necessary.
*/
bool mysql_xa_recover(Session *session)
{
  List<Item> field_list;
  Protocol *protocol= session->protocol;
  int i=0;
  XID_STATE *xs;

  field_list.push_back(new Item_int("formatID", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int("gtrid_length", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int("bqual_length", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("data",XIDDATASIZE));

  if (protocol->sendFields(&field_list,
                           Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return(1);

  pthread_mutex_lock(&LOCK_xid_cache);
  while ((xs= (XID_STATE*)hash_element(&xid_cache, i++)))
  {
    if (xs->xa_state==XA_PREPARED)
    {
      protocol->prepareForResend();
      protocol->store((int64_t)xs->xid.formatID);
      protocol->store((int64_t)xs->xid.gtrid_length);
      protocol->store((int64_t)xs->xid.bqual_length);
      protocol->store(xs->xid.data, xs->xid.gtrid_length+xs->xid.bqual_length,
                      &my_charset_bin);
      if (protocol->write())
      {
        pthread_mutex_unlock(&LOCK_xid_cache);
        return(1);
      }
    }
  }

  pthread_mutex_unlock(&LOCK_xid_cache);
  session->my_eof();
  return(0);
}


int ha_rollback_to_savepoint(Session *session, SAVEPOINT *sv)
{
  int error=0;
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
    StorageEngine *engine= ha_info->engine();
    assert(engine);
    if ((err= engine->savepoint_rollback(session,
                                         (void *)(sv+1))))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error=1;
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
    StorageEngine *engine= ha_info->engine();
    if ((err= engine->rollback(session, !(0))))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error=1;
    }
    status_var_increment(session->status_var.ha_rollback_count);
    ha_info_next= ha_info->next();
    ha_info->reset(); /* keep it conveniently zero-filled */
  }
  trans->ha_list= sv->ha_list;
  return(error);
}

/**
  @note
  according to the sql standard (ISO/IEC 9075-2:2003)
  section "4.33.4 SQL-statements and transaction states",
  SAVEPOINT is *not* transaction-initiating SQL-statement
*/
int ha_savepoint(Session *session, SAVEPOINT *sv)
{
  int error=0;
  Session_TRANS *trans= &session->transaction.all;
  Ha_trx_info *ha_info= trans->ha_list;
  for (; ha_info; ha_info= ha_info->next())
  {
    int err;
    StorageEngine *engine= ha_info->engine();
    assert(engine);
/*    if (! engine->savepoint_set)
    {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "SAVEPOINT");
      error=1;
      break;
    } */
    if ((err= engine->savepoint_set(session, (void *)(sv+1))))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error=1;
    }
    status_var_increment(session->status_var.ha_savepoint_count);
  }
  /*
    Remember the list of registered storage engines. All new
    engines are prepended to the beginning of the list.
  */
  sv->ha_list= trans->ha_list;
  return(error);
}

int ha_release_savepoint(Session *session, SAVEPOINT *sv)
{
  int error=0;
  Ha_trx_info *ha_info= sv->ha_list;

  for (; ha_info; ha_info= ha_info->next())
  {
    int err;
    StorageEngine *engine= ha_info->engine();
    /* Savepoint life time is enclosed into transaction life time. */
    assert(engine);
    if ((err= engine->savepoint_release(session,
                                        (void *)(sv+1))))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error=1;
    }
  }
  return(error);
}





/****************************************************************************
** General handler functions
****************************************************************************/
handler *handler::clone(MEM_ROOT *mem_root)
{
  handler *new_handler= get_new_handler(table->s, mem_root, table->s->db_type());
  /*
    Allocate handler->ref here because otherwise ha_open will allocate it
    on this->table->mem_root and we will not be able to reclaim that memory
    when the clone handler object is destroyed.
  */
  if (!(new_handler->ref= (unsigned char*) alloc_root(mem_root, ALIGN_SIZE(ref_length)*2)))
    return NULL;
  if (new_handler && !new_handler->ha_open(table,
                                           table->s->normalized_path.str,
                                           table->getDBStat(),
                                           HA_OPEN_IGNORE_IF_LOCKED))
    return new_handler;
  return NULL;
}

int handler::ha_index_init(uint32_t idx, bool sorted)
{
  int result;
  assert(inited==NONE);
  if (!(result= index_init(idx, sorted)))
    inited=INDEX;
  end_range= NULL;
  return(result);
}

int handler::ha_index_end()
{
  assert(inited==INDEX);
  inited=NONE;
  end_range= NULL;
  return(index_end());
}

int handler::ha_rnd_init(bool scan)
{
  int result;
  assert(inited==NONE || (inited==RND && scan));
  inited= (result= rnd_init(scan)) ? NONE: RND;
  return(result);
}

int handler::ha_rnd_end()
{
  assert(inited==RND);
  inited=NONE;
  return(rnd_end());
}

int handler::ha_index_or_rnd_end()
{
  return inited == INDEX ? ha_index_end() : inited == RND ? ha_rnd_end() : 0;
}

handler::Table_flags handler::ha_table_flags() const
{
  return cached_table_flags;
}

void handler::ha_start_bulk_insert(ha_rows rows)
{
  estimation_rows_to_insert= rows;
  start_bulk_insert(rows);
}

int handler::ha_end_bulk_insert()
{
  estimation_rows_to_insert= 0;
  return end_bulk_insert();
}

void handler::change_table_ptr(Table *table_arg, TableShare *share)
{
  table= table_arg;
  table_share= share;
}

const key_map *handler::keys_to_use_for_scanning()
{
  return &key_map_empty;
}

bool handler::has_transactions()
{
  return (ha_table_flags() & HA_NO_TRANSACTIONS) == 0;
}

void handler::ha_statistic_increment(ulong SSV::*offset) const
{
  status_var_increment(table->in_use->status_var.*offset);
}

void **handler::ha_data(Session *session) const
{
  return session_ha_data(session, engine);
}

Session *handler::ha_session(void) const
{
  assert(!table || !table->in_use || table->in_use == current_session);
  return (table && table->in_use) ? table->in_use : current_session;
}


bool handler::is_fatal_error(int error, uint32_t flags)
{
  if (!error ||
      ((flags & HA_CHECK_DUP_KEY) &&
       (error == HA_ERR_FOUND_DUPP_KEY ||
        error == HA_ERR_FOUND_DUPP_UNIQUE)))
    return false;
  return true;
}


ha_rows handler::records() { return stats.records; }

/**
  Open database-handler.

  Try O_RDONLY if cannot open as O_RDWR
  Don't wait for locks if not HA_OPEN_WAIT_IF_LOCKED is set
*/
int handler::ha_open(Table *table_arg, const char *name, int mode,
                     int test_if_locked)
{
  int error;

  table= table_arg;
  assert(table->s == table_share);
  assert(alloc_root_inited(&table->mem_root));

  if ((error=open(name,mode,test_if_locked)))
  {
    if ((error == EACCES || error == EROFS) && mode == O_RDWR &&
	(table->db_stat & HA_TRY_READ_ONLY))
    {
      table->db_stat|=HA_READ_ONLY;
      error=open(name,O_RDONLY,test_if_locked);
    }
  }
  if (error)
  {
    my_errno= error;                            /* Safeguard */
  }
  else
  {
    if (table->s->db_options_in_use & HA_OPTION_READ_ONLY_DATA)
      table->db_stat|=HA_READ_ONLY;
    (void) extra(HA_EXTRA_NO_READCHECK);	// Not needed in SQL

    /* ref is already allocated for us if we're called from handler::clone() */
    if (!ref && !(ref= (unsigned char*) alloc_root(&table->mem_root,
                                          ALIGN_SIZE(ref_length)*2)))
    {
      close();
      error=HA_ERR_OUT_OF_MEM;
    }
    else
      dup_ref=ref+ALIGN_SIZE(ref_length);
    cached_table_flags= table_flags();
  }
  return(error);
}

/**
  one has to use this method when to find
  random position by record as the plain
  position() call doesn't work for some
  handlers for random position
*/

int handler::rnd_pos_by_record(unsigned char *record)
{
  register int error;

  position(record);
  if (inited && (error= ha_index_end()))
    return(error);
  if ((error= ha_rnd_init(false)))
    return(error);

  return(rnd_pos(record, ref));
}

/**
  Read first row (only) from a table.

  This is never called for InnoDB tables, as these table types
  has the HA_STATS_RECORDS_IS_EXACT set.
*/
int handler::read_first_row(unsigned char * buf, uint32_t primary_key)
{
  register int error;

  ha_statistic_increment(&SSV::ha_read_first_count);

  /*
    If there is very few deleted rows in the table, find the first row by
    scanning the table.
    TODO remove the test for HA_READ_ORDER
  */
  if (stats.deleted < 10 || primary_key >= MAX_KEY ||
      !(index_flags(primary_key, 0, 0) & HA_READ_ORDER))
  {
    (void) ha_rnd_init(1);
    while ((error= rnd_next(buf)) == HA_ERR_RECORD_DELETED) ;
    (void) ha_rnd_end();
  }
  else
  {
    /* Find the first row through the primary key */
    (void) ha_index_init(primary_key, 0);
    error=index_first(buf);
    (void) ha_index_end();
  }
  return(error);
}

/**
  Generate the next auto-increment number based on increment and offset.
  computes the lowest number
  - strictly greater than "nr"
  - of the form: auto_increment_offset + N * auto_increment_increment

  In most cases increment= offset= 1, in which case we get:
  @verbatim 1,2,3,4,5,... @endverbatim
    If increment=10 and offset=5 and previous number is 1, we get:
  @verbatim 1,5,15,25,35,... @endverbatim
*/
inline uint64_t
compute_next_insert_id(uint64_t nr,struct system_variables *variables)
{
  if (variables->auto_increment_increment == 1)
    return (nr+1); // optimization of the formula below
  nr= (((nr+ variables->auto_increment_increment -
         variables->auto_increment_offset)) /
       (uint64_t) variables->auto_increment_increment);
  return (nr* (uint64_t) variables->auto_increment_increment +
          variables->auto_increment_offset);
}


void handler::adjust_next_insert_id_after_explicit_value(uint64_t nr)
{
  /*
    If we have set Session::next_insert_id previously and plan to insert an
    explicitely-specified value larger than this, we need to increase
    Session::next_insert_id to be greater than the explicit value.
  */
  if ((next_insert_id > 0) && (nr >= next_insert_id))
    set_next_insert_id(compute_next_insert_id(nr, &table->in_use->variables));
}


/**
  Compute a previous insert id

  Computes the largest number X:
  - smaller than or equal to "nr"
  - of the form: auto_increment_offset + N * auto_increment_increment
    where N>=0.

  @param nr            Number to "round down"
  @param variables     variables struct containing auto_increment_increment and
                       auto_increment_offset

  @return
    The number X if it exists, "nr" otherwise.
*/
inline uint64_t
prev_insert_id(uint64_t nr, struct system_variables *variables)
{
  if (unlikely(nr < variables->auto_increment_offset))
  {
    /*
      There's nothing good we can do here. That is a pathological case, where
      the offset is larger than the column's max possible value, i.e. not even
      the first sequence value may be inserted. User will receive warning.
    */
    return nr;
  }
  if (variables->auto_increment_increment == 1)
    return nr; // optimization of the formula below
  nr= (((nr - variables->auto_increment_offset)) /
       (uint64_t) variables->auto_increment_increment);
  return (nr * (uint64_t) variables->auto_increment_increment +
          variables->auto_increment_offset);
}


/**
  Update the auto_increment field if necessary.

  Updates columns with type NEXT_NUMBER if:

  - If column value is set to NULL (in which case
    auto_increment_field_not_null is 0)
  - If column is set to 0 and (sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO) is not
    set. In the future we will only set NEXT_NUMBER fields if one sets them
    to NULL (or they are not included in the insert list).

    In those cases, we check if the currently reserved interval still has
    values we have not used. If yes, we pick the smallest one and use it.
    Otherwise:

  - If a list of intervals has been provided to the statement via SET
    INSERT_ID or via an Intvar_log_event (in a replication slave), we pick the
    first unused interval from this list, consider it as reserved.

  - Otherwise we set the column for the first row to the value
    next_insert_id(get_auto_increment(column))) which is usually
    max-used-column-value+1.
    We call get_auto_increment() for the first row in a multi-row
    statement. get_auto_increment() will tell us the interval of values it
    reserved for us.

  - In both cases, for the following rows we use those reserved values without
    calling the handler again (we just progress in the interval, computing
    each new value from the previous one). Until we have exhausted them, then
    we either take the next provided interval or call get_auto_increment()
    again to reserve a new interval.

  - In both cases, the reserved intervals are remembered in
    session->auto_inc_intervals_in_cur_stmt_for_binlog if statement-based
    binlogging; the last reserved interval is remembered in
    auto_inc_interval_for_cur_row.

    The idea is that generated auto_increment values are predictable and
    independent of the column values in the table.  This is needed to be
    able to replicate into a table that already has rows with a higher
    auto-increment value than the one that is inserted.

    After we have already generated an auto-increment number and the user
    inserts a column with a higher value than the last used one, we will
    start counting from the inserted value.

    This function's "outputs" are: the table's auto_increment field is filled
    with a value, session->next_insert_id is filled with the value to use for the
    next row, if a value was autogenerated for the current row it is stored in
    session->insert_id_for_cur_row, if get_auto_increment() was called
    session->auto_inc_interval_for_cur_row is modified, if that interval is not
    present in session->auto_inc_intervals_in_cur_stmt_for_binlog it is added to
    this list.

  @todo
    Replace all references to "next number" or NEXT_NUMBER to
    "auto_increment", everywhere (see below: there is
    table->auto_increment_field_not_null, and there also exists
    table->next_number_field, it's not consistent).

  @retval
    0	ok
  @retval
    HA_ERR_AUTOINC_READ_FAILED  get_auto_increment() was called and
    returned ~(uint64_t) 0
  @retval
    HA_ERR_AUTOINC_ERANGE storing value in field caused strict mode
    failure.
*/

#define AUTO_INC_DEFAULT_NB_ROWS 1 // Some prefer 1024 here
#define AUTO_INC_DEFAULT_NB_MAX_BITS 16
#define AUTO_INC_DEFAULT_NB_MAX ((1 << AUTO_INC_DEFAULT_NB_MAX_BITS) - 1)

int handler::update_auto_increment()
{
  uint64_t nr, nb_reserved_values;
  bool append= false;
  Session *session= table->in_use;
  struct system_variables *variables= &session->variables;

  /*
    next_insert_id is a "cursor" into the reserved interval, it may go greater
    than the interval, but not smaller.
  */
  assert(next_insert_id >= auto_inc_interval_for_cur_row.minimum());

  if ((nr= table->next_number_field->val_int()) != 0)
  {
    /*
      Update next_insert_id if we had already generated a value in this
      statement (case of INSERT VALUES(null),(3763),(null):
      the last NULL needs to insert 3764, not the value of the first NULL plus
      1).
    */
    adjust_next_insert_id_after_explicit_value(nr);
    insert_id_for_cur_row= 0; // didn't generate anything
    return(0);
  }

  if ((nr= next_insert_id) >= auto_inc_interval_for_cur_row.maximum())
  {
    /* next_insert_id is beyond what is reserved, so we reserve more. */
    const Discrete_interval *forced=
      session->auto_inc_intervals_forced.get_next();
    if (forced != NULL)
    {
      nr= forced->minimum();
      nb_reserved_values= forced->values();
    }
    else
    {
      /*
        handler::estimation_rows_to_insert was set by
        handler::ha_start_bulk_insert(); if 0 it means "unknown".
      */
      uint32_t nb_already_reserved_intervals=
        session->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements();
      uint64_t nb_desired_values;
      /*
        If an estimation was given to the engine:
        - use it.
        - if we already reserved numbers, it means the estimation was
        not accurate, then we'll reserve 2*AUTO_INC_DEFAULT_NB_ROWS the 2nd
        time, twice that the 3rd time etc.
        If no estimation was given, use those increasing defaults from the
        start, starting from AUTO_INC_DEFAULT_NB_ROWS.
        Don't go beyond a max to not reserve "way too much" (because
        reservation means potentially losing unused values).
      */
      if (nb_already_reserved_intervals == 0 &&
          (estimation_rows_to_insert > 0))
        nb_desired_values= estimation_rows_to_insert;
      else /* go with the increasing defaults */
      {
        /* avoid overflow in formula, with this if() */
        if (nb_already_reserved_intervals <= AUTO_INC_DEFAULT_NB_MAX_BITS)
        {
          nb_desired_values= AUTO_INC_DEFAULT_NB_ROWS *
            (1 << nb_already_reserved_intervals);
          set_if_smaller(nb_desired_values, (uint64_t)AUTO_INC_DEFAULT_NB_MAX);
        }
        else
          nb_desired_values= AUTO_INC_DEFAULT_NB_MAX;
      }
      /* This call ignores all its parameters but nr, currently */
      get_auto_increment(variables->auto_increment_offset,
                         variables->auto_increment_increment,
                         nb_desired_values, &nr,
                         &nb_reserved_values);
      if (nr == ~(uint64_t) 0)
        return(HA_ERR_AUTOINC_READ_FAILED);  // Mark failure

      /*
        That rounding below should not be needed when all engines actually
        respect offset and increment in get_auto_increment(). But they don't
        so we still do it. Wonder if for the not-first-in-index we should do
        it. Hope that this rounding didn't push us out of the interval; even
        if it did we cannot do anything about it (calling the engine again
        will not help as we inserted no row).
      */
      nr= compute_next_insert_id(nr-1, variables);
    }

    if (table->s->next_number_keypart == 0)
    {
      /* We must defer the appending until "nr" has been possibly truncated */
      append= true;
    }
  }

  if (unlikely(table->next_number_field->store((int64_t) nr, true)))
  {
    /*
      first test if the query was aborted due to strict mode constraints
    */
    if (session->killed == Session::KILL_BAD_DATA)
      return(HA_ERR_AUTOINC_ERANGE);

    /*
      field refused this value (overflow) and truncated it, use the result of
      the truncation (which is going to be inserted); however we try to
      decrease it to honour auto_increment_* variables.
      That will shift the left bound of the reserved interval, we don't
      bother shifting the right bound (anyway any other value from this
      interval will cause a duplicate key).
    */
    nr= prev_insert_id(table->next_number_field->val_int(), variables);
    if (unlikely(table->next_number_field->store((int64_t) nr, true)))
      nr= table->next_number_field->val_int();
  }
  if (append)
  {
    auto_inc_interval_for_cur_row.replace(nr, nb_reserved_values,
                                          variables->auto_increment_increment);
  }

  /*
    Record this autogenerated value. If the caller then
    succeeds to insert this value, it will call
    record_first_successful_insert_id_in_cur_stmt()
    which will set first_successful_insert_id_in_cur_stmt if it's not
    already set.
  */
  insert_id_for_cur_row= nr;
  /*
    Set next insert id to point to next auto-increment value to be able to
    handle multi-row statements.
  */
  set_next_insert_id(compute_next_insert_id(nr, variables));

  return(0);
}


/**
  MySQL signal that it changed the column bitmap

  This is for handlers that needs to setup their own column bitmaps.
  Normally the handler should set up their own column bitmaps in
  index_init() or rnd_init() and in any column_bitmaps_signal() call after
  this.

  The handler is allowed to do changes to the bitmap after a index_init or
  rnd_init() call is made as after this, MySQL will not use the bitmap
  for any program logic checking.
*/
void handler::column_bitmaps_signal()
{
  return;
}


/**
  Reserves an interval of auto_increment values from the handler.

  offset and increment means that we want values to be of the form
  offset + N * increment, where N>=0 is integer.
  If the function sets *first_value to ~(uint64_t)0 it means an error.
  If the function sets *nb_reserved_values to UINT64_MAX it means it has
  reserved to "positive infinite".

  @param offset
  @param increment
  @param nb_desired_values   how many values we want
  @param first_value         (OUT) the first value reserved by the handler
  @param nb_reserved_values  (OUT) how many values the handler reserved
*/
void handler::get_auto_increment(uint64_t ,
                                 uint64_t ,
                                 uint64_t ,
                                 uint64_t *first_value,
                                 uint64_t *nb_reserved_values)
{
  uint64_t nr;
  int error;

  (void) extra(HA_EXTRA_KEYREAD);
  table->mark_columns_used_by_index_no_reset(table->s->next_number_index,
                                        table->read_set);
  column_bitmaps_signal();
  index_init(table->s->next_number_index, 1);
  if (table->s->next_number_keypart == 0)
  {						// Autoincrement at key-start
    error=index_last(table->record[1]);
    /*
      MySQL implicitely assumes such method does locking (as MySQL decides to
      use nr+increment without checking again with the handler, in
      handler::update_auto_increment()), so reserves to infinite.
    */
    *nb_reserved_values= UINT64_MAX;
  }
  else
  {
    unsigned char key[MAX_KEY_LENGTH];
    key_copy(key, table->record[0],
             table->key_info + table->s->next_number_index,
             table->s->next_number_key_offset);
    error= index_read_map(table->record[1], key,
                          make_prev_keypart_map(table->s->next_number_keypart),
                          HA_READ_PREFIX_LAST);
    /*
      MySQL needs to call us for next row: assume we are inserting ("a",null)
      here, we return 3, and next this statement will want to insert
      ("b",null): there is no reason why ("b",3+1) would be the good row to
      insert: maybe it already exists, maybe 3+1 is too large...
    */
    *nb_reserved_values= 1;
  }

  if (error)
    nr=1;
  else
    nr= ((uint64_t) table->next_number_field->
         val_int_offset(table->s->rec_buff_length)+1);
  index_end();
  (void) extra(HA_EXTRA_NO_KEYREAD);
  *first_value= nr;
}


void handler::ha_release_auto_increment()
{
  release_auto_increment();
  insert_id_for_cur_row= 0;
  auto_inc_interval_for_cur_row.replace(0, 0, 0);
  if (next_insert_id > 0)
  {
    next_insert_id= 0;
    /*
      this statement used forced auto_increment values if there were some,
      wipe them away for other statements.
    */
    table->in_use->auto_inc_intervals_forced.empty();
  }
}


void handler::print_keydup_error(uint32_t key_nr, const char *msg)
{
  /* Write the duplicated key in the error message */
  char key[MAX_KEY_LENGTH];
  String str(key,sizeof(key),system_charset_info);

  if (key_nr == MAX_KEY)
  {
    /* Key is unknown */
    str.copy("", 0, system_charset_info);
    my_printf_error(ER_DUP_ENTRY, msg, MYF(0), str.c_ptr(), "*UNKNOWN*");
  }
  else
  {
    /* Table is opened and defined at this point */
    key_unpack(&str,table,(uint32_t) key_nr);
    uint32_t max_length=DRIZZLE_ERRMSG_SIZE-(uint32_t) strlen(msg);
    if (str.length() >= max_length)
    {
      str.length(max_length-4);
      str.append(STRING_WITH_LEN("..."));
    }
    my_printf_error(ER_DUP_ENTRY, msg,
		    MYF(0), str.c_ptr(), table->key_info[key_nr].name);
  }
}


/**
  Print error that we got from handler function.

  @note
    In case of delete table it's only safe to use the following parts of
    the 'table' structure:
    - table->s->path
    - table->alias
*/
void handler::print_error(int error, myf errflag)
{
  int textno=ER_GET_ERRNO;
  switch (error) {
  case EACCES:
    textno=ER_OPEN_AS_READONLY;
    break;
  case EAGAIN:
    textno=ER_FILE_USED;
    break;
  case ENOENT:
    textno=ER_FILE_NOT_FOUND;
    break;
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_NO_ACTIVE_RECORD:
  case HA_ERR_END_OF_FILE:
    textno=ER_KEY_NOT_FOUND;
    break;
  case HA_ERR_WRONG_MRG_TABLE_DEF:
    textno=ER_WRONG_MRG_TABLE;
    break;
  case HA_ERR_FOUND_DUPP_KEY:
  {
    uint32_t key_nr=get_dup_key(error);
    if ((int) key_nr >= 0)
    {
      print_keydup_error(key_nr, ER(ER_DUP_ENTRY_WITH_KEY_NAME));
      return;
    }
    textno=ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOREIGN_DUPLICATE_KEY:
  {
    uint32_t key_nr= get_dup_key(error);
    if ((int) key_nr >= 0)
    {
      uint32_t max_length;
      /* Write the key in the error message */
      char key[MAX_KEY_LENGTH];
      String str(key,sizeof(key),system_charset_info);
      /* Table is opened and defined at this point */
      key_unpack(&str,table,(uint32_t) key_nr);
      max_length= (DRIZZLE_ERRMSG_SIZE-
                   (uint32_t) strlen(ER(ER_FOREIGN_DUPLICATE_KEY)));
      if (str.length() >= max_length)
      {
        str.length(max_length-4);
        str.append(STRING_WITH_LEN("..."));
      }
      my_error(ER_FOREIGN_DUPLICATE_KEY, MYF(0), table_share->table_name.str,
        str.c_ptr(), key_nr+1);
      return;
    }
    textno= ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOUND_DUPP_UNIQUE:
    textno=ER_DUP_UNIQUE;
    break;
  case HA_ERR_RECORD_CHANGED:
    textno=ER_CHECKREAD;
    break;
  case HA_ERR_CRASHED:
    textno=ER_NOT_KEYFILE;
    break;
  case HA_ERR_WRONG_IN_RECORD:
    textno= ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_CRASHED_ON_USAGE:
    textno=ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_NOT_A_TABLE:
    textno= error;
    break;
  case HA_ERR_CRASHED_ON_REPAIR:
    textno=ER_CRASHED_ON_REPAIR;
    break;
  case HA_ERR_OUT_OF_MEM:
    textno=ER_OUT_OF_RESOURCES;
    break;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  case HA_ERR_OLD_FILE:
    textno=ER_OLD_KEYFILE;
    break;
  case HA_ERR_UNSUPPORTED:
    textno=ER_UNSUPPORTED_EXTENSION;
    break;
  case HA_ERR_RECORD_FILE_FULL:
  case HA_ERR_INDEX_FILE_FULL:
    textno=ER_RECORD_FILE_FULL;
    break;
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_LOCK_TABLE_FULL:
    textno=ER_LOCK_TABLE_FULL;
    break;
  case HA_ERR_LOCK_DEADLOCK:
    textno=ER_LOCK_DEADLOCK;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  case HA_ERR_CANNOT_ADD_FOREIGN:
    textno=ER_CANNOT_ADD_FOREIGN;
    break;
  case HA_ERR_ROW_IS_REFERENCED:
  {
    String str;
    get_error_message(error, &str);
    my_error(ER_ROW_IS_REFERENCED_2, MYF(0), str.c_ptr_safe());
    return;
  }
  case HA_ERR_NO_REFERENCED_ROW:
  {
    String str;
    get_error_message(error, &str);
    my_error(ER_NO_REFERENCED_ROW_2, MYF(0), str.c_ptr_safe());
    return;
  }
  case HA_ERR_TABLE_DEF_CHANGED:
    textno=ER_TABLE_DEF_CHANGED;
    break;
  case HA_ERR_NO_SUCH_TABLE:
    my_error(ER_NO_SUCH_TABLE, MYF(0), table_share->db.str,
             table_share->table_name.str);
    return;
  case HA_ERR_RBR_LOGGING_FAILED:
    textno= ER_BINLOG_ROW_LOGGING_FAILED;
    break;
  case HA_ERR_DROP_INDEX_FK:
  {
    const char *ptr= "???";
    uint32_t key_nr= get_dup_key(error);
    if ((int) key_nr >= 0)
      ptr= table->key_info[key_nr].name;
    my_error(ER_DROP_INDEX_FK, MYF(0), ptr);
    return;
  }
  case HA_ERR_TABLE_NEEDS_UPGRADE:
    textno=ER_TABLE_NEEDS_UPGRADE;
    break;
  case HA_ERR_TABLE_READONLY:
    textno= ER_OPEN_AS_READONLY;
    break;
  case HA_ERR_AUTOINC_READ_FAILED:
    textno= ER_AUTOINC_READ_FAILED;
    break;
  case HA_ERR_AUTOINC_ERANGE:
    textno= ER_WARN_DATA_OUT_OF_RANGE;
    break;
  case HA_ERR_LOCK_OR_ACTIVE_TRANSACTION:
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    return;
  default:
    {
      /* The error was "unknown" to this function.
	 Ask handler if it has got a message for this error */
      bool temporary= false;
      String str;
      temporary= get_error_message(error, &str);
      if (!str.is_empty())
      {
	      const char* engine_name= table_type();
	      if (temporary)
	        my_error(ER_GET_TEMPORARY_ERRMSG, MYF(0), error, str.ptr(),
                   engine_name);
	      else
	        my_error(ER_GET_ERRMSG, MYF(0), error, str.ptr(), engine_name);
      }
      else
      {
	      my_error(ER_GET_ERRNO,errflag,error);
      }
      return;
    }
  }
  my_error(textno, errflag, table_share->table_name.str, error);
  return;
}


/**
  Return an error message specific to this handler.

  @param error  error code previously returned by handler
  @param buf    pointer to String where to add error message

  @return
    Returns true if this is a temporary error
*/
bool handler::get_error_message(int ,
                                String* )
{
  return false;
}


int handler::ha_check_for_upgrade(HA_CHECK_OPT *check_opt)
{
  KEY *keyinfo, *keyend;
  KEY_PART_INFO *keypart, *keypartend;

  if (!table->s->mysql_version)
  {
    /* check for blob-in-key error */
    keyinfo= table->key_info;
    keyend= table->key_info + table->s->keys;
    for (; keyinfo < keyend; keyinfo++)
    {
      keypart= keyinfo->key_part;
      keypartend= keypart + keyinfo->key_parts;
      for (; keypart < keypartend; keypart++)
      {
        if (!keypart->fieldnr)
          continue;
        Field *field= table->field[keypart->fieldnr-1];
        if (field->type() == DRIZZLE_TYPE_BLOB)
        {
          return HA_ADMIN_NEEDS_CHECK;
        }
      }
    }
  }
  return check_for_upgrade(check_opt);
}


/* Code left, but Drizzle has no legacy yet (while MySQL did) */
int handler::check_old_types()
{
  return 0;
}

/**
  @return
    key if error because of duplicated keys
*/
uint32_t handler::get_dup_key(int error)
{
  table->file->errkey  = (uint32_t) -1;
  if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_FOREIGN_DUPLICATE_KEY ||
      error == HA_ERR_FOUND_DUPP_UNIQUE ||
      error == HA_ERR_DROP_INDEX_FK)
    info(HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK);
  return(table->file->errkey);
}


/**
  Delete all files with extension from bas_ext().

  @param name		Base name of table

  @note
    We assume that the handler may return more extensions than
    was actually used for the file.

  @retval
    0   If we successfully deleted at least one file from base_ext and
    didn't get any other errors than ENOENT
  @retval
    !0  Error
*/
int handler::delete_table(const char *name)
{
  int error= 0;
  int enoent_or_zero= ENOENT;                   // Error if no file was deleted
  char buff[FN_REFLEN];

  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    fn_format(buff, name, "", *ext, MY_UNPACK_FILENAME|MY_APPEND_EXT);
    if (my_delete_with_symlink(buff, MYF(0)))
    {
      if ((error= my_errno) != ENOENT)
	break;
    }
    else
      enoent_or_zero= 0;                        // No error for ENOENT
    error= enoent_or_zero;
  }
  return error;
}


int handler::rename_table(const char * from, const char * to)
{
  int error= 0;
  for (const char **ext= bas_ext(); *ext ; ext++)
  {
    if (rename_file_ext(from, to, *ext))
    {
      if ((error=my_errno) != ENOENT)
	break;
      error= 0;
    }
  }
  return error;
}


void handler::drop_table(const char *name)
{
  close();
  delete_table(name);
}


/**
  Performs checks upon the table.

  @param session                thread doing CHECK Table operation
  @param check_opt          options from the parser

  @retval
    HA_ADMIN_OK               Successful upgrade
  @retval
    HA_ADMIN_NEEDS_UPGRADE    Table has structures requiring upgrade
  @retval
    HA_ADMIN_NEEDS_ALTER      Table has structures requiring ALTER Table
  @retval
    HA_ADMIN_NOT_IMPLEMENTED
*/
int handler::ha_check(Session *session, HA_CHECK_OPT *check_opt)
{
  int error;

  if (table->s->mysql_version < DRIZZLE_VERSION_ID)
  {
    if ((error= check_old_types()))
      return error;
    error= ha_check_for_upgrade(check_opt);
    if (error && (error != HA_ADMIN_NEEDS_CHECK))
      return error;
  }
  if ((error= check(session, check_opt)))
    return error;
  return HA_ADMIN_OK;
}

/**
  A helper function to mark a transaction read-write,
  if it is started.
*/

inline
void
handler::mark_trx_read_write()
{
  Ha_trx_info *ha_info= &ha_session()->ha_data[engine->slot].ha_info[0];
  /*
    When a storage engine method is called, the transaction must
    have been started, unless it's a DDL call, for which the
    storage engine starts the transaction internally, and commits
    it internally, without registering in the ha_list.
    Unfortunately here we can't know know for sure if the engine
    has registered the transaction or not, so we must check.
  */
  if (ha_info->is_started())
  {
    /*
      table_share can be NULL in ha_delete_table(). See implementation
      of standalone function ha_delete_table() in sql_base.cc.
    */
    if (table_share == NULL || table_share->tmp_table == NO_TMP_TABLE)
      ha_info->set_trx_read_write();
  }
}


/**
  Repair table: public interface.

  @sa handler::repair()
*/

int handler::ha_repair(Session* session, HA_CHECK_OPT* check_opt)
{
  int result;

  mark_trx_read_write();

  if ((result= repair(session, check_opt)))
    return result;
  return HA_ADMIN_OK;
}


/**
  Bulk update row: public interface.

  @sa handler::bulk_update_row()
*/

int
handler::ha_bulk_update_row(const unsigned char *old_data, unsigned char *new_data,
                            uint32_t *dup_key_found)
{
  mark_trx_read_write();

  return bulk_update_row(old_data, new_data, dup_key_found);
}


/**
  Delete all rows: public interface.

  @sa handler::delete_all_rows()
*/

int
handler::ha_delete_all_rows()
{
  mark_trx_read_write();

  return delete_all_rows();
}


/**
  Reset auto increment: public interface.

  @sa handler::reset_auto_increment()
*/

int
handler::ha_reset_auto_increment(uint64_t value)
{
  mark_trx_read_write();

  return reset_auto_increment(value);
}


/**
  Optimize table: public interface.

  @sa handler::optimize()
*/

int
handler::ha_optimize(Session* session, HA_CHECK_OPT* check_opt)
{
  mark_trx_read_write();

  return optimize(session, check_opt);
}


/**
  Analyze table: public interface.

  @sa handler::analyze()
*/

int
handler::ha_analyze(Session* session, HA_CHECK_OPT* check_opt)
{
  mark_trx_read_write();

  return analyze(session, check_opt);
}


/**
  Check and repair table: public interface.

  @sa handler::check_and_repair()
*/

bool
handler::ha_check_and_repair(Session *session)
{
  mark_trx_read_write();

  return check_and_repair(session);
}


/**
  Disable indexes: public interface.

  @sa handler::disable_indexes()
*/

int
handler::ha_disable_indexes(uint32_t mode)
{
  mark_trx_read_write();

  return disable_indexes(mode);
}


/**
  Enable indexes: public interface.

  @sa handler::enable_indexes()
*/

int
handler::ha_enable_indexes(uint32_t mode)
{
  mark_trx_read_write();

  return enable_indexes(mode);
}


/**
  Discard or import tablespace: public interface.

  @sa handler::discard_or_import_tablespace()
*/

int
handler::ha_discard_or_import_tablespace(bool discard)
{
  mark_trx_read_write();

  return discard_or_import_tablespace(discard);
}


/**
  Prepare for alter: public interface.

  Called to prepare an *online* ALTER.

  @sa handler::prepare_for_alter()
*/

void
handler::ha_prepare_for_alter()
{
  mark_trx_read_write();

  prepare_for_alter();
}


/**
  Rename table: public interface.

  @sa handler::rename_table()
*/

int
handler::ha_rename_table(const char *from, const char *to)
{
  mark_trx_read_write();

  return rename_table(from, to);
}


/**
  Delete table: public interface.

  @sa handler::delete_table()
*/

int
handler::ha_delete_table(const char *name)
{
  mark_trx_read_write();

  return delete_table(name);
}


/**
  Drop table in the engine: public interface.

  @sa handler::drop_table()
*/

void
handler::ha_drop_table(const char *name)
{
  mark_trx_read_write();

  return drop_table(name);
}


/**
  Create a table in the engine: public interface.

  @sa handler::create()
*/

int
handler::ha_create(const char *name, Table *form, HA_CREATE_INFO *create_info)
{
  mark_trx_read_write();

  return create(name, form, create_info);
}


/**
  Create handler files for CREATE TABLE: public interface.

  @sa handler::create_handler_files()
*/

int
handler::ha_create_handler_files(const char *name, const char *old_name,
                                 int action_flag, HA_CREATE_INFO *create_info)
{
  mark_trx_read_write();

  return create_handler_files(name, old_name, action_flag, create_info);
}


/**
  Tell the storage engine that it is allowed to "disable transaction" in the
  handler. It is a hint that ACID is not required - it is used in NDB for
  ALTER Table, for example, when data are copied to temporary table.
  A storage engine may treat this hint any way it likes. NDB for example
  starts to commit every now and then automatically.
  This hint can be safely ignored.
*/
int ha_enable_transaction(Session *session, bool on)
{
  int error=0;

  if ((session->transaction.on= on))
  {
    /*
      Now all storage engines should have transaction handling enabled.
      But some may have it enabled all the time - "disabling" transactions
      is an optimization hint that storage engine is free to ignore.
      So, let's commit an open transaction (if any) now.
    */
    if (!(error= ha_commit_trans(session, 0)))
      if (! session->endTransaction(COMMIT))
        error= 1;

  }
  return(error);
}

int handler::index_next_same(unsigned char *buf, const unsigned char *key, uint32_t keylen)
{
  int error;
  if (!(error=index_next(buf)))
  {
    my_ptrdiff_t ptrdiff= buf - table->record[0];
    unsigned char *save_record_0= NULL;
    KEY *key_info= NULL;
    KEY_PART_INFO *key_part;
    KEY_PART_INFO *key_part_end= NULL;

    /*
      key_cmp_if_same() compares table->record[0] against 'key'.
      In parts it uses table->record[0] directly, in parts it uses
      field objects with their local pointers into table->record[0].
      If 'buf' is distinct from table->record[0], we need to move
      all record references. This is table->record[0] itself and
      the field pointers of the fields used in this key.
    */
    if (ptrdiff)
    {
      save_record_0= table->record[0];
      table->record[0]= buf;
      key_info= table->key_info + active_index;
      key_part= key_info->key_part;
      key_part_end= key_part + key_info->key_parts;
      for (; key_part < key_part_end; key_part++)
      {
        assert(key_part->field);
        key_part->field->move_field_offset(ptrdiff);
      }
    }

    if (key_cmp_if_same(table, key, active_index, keylen))
    {
      table->status=STATUS_NOT_FOUND;
      error=HA_ERR_END_OF_FILE;
    }

    /* Move back if necessary. */
    if (ptrdiff)
    {
      table->record[0]= save_record_0;
      for (key_part= key_info->key_part; key_part < key_part_end; key_part++)
        key_part->field->move_field_offset(-ptrdiff);
    }
  }
  return(error);
}


/****************************************************************************
** Some general functions that isn't in the handler class
****************************************************************************/


void st_ha_check_opt::init()
{
  flags= 0; 
  use_frm= false;
}


/*****************************************************************************
  Key cache handling.

  This code is only relevant for ISAM/MyISAM tables

  key_cache->cache may be 0 only in the case where a key cache is not
  initialized or when we where not able to init the key cache in a previous
  call to ha_init_key_cache() (probably out of memory)
*****************************************************************************/

/**
  Init a key cache if it has not been initied before.
*/
int ha_init_key_cache(const char *,
                      KEY_CACHE *key_cache)
{
  if (!key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    uint32_t tmp_buff_size= (uint32_t) key_cache->param_buff_size;
    uint32_t tmp_block_size= (uint32_t) key_cache->param_block_size;
    uint32_t division_limit= key_cache->param_division_limit;
    uint32_t age_threshold=  key_cache->param_age_threshold;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return(!init_key_cache(key_cache,
				tmp_block_size,
				tmp_buff_size,
				division_limit, age_threshold));
  }
  return(0);
}


/**
  Resize key cache.
*/
int ha_resize_key_cache(KEY_CACHE *key_cache)
{
  if (key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    long tmp_buff_size= (long) key_cache->param_buff_size;
    long tmp_block_size= (long) key_cache->param_block_size;
    uint32_t division_limit= key_cache->param_division_limit;
    uint32_t age_threshold=  key_cache->param_age_threshold;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return(!resize_key_cache(key_cache, tmp_block_size,
				  tmp_buff_size,
				  division_limit, age_threshold));
  }
  return(0);
}


/**
  Change parameters for key cache (like size)
*/
int ha_change_key_cache_param(KEY_CACHE *key_cache)
{
  if (key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    uint32_t division_limit= key_cache->param_division_limit;
    uint32_t age_threshold=  key_cache->param_age_threshold;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    change_key_cache_param(key_cache, division_limit, age_threshold);
  }
  return 0;
}

/**
  Free memory allocated by a key cache.
*/
int ha_end_key_cache(KEY_CACHE *key_cache)
{
  end_key_cache(key_cache, 1);		// Can never fail
  return 0;
}

/**
  Move all tables from one key cache to another one.
*/
int ha_change_key_cache(KEY_CACHE *old_key_cache,
			KEY_CACHE *new_key_cache)
{
  mi_change_key_cache(old_key_cache, new_key_cache);
  return 0;
}


/**
  Calculate cost of 'index only' scan for given index and number of records

  @param keynr    Index number
  @param records  Estimated number of records to be retrieved

  @note
    It is assumed that we will read trough the whole key range and that all
    key blocks are half full (normally things are much better). It is also
    assumed that each time we read the next key from the index, the handler
    performs a random seek, thus the cost is proportional to the number of
    blocks read.

  @todo
    Consider joining this function and handler::read_time() into one
    handler::read_time(keynr, records, ranges, bool index_only) function.

  @return
    Estimated cost of 'index only' scan
*/

double handler::index_only_read_time(uint32_t keynr, double key_records)
{
  uint32_t keys_per_block= (stats.block_size/2/
			(table->key_info[keynr].key_length + ref_length) + 1);
  return ((double) (key_records + keys_per_block-1) /
          (double) keys_per_block);
}


/****************************************************************************
 * Default MRR implementation (MRR to non-MRR converter)
 ***************************************************************************/

/**
  Get cost and other information about MRR scan over a known list of ranges

  Calculate estimated cost and other information about an MRR scan for given
  sequence of ranges.

  @param keyno           Index number
  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges_arg    Number of ranges in the sequence, or 0 if the caller
                         can't efficiently determine it
  @param bufsz    INOUT  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that is expected to be actually
                              used, or 0 if buffer is not needed.
  @param flags    INOUT  A combination of HA_MRR_* flags
  @param cost     OUT    Estimated cost of MRR access

  @note
    This method (or an overriding one in a derived class) must check for
    session->killed and return HA_POS_ERROR if it is not zero. This is required
    for a user to be able to interrupt the calculation by killing the
    connection/query.

  @retval
    HA_POS_ERROR  Error or the engine is unable to perform the requested
                  scan. Values of OUT parameters are undefined.
  @retval
    other         OK, *cost contains cost of the scan, *bufsz and *flags
                  contain scan parameters.
*/

ha_rows
handler::multi_range_read_info_const(uint32_t keyno, RANGE_SEQ_IF *seq,
                                     void *seq_init_param,
                                     uint32_t ,
                                     uint32_t *bufsz, uint32_t *flags, COST_VECT *cost)
{
  KEY_MULTI_RANGE range;
  range_seq_t seq_it;
  ha_rows rows, total_rows= 0;
  uint32_t n_ranges=0;
  Session *session= current_session;

  /* Default MRR implementation doesn't need buffer */
  *bufsz= 0;

  seq_it= seq->init(seq_init_param, n_ranges, *flags);
  while (!seq->next(seq_it, &range))
  {
    if (unlikely(session->killed != 0))
      return HA_POS_ERROR;

    n_ranges++;
    key_range *min_endp, *max_endp;
    {
      min_endp= range.start_key.length? &range.start_key : NULL;
      max_endp= range.end_key.length? &range.end_key : NULL;
    }
    if ((range.range_flag & UNIQUE_RANGE) && !(range.range_flag & NULL_RANGE))
      rows= 1; /* there can be at most one row */
    else
    {
      if (HA_POS_ERROR == (rows= this->records_in_range(keyno, min_endp,
                                                        max_endp)))
      {
        /* Can't scan one range => can't do MRR scan at all */
        total_rows= HA_POS_ERROR;
        break;
      }
    }
    total_rows += rows;
  }

  if (total_rows != HA_POS_ERROR)
  {
    /* The following calculation is the same as in multi_range_read_info(): */
    *flags |= HA_MRR_USE_DEFAULT_IMPL;
    cost->zero();
    cost->avg_io_cost= 1; /* assume random seeks */
    if ((*flags & HA_MRR_INDEX_ONLY) && total_rows > 2)
      cost->io_count= index_only_read_time(keyno, (uint32_t)total_rows);
    else
      cost->io_count= read_time(keyno, n_ranges, total_rows);
    cost->cpu_cost= (double) total_rows / TIME_FOR_COMPARE + 0.01;
  }
  return total_rows;
}


/**
  Get cost and other information about MRR scan over some sequence of ranges

  Calculate estimated cost and other information about an MRR scan for some
  sequence of ranges.

  The ranges themselves will be known only at execution phase. When this
  function is called we only know number of ranges and a (rough) E(#records)
  within those ranges.

  Currently this function is only called for "n-keypart singlepoint" ranges,
  i.e. each range is "keypart1=someconst1 AND ... AND keypartN=someconstN"

  The flags parameter is a combination of those flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION, HA_MRR_LIMITS.

  @param keyno           Index number
  @param n_ranges        Estimated number of ranges (i.e. intervals) in the
                         range sequence.
  @param n_rows          Estimated total number of records contained within all
                         of the ranges
  @param bufsz    INOUT  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that will be actually used, or
                              0 if buffer is not needed.
  @param flags    INOUT  A combination of HA_MRR_* flags
  @param cost     OUT    Estimated cost of MRR access

  @retval
    0     OK, *cost contains cost of the scan, *bufsz and *flags contain scan
          parameters.
  @retval
    other Error or can't perform the requested scan
*/

int handler::multi_range_read_info(uint32_t keyno, uint32_t n_ranges, uint32_t n_rows,
                                   uint32_t *bufsz, uint32_t *flags, COST_VECT *cost)
{
  *bufsz= 0; /* Default implementation doesn't need a buffer */

  *flags |= HA_MRR_USE_DEFAULT_IMPL;

  cost->zero();
  cost->avg_io_cost= 1; /* assume random seeks */

  /* Produce the same cost as non-MRR code does */
  if (*flags & HA_MRR_INDEX_ONLY)
    cost->io_count= index_only_read_time(keyno, n_rows);
  else
    cost->io_count= read_time(keyno, n_ranges, n_rows);
  return 0;
}


/**
  Initialize the MRR scan

  Initialize the MRR scan. This function may do heavyweight scan
  initialization like row prefetching/sorting/etc (NOTE: but better not do
  it here as we may not need it, e.g. if we never satisfy WHERE clause on
  previous tables. For many implementations it would be natural to do such
  initializations in the first multi_read_range_next() call)

  mode is a combination of the following flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION

  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges        Number of ranges in the sequence
  @param mode            Flags, see the description section for the details
  @param buf             INOUT: memory buffer to be used

  @note
    One must have called index_init() before calling this function. Several
    multi_range_read_init() calls may be made in course of one query.

    Until WL#2623 is done (see its text, section 3.2), the following will
    also hold:
    The caller will guarantee that if "seq->init == mrr_ranges_array_init"
    then seq_init_param is an array of n_ranges KEY_MULTI_RANGE structures.
    This property will only be used by NDB handler until WL#2623 is done.

    Buffer memory management is done according to the following scenario:
    The caller allocates the buffer and provides it to the callee by filling
    the members of HANDLER_BUFFER structure.
    The callee consumes all or some fraction of the provided buffer space, and
    sets the HANDLER_BUFFER members accordingly.
    The callee may use the buffer memory until the next multi_range_read_init()
    call is made, all records have been read, or until index_end() call is
    made, whichever comes first.

  @retval 0  OK
  @retval 1  Error
*/

int
handler::multi_range_read_init(RANGE_SEQ_IF *seq_funcs, void *seq_init_param,
                               uint32_t n_ranges, uint32_t mode,
                               HANDLER_BUFFER *)
{
  mrr_iter= seq_funcs->init(seq_init_param, n_ranges, mode);
  mrr_funcs= *seq_funcs;
  mrr_is_output_sorted= test(mode & HA_MRR_SORTED);
  mrr_have_range= false;
  return(0);
}


/**
  Get next record in MRR scan

  Default MRR implementation: read the next record

  @param range_info  OUT  Undefined if HA_MRR_NO_ASSOCIATION flag is in effect
                          Otherwise, the opaque value associated with the range
                          that contains the returned record.

  @retval 0      OK
  @retval other  Error code
*/

int handler::multi_range_read_next(char **range_info)
{
  int result= 0;
  int range_res= 0;

  if (!mrr_have_range)
  {
    mrr_have_range= true;
    goto start;
  }

  do
  {
    /* Save a call if there can be only one row in range. */
    if (mrr_cur_range.range_flag != (UNIQUE_RANGE | EQ_RANGE))
    {
      result= read_range_next();
      /* On success or non-EOF errors jump to the end. */
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
    else
    {
      if (was_semi_consistent_read())
        goto scan_it_again;
      /*
        We need to set this for the last range only, but checking this
        condition is more expensive than just setting the result code.
      */
      result= HA_ERR_END_OF_FILE;
    }

start:
    /* Try the next range(s) until one matches a record. */
    while (!(range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range)))
    {
scan_it_again:
      result= read_range_first(mrr_cur_range.start_key.keypart_map ?
                                 &mrr_cur_range.start_key : 0,
                               mrr_cur_range.end_key.keypart_map ?
                                 &mrr_cur_range.end_key : 0,
                               test(mrr_cur_range.range_flag & EQ_RANGE),
                               mrr_is_output_sorted);
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
  }
  while ((result == HA_ERR_END_OF_FILE) && !range_res);

  *range_info= mrr_cur_range.ptr;
  return(result);
}


/* **************************************************************************
 * DS-MRR implementation
 ***************************************************************************/

/**
  DS-MRR: Initialize and start MRR scan

  Initialize and start the MRR scan. Depending on the mode parameter, this
  may use default or DS-MRR implementation.

  @param h               Table handler to be used
  @param key             Index to be used
  @param seq_funcs       Interval sequence enumeration functions
  @param seq_init_param  Interval sequence enumeration parameter
  @param n_ranges        Number of ranges in the sequence.
  @param mode            HA_MRR_* modes to use
  @param buf             INOUT Buffer to use

  @retval 0     Ok, Scan started.
  @retval other Error
*/

int DsMrr_impl::dsmrr_init(handler *h_in, KEY *key,
                           RANGE_SEQ_IF *seq_funcs, void *seq_init_param,
                           uint32_t n_ranges, uint32_t mode, HANDLER_BUFFER *buf)
{
  uint32_t elem_size;
  uint32_t keyno;
  Item *pushed_cond= NULL;
  handler *new_h2;
  keyno= h_in->active_index;
  assert(h2 == NULL);
  if (mode & HA_MRR_USE_DEFAULT_IMPL || mode & HA_MRR_SORTED)
  {
    use_default_impl= true;
    return(h_in->handler::multi_range_read_init(seq_funcs, seq_init_param,
                                                  n_ranges, mode, buf));
  }
  rowids_buf= buf->buffer;
  //psergey-todo: don't add key_length as it is not needed anymore
  rowids_buf += key->key_length + h_in->ref_length;

  is_mrr_assoc= !test(mode & HA_MRR_NO_ASSOCIATION);
  rowids_buf_end= buf->buffer_end;

  elem_size= h_in->ref_length + (int)is_mrr_assoc * sizeof(void*);
  rowids_buf_last= rowids_buf +
                      ((rowids_buf_end - rowids_buf)/ elem_size)*
                      elem_size;
  rowids_buf_end= rowids_buf_last;

  /* Create a separate handler object to do rndpos() calls. */
  Session *session= current_session;
  if (!(new_h2= h_in->clone(session->mem_root)) ||
      new_h2->ha_external_lock(session, F_RDLCK))
  {
    delete new_h2;
    return(1);
  }

  if (keyno == h_in->pushed_idx_cond_keyno)
    pushed_cond= h_in->pushed_idx_cond;
  if (h_in->ha_index_end())
  {
    new_h2= h2;
    goto error;
  }

  h2= new_h2;
  table->prepare_for_position();
  new_h2->extra(HA_EXTRA_KEYREAD);

  if (h2->ha_index_init(keyno, false) ||
      h2->handler::multi_range_read_init(seq_funcs, seq_init_param, n_ranges,
                                         mode, buf))
    goto error;
  use_default_impl= false;

  if (pushed_cond)
    h2->idx_cond_push(keyno, pushed_cond);
  if (dsmrr_fill_buffer(new_h2))
    goto error;

  /*
    If the above call has scanned through all intervals in *seq, then
    adjust *buf to indicate that the remaining buffer space will not be used.
  */
  if (dsmrr_eof)
    buf->end_of_used_area= rowids_buf_last;

  if (h_in->ha_rnd_init(false))
    goto error;

  return(0);
error:
  h2->ha_index_or_rnd_end();
  h2->ha_external_lock(session, F_UNLCK);
  h2->close();
  delete h2;
  return(1);
}


void DsMrr_impl::dsmrr_close()
{
  if (h2)
  {
    h2->ha_external_lock(current_session, F_UNLCK);
    h2->close();
    delete h2;
    h2= NULL;
  }
  use_default_impl= true;
  return;
}


static int rowid_cmp(void *h, unsigned char *a, unsigned char *b)
{
  return ((handler*)h)->cmp_ref(a, b);
}


/**
  DS-MRR: Fill the buffer with rowids and sort it by rowid

  {This is an internal function of DiskSweep MRR implementation}
  Scan the MRR ranges and collect ROWIDs (or {ROWID, range_id} pairs) into
  buffer. When the buffer is full or scan is completed, sort the buffer by
  rowid and return.

  The function assumes that rowids buffer is empty when it is invoked.

  @param h  Table handler

  @retval 0      OK, the next portion of rowids is in the buffer,
                 properly ordered
  @retval other  Error
*/

int DsMrr_impl::dsmrr_fill_buffer(handler *)
{
  char *range_info;
  int res = 0;

  rowids_buf_cur= rowids_buf;
  while ((rowids_buf_cur < rowids_buf_end) &&
         !(res= h2->handler::multi_range_read_next(&range_info)))
  {
    /* Put rowid, or {rowid, range_id} pair into the buffer */
    h2->position(table->record[0]);
    memcpy(rowids_buf_cur, h2->ref, h2->ref_length);
    rowids_buf_cur += h->ref_length;

    if (is_mrr_assoc)
    {
      memcpy(rowids_buf_cur, &range_info, sizeof(void*));
      rowids_buf_cur += sizeof(void*);
    }
  }

  if (res && res != HA_ERR_END_OF_FILE)
    return(res);
  dsmrr_eof= test(res == HA_ERR_END_OF_FILE);

  /* Sort the buffer contents by rowid */
  uint32_t elem_size= h->ref_length + (int)is_mrr_assoc * sizeof(void*);
  uint32_t n_rowids= (rowids_buf_cur - rowids_buf) / elem_size;

  my_qsort2(rowids_buf, n_rowids, elem_size, (qsort2_cmp)rowid_cmp,
            (void*)h);
  rowids_buf_last= rowids_buf_cur;
  rowids_buf_cur=  rowids_buf;
  return(0);
}


/**
  DS-MRR implementation: multi_range_read_next() function
*/

int DsMrr_impl::dsmrr_next(handler *h_in, char **range_info)
{
  int res;

  if (use_default_impl)
    return h_in->handler::multi_range_read_next(range_info);

  if (rowids_buf_cur == rowids_buf_last)
  {
    if (dsmrr_eof)
    {
      res= HA_ERR_END_OF_FILE;
      goto end;
    }
    res= dsmrr_fill_buffer(h);
    if (res)
      goto end;
  }

  /* Return EOF if there are no rowids in the buffer after re-fill attempt */
  if (rowids_buf_cur == rowids_buf_last)
  {
    res= HA_ERR_END_OF_FILE;
    goto end;
  }

  res= h_in->rnd_pos(table->record[0], rowids_buf_cur);
  rowids_buf_cur += h_in->ref_length;
  if (is_mrr_assoc)
  {
    memcpy(range_info, rowids_buf_cur, sizeof(void*));
    rowids_buf_cur += sizeof(void*);
  }

end:
  if (res)
    dsmrr_close();
  return res;
}


/**
  DS-MRR implementation: multi_range_read_info() function
*/
int DsMrr_impl::dsmrr_info(uint32_t keyno, uint32_t n_ranges, uint32_t rows, uint32_t *bufsz,
                           uint32_t *flags, COST_VECT *cost)
{
  int res;
  uint32_t def_flags= *flags;
  uint32_t def_bufsz= *bufsz;

  /* Get cost/flags/mem_usage of default MRR implementation */
  res= h->handler::multi_range_read_info(keyno, n_ranges, rows, &def_bufsz,
                                         &def_flags, cost);
  assert(!res);

  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, rows, &def_flags, &def_bufsz, cost))
  {
    /* Default implementation is choosen */
    *flags= def_flags;
    *bufsz= def_bufsz;
  }
  return 0;
}


/**
  DS-MRR Implementation: multi_range_read_info_const() function
*/

ha_rows DsMrr_impl::dsmrr_info_const(uint32_t keyno, RANGE_SEQ_IF *seq,
                                 void *seq_init_param, uint32_t n_ranges,
                                 uint32_t *bufsz, uint32_t *flags, COST_VECT *cost)
{
  ha_rows rows;
  uint32_t def_flags= *flags;
  uint32_t def_bufsz= *bufsz;
  /* Get cost/flags/mem_usage of default MRR implementation */
  rows= h->handler::multi_range_read_info_const(keyno, seq, seq_init_param,
                                                n_ranges, &def_bufsz,
                                                &def_flags, cost);
  if (rows == HA_POS_ERROR)
  {
    /* Default implementation can't perform MRR scan => we can't either */
    return rows;
  }

  /*
    If HA_MRR_USE_DEFAULT_IMPL has been passed to us, that is an order to
    use the default MRR implementation (we need it for UPDATE/DELETE).
    Otherwise, make a choice based on cost and @@optimizer_use_mrr.
  */
  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, rows, flags, bufsz, cost))
  {
    *flags= def_flags;
    *bufsz= def_bufsz;
  }
  else
  {
    *flags &= ~HA_MRR_USE_DEFAULT_IMPL;
  }
  return rows;
}


/**
  Check if key has partially-covered columns

  We can't use DS-MRR to perform range scans when the ranges are over
  partially-covered keys, because we'll not have full key part values
  (we'll have their prefixes from the index) and will not be able to check
  if we've reached the end the range.

  @param keyno  Key to check

  @todo
    Allow use of DS-MRR in cases where the index has partially-covered
    components but they are not used for scanning.

  @retval true   Yes
  @retval false  No
*/

bool DsMrr_impl::key_uses_partial_cols(uint32_t keyno)
{
  KEY_PART_INFO *kp= table->key_info[keyno].key_part;
  KEY_PART_INFO *kp_end= kp + table->key_info[keyno].key_parts;
  for (; kp != kp_end; kp++)
  {
    if (!kp->field->part_of_key.is_set(keyno))
      return true;
  }
  return false;
}


/**
  DS-MRR Internals: Choose between Default MRR implementation and DS-MRR

  Make the choice between using Default MRR implementation and DS-MRR.
  This function contains common functionality factored out of dsmrr_info()
  and dsmrr_info_const(). The function assumes that the default MRR
  implementation's applicability requirements are satisfied.

  @param keyno       Index number
  @param rows        E(full rows to be retrieved)
  @param flags  IN   MRR flags provided by the MRR user
                OUT  If DS-MRR is choosen, flags of DS-MRR implementation
                     else the value is not modified
  @param bufsz  IN   If DS-MRR is choosen, buffer use of DS-MRR implementation
                     else the value is not modified
  @param cost   IN   Cost of default MRR implementation
                OUT  If DS-MRR is choosen, cost of DS-MRR scan
                     else the value is not modified

  @retval true   Default MRR implementation should be used
  @retval false  DS-MRR implementation should be used
*/

bool DsMrr_impl::choose_mrr_impl(uint32_t keyno, ha_rows rows, uint32_t *flags,
                                 uint32_t *bufsz, COST_VECT *cost)
{
  COST_VECT dsmrr_cost;
  bool res;
  Session *session= current_session;
  if ((session->variables.optimizer_use_mrr == 2) ||
      (*flags & HA_MRR_INDEX_ONLY) || (*flags & HA_MRR_SORTED) ||
      (keyno == table->s->primary_key &&
       h->primary_key_is_clustered()) ||
       key_uses_partial_cols(keyno))
  {
    /* Use the default implementation */
    *flags |= HA_MRR_USE_DEFAULT_IMPL;
    return true;
  }

  uint32_t add_len= table->key_info[keyno].key_length + h->ref_length;
  *bufsz -= add_len;
  if (get_disk_sweep_mrr_cost(keyno, rows, *flags, bufsz, &dsmrr_cost))
    return true;
  *bufsz += add_len;

  bool force_dsmrr;
  /*
    If @@optimizer_use_mrr==force, then set cost of DS-MRR to be minimum of
    DS-MRR and Default implementations cost. This allows one to force use of
    DS-MRR whenever it is applicable without affecting other cost-based
    choices.
  */
  if ((force_dsmrr= (session->variables.optimizer_use_mrr == 1)) &&
      dsmrr_cost.total_cost() > cost->total_cost())
    dsmrr_cost= *cost;

  if (force_dsmrr || dsmrr_cost.total_cost() <= cost->total_cost())
  {
    *flags &= ~HA_MRR_USE_DEFAULT_IMPL;  /* Use the DS-MRR implementation */
    *flags &= ~HA_MRR_SORTED;          /* We will return unordered output */
    *cost= dsmrr_cost;
    res= false;
  }
  else
  {
    /* Use the default MRR implementation */
    res= true;
  }
  return res;
}


static void get_sort_and_sweep_cost(Table *table, ha_rows nrows, COST_VECT *cost);


/**
  Get cost of DS-MRR scan

  @param keynr              Index to be used
  @param rows               E(Number of rows to be scanned)
  @param flags              Scan parameters (HA_MRR_* flags)
  @param buffer_size INOUT  Buffer size
  @param cost        OUT    The cost

  @retval false  OK
  @retval true   Error, DS-MRR cannot be used (the buffer is too small
                 for even 1 rowid)
*/

bool DsMrr_impl::get_disk_sweep_mrr_cost(uint32_t keynr, ha_rows rows, uint32_t flags,
                                         uint32_t *buffer_size, COST_VECT *cost)
{
  uint32_t max_buff_entries, elem_size;
  ha_rows rows_in_full_step, rows_in_last_step;
  uint32_t n_full_steps;
  double index_read_cost;

  elem_size= h->ref_length + sizeof(void*) * (!test(flags & HA_MRR_NO_ASSOCIATION));
  max_buff_entries = *buffer_size / elem_size;

  if (!max_buff_entries)
    return true; /* Buffer has not enough space for even 1 rowid */

  /* Number of iterations we'll make with full buffer */
  n_full_steps= (uint32_t)floor(rows2double(rows) / max_buff_entries);

  /*
    Get numbers of rows we'll be processing in
     - non-last sweep, with full buffer
     - last iteration, with non-full buffer
  */
  rows_in_full_step= max_buff_entries;
  rows_in_last_step= rows % max_buff_entries;

  /* Adjust buffer size if we expect to use only part of the buffer */
  if (n_full_steps)
  {
    get_sort_and_sweep_cost(table, rows, cost);
    cost->multiply(n_full_steps);
  }
  else
  {
    cost->zero();
    *buffer_size= cmax((ulong)*buffer_size,
                      (size_t)(1.2*rows_in_last_step) * elem_size +
                      h->ref_length + table->key_info[keynr].key_length);
  }

  COST_VECT last_step_cost;
  get_sort_and_sweep_cost(table, rows_in_last_step, &last_step_cost);
  cost->add(&last_step_cost);

  if (n_full_steps != 0)
    cost->mem_cost= *buffer_size;
  else
    cost->mem_cost= (double)rows_in_last_step * elem_size;

  /* Total cost of all index accesses */
  index_read_cost= h->index_only_read_time(keynr, (double)rows);
  cost->add_io(index_read_cost, 1 /* Random seeks */);
  return false;
}


/*
  Get cost of one sort-and-sweep step

  SYNOPSIS
    get_sort_and_sweep_cost()
      table       Table being accessed
      nrows       Number of rows to be sorted and retrieved
      cost   OUT  The cost

  DESCRIPTION
    Get cost of these operations:
     - sort an array of #nrows ROWIDs using qsort
     - read #nrows records from table in a sweep.
*/

static
void get_sort_and_sweep_cost(Table *table, ha_rows nrows, COST_VECT *cost)
{
  if (nrows)
  {
    get_sweep_read_cost(table, nrows, false, cost);
    /* Add cost of qsort call: n * log2(n) * cost(rowid_comparison) */
    double cmp_op= rows2double(nrows) * (1.0 / TIME_FOR_COMPARE_ROWID);
    if (cmp_op < 3)
      cmp_op= 3;
    cost->cpu_cost += cmp_op * log2(cmp_op);
  }
  else
    cost->zero();
}


/**
  Get cost of reading nrows table records in a "disk sweep"

  A disk sweep read is a sequence of handler->rnd_pos(rowid) calls that made
  for an ordered sequence of rowids.

  We assume hard disk IO. The read is performed as follows:

   1. The disk head is moved to the needed cylinder
   2. The controller waits for the plate to rotate
   3. The data is transferred

  Time to do #3 is insignificant compared to #2+#1.

  Time to move the disk head is proportional to head travel distance.

  Time to wait for the plate to rotate depends on whether the disk head
  was moved or not.

  If disk head wasn't moved, the wait time is proportional to distance
  between the previous block and the block we're reading.

  If the head was moved, we don't know how much we'll need to wait for the
  plate to rotate. We assume the wait time to be a variate with a mean of
  0.5 of full rotation time.

  Our cost units are "random disk seeks". The cost of random disk seek is
  actually not a constant, it depends one range of cylinders we're going
  to access. We make it constant by introducing a fuzzy concept of "typical
  datafile length" (it's fuzzy as it's hard to tell whether it should
  include index file, temp.tables etc). Then random seek cost is:

    1 = half_rotation_cost + move_cost * 1/3 * typical_data_file_length

  We define half_rotation_cost as DISK_SEEK_BASE_COST=0.9.

  @param table             Table to be accessed
  @param nrows             Number of rows to retrieve
  @param interrupted       true <=> Assume that the disk sweep will be
                           interrupted by other disk IO. false - otherwise.
  @param cost         OUT  The cost.
*/

void get_sweep_read_cost(Table *table, ha_rows nrows, bool interrupted,
                         COST_VECT *cost)
{
  cost->zero();
  if (table->file->primary_key_is_clustered())
  {
    cost->io_count= table->file->read_time(table->s->primary_key,
                                           (uint32_t) nrows, nrows);
  }
  else
  {
    double n_blocks=
      ceil(uint64_t2double(table->file->stats.data_file_length) / IO_SIZE);
    double busy_blocks=
      n_blocks * (1.0 - pow(1.0 - 1.0/n_blocks, rows2double(nrows)));
    if (busy_blocks < 1.0)
      busy_blocks= 1.0;

    cost->io_count= busy_blocks;

    if (!interrupted)
    {
      /* Assume reading is done in one 'sweep' */
      cost->avg_io_cost= (DISK_SEEK_BASE_COST +
                          DISK_SEEK_PROP_COST*n_blocks/busy_blocks);
    }
  }
  return;
}


/* **************************************************************************
 * DS-MRR implementation ends
 ***************************************************************************/

/**
  Read first row between two ranges.

  @param start_key		Start key. Is 0 if no min range
  @param end_key		End key.  Is 0 if no max range
  @param eq_range_arg	        Set to 1 if start_key == end_key
  @param sorted		Set to 1 if result should be sorted per key

  @note
    Record is read into table->record[0]

  @retval
    0			Found row
  @retval
    HA_ERR_END_OF_FILE	No rows in range
  @retval
    \#			Error code
*/
int handler::read_range_first(const key_range *start_key,
			      const key_range *end_key,
			      bool eq_range_arg,
                              bool )
{
  int result;

  eq_range= eq_range_arg;
  end_range= 0;
  if (end_key)
  {
    end_range= &save_end_range;
    save_end_range= *end_key;
    key_compare_result_on_equal= ((end_key->flag == HA_READ_BEFORE_KEY) ? 1 :
				  (end_key->flag == HA_READ_AFTER_KEY) ? -1 : 0);
  }
  range_key_part= table->key_info[active_index].key_part;

  if (!start_key)			// Read first record
    result= index_first(table->record[0]);
  else
    result= index_read_map(table->record[0],
                           start_key->key,
                           start_key->keypart_map,
                           start_key->flag);
  if (result)
    return((result == HA_ERR_KEY_NOT_FOUND)
		? HA_ERR_END_OF_FILE
		: result);

  return (compare_key(end_range) <= 0 ? 0 : HA_ERR_END_OF_FILE);
}


/**
  Read next row between two endpoints.

  @note
    Record is read into table->record[0]

  @retval
    0			Found row
  @retval
    HA_ERR_END_OF_FILE	No rows in range
  @retval
    \#			Error code
*/
int handler::read_range_next()
{
  int result;

  if (eq_range)
  {
    /* We trust that index_next_same always gives a row in range */
    return(index_next_same(table->record[0],
                                end_range->key,
                                end_range->length));
  }
  result= index_next(table->record[0]);
  if (result)
    return(result);
  return(compare_key(end_range) <= 0 ? 0 : HA_ERR_END_OF_FILE);
}


/**
  Compare if found key (in row) is over max-value.

  @param range		range to compare to row. May be 0 for no range

  @seealso
    key.cc::key_cmp()

  @return
    The return value is SIGN(key_in_row - range_key):

    - 0   : Key is equal to range or 'range' == 0 (no range)
    - -1  : Key is less than range
    - 1   : Key is larger than range
*/
int handler::compare_key(key_range *range)
{
  int cmp;
  if (!range || in_range_check_pushed_down)
    return 0;					// No max range
  cmp= key_cmp(range_key_part, range->key, range->length);
  if (!cmp)
    cmp= key_compare_result_on_equal;
  return cmp;
}


/*
  Same as compare_key() but doesn't check have in_range_check_pushed_down.
  This is used by index condition pushdown implementation.
*/

int handler::compare_key2(key_range *range)
{
  int cmp;
  if (!range)
    return 0;					// no max range
  cmp= key_cmp(range_key_part, range->key, range->length);
  if (!cmp)
    cmp= key_compare_result_on_equal;
  return cmp;
}

int handler::index_read_idx_map(unsigned char * buf, uint32_t index,
                                const unsigned char * key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag)
{
  int error, error1;
  error= index_init(index, 0);
  if (!error)
  {
    error= index_read_map(buf, key, keypart_map, find_flag);
    error1= index_end();
  }
  return error ?  error : error1;
}


static bool stat_print(Session *session, const char *type, uint32_t type_len,
                       const char *file, uint32_t file_len,
                       const char *status, uint32_t status_len)
{
  Protocol *protocol= session->protocol;
  protocol->prepareForResend();
  protocol->store(type, type_len, system_charset_info);
  protocol->store(file, file_len, system_charset_info);
  protocol->store(status, status_len, system_charset_info);
  if (protocol->write())
    return true;
  return false;
}

bool ha_show_status(Session *session, StorageEngine *engine, enum ha_stat_type stat)
{
  List<Item> field_list;
  Protocol *protocol= session->protocol;
  bool result;

  field_list.push_back(new Item_empty_string("Type",10));
  field_list.push_back(new Item_empty_string("Name",FN_REFLEN));
  field_list.push_back(new Item_empty_string("Status",10));

  if (protocol->sendFields(&field_list,
                           Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  result= engine->show_status(session, stat_print, stat) ? 1 : 0;

  if (!result)
    session->my_eof();
  return result;
}


/**
  Check if the conditions for row-based binlogging is correct for the table.

  A row in the given table should be replicated if:
  - Row-based replication is enabled in the current thread
  - The binlog is enabled
  - It is not a temporary table
  - The binary log is open
  - The database the table resides in shall be binlogged (binlog_*_db rules)
  - table is not mysql.event
*/

static bool binlog_log_row(Table* table,
                           const unsigned char *before_record,
                           const unsigned char *after_record)
{
  Session *const session= table->in_use;

  switch (session->lex->sql_command)
  {
  case SQLCOM_REPLACE:
  case SQLCOM_INSERT:
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  case SQLCOM_CREATE_TABLE:
    transaction_services.insertRecord(session, table);
    break;

  case SQLCOM_UPDATE:
  case SQLCOM_UPDATE_MULTI:
    transaction_services.updateRecord(session, table, before_record, after_record);
    break;

  case SQLCOM_DELETE:
  case SQLCOM_DELETE_MULTI:
    transaction_services.deleteRecord(session, table);
    break;

    /*
      For everything else we ignore the event (since it just involves a temp table)
    */
  default:
    break;
  }

  return false; //error;
}

int handler::ha_external_lock(Session *session, int lock_type)
{
  /*
    Whether this is lock or unlock, this should be true, and is to verify that
    if get_auto_increment() was called (thus may have reserved intervals or
    taken a table lock), ha_release_auto_increment() was too.
  */
  assert(next_insert_id == 0);

  /*
    We cache the table flags if the locking succeeded. Otherwise, we
    keep them as they were when they were fetched in ha_open().
  */
  DRIZZLE_EXTERNAL_LOCK(lock_type);

  int error= external_lock(session, lock_type);
  if (error == 0)
    cached_table_flags= table_flags();
  return(error);
}


/**
  Check handler usage and reset state of file to after 'open'
*/
int handler::ha_reset()
{
  /* Check that we have called all proper deallocation functions */
  assert((unsigned char*) table->def_read_set.bitmap +
              table->s->column_bitmap_size ==
              (unsigned char*) table->def_write_set.bitmap);
  assert(bitmap_is_set_all(&table->s->all_set));
  assert(table->key_read == 0);
  /* ensure that ha_index_end / ha_rnd_end has been called */
  assert(inited == NONE);
  /* Free cache used by filesort */
  free_io_cache(table);
  /* reset the bitmaps to point to defaults */
  table->default_column_bitmaps();
  return(reset());
}


int handler::ha_write_row(unsigned char *buf)
{
  int error;
  DRIZZLE_INSERT_ROW_START();

  /* 
   * If we have a timestamp column, update it to the current time 
   * 
   * @TODO Technically, the below two lines can be take even further out of the
   * handler interface and into the fill_record() method.
   */
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  mark_trx_read_write();

  if (unlikely(error= write_row(buf)))
    return(error);

  if (unlikely(binlog_log_row(table, 0, buf)))
    return HA_ERR_RBR_LOGGING_FAILED; /* purecov: inspected */

  DRIZZLE_INSERT_ROW_END();
  return(0);
}


int handler::ha_update_row(const unsigned char *old_data, unsigned char *new_data)
{
  int error;

  /*
    Some storage engines require that the new record is in record[0]
    (and the old record is in record[1]).
   */
  assert(new_data == table->record[0]);

  mark_trx_read_write();

  if (unlikely(error= update_row(old_data, new_data)))
    return error;

  if (unlikely(binlog_log_row(table, old_data, new_data)))
    return HA_ERR_RBR_LOGGING_FAILED;

  return 0;
}

int handler::ha_delete_row(const unsigned char *buf)
{
  int error;

  mark_trx_read_write();

  if (unlikely(error= delete_row(buf)))
    return error;

  if (unlikely(binlog_log_row(table, buf, 0)))
    return HA_ERR_RBR_LOGGING_FAILED;

  return 0;
}



/**
  @details
  use_hidden_primary_key() is called in case of an update/delete when
  (table_flags() and HA_PRIMARY_KEY_REQUIRED_FOR_DELETE) is defined
  but we don't have a primary key
*/
void handler::use_hidden_primary_key()
{
  /* fallback to use all columns in the table to identify row */
  table->use_all_columns();
}

void table_case_convert(char * name, uint32_t length)
{
  if (lower_case_table_names)
    files_charset_info->cset->casedn(files_charset_info,
                                     name, length, name, length);
}

const char *table_case_name(HA_CREATE_INFO *info, const char *name)
{
  return ((lower_case_table_names == 2 && info->alias) ? info->alias : name);
}
