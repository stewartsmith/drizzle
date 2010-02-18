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

#ifndef DRIZZLED_PLUGIN_TRANSACTIONAL_STORAGE_ENGINE_H
#define DRIZZLED_PLUGIN_TRANSACTIONAL_STORAGE_ENGINE_H

#include "drizzled/plugin/storage_engine.h"
#include "drizzled/transaction_services.h"

namespace drizzled
{

namespace plugin
{

/**
 * A type of storage engine which supports SQL transactions.
 *
 * This class adds the SQL transactional API to the regular
 * storage engine.  In other words, it adds support for the
 * following SQL statements:
 *
 * START TRANSACTION;
 * COMMIT;
 * ROLLBACK;
 * ROLLBACK TO SAVEPOINT;
 * SET SAVEPOINT;
 * RELEASE SAVEPOINT;
 *
 * @note
 *
 * This class does not implement the XA protocol (two phase commit).
 * There is an XaStorageEngine class which extends this class that
 * exposes the XA API.
 *
 * @todo
 *
 * kill two_phase_commit member. Use an HTON flag if
 * absolutely needed to keep.
 */
class TransactionalStorageEngine :public StorageEngine
{
public:
  TransactionalStorageEngine(const std::string name_arg,
                             const std::bitset<HTON_BIT_SIZE> &flags_arg= HTON_NO_FLAGS,
                             bool two_phase_commit= false);

  virtual ~TransactionalStorageEngine();

  void startStatement(Session *session)
  {
    TransactionServices &transaction_services= TransactionServices::singleton();
    transaction_services.registerResourceForStatement(session, this);
    doStartStatement(session);
  }

  int commit(Session *session, bool normal_transaction)
  {
    return doCommit(session, normal_transaction);
  }

  int rollback(Session *session, bool normal_transaction)
  {
    return doRollback(session, normal_transaction);
  }

  int setSavepoint(Session *session, NamedSavepoint &sp)
  {
    return doSetSavepoint(session, sp);
  }

  int rollbackToSavepoint(Session *session, NamedSavepoint &sp)
  {
     return doRollbackToSavepoint(session, sp);
  }

  int releaseSavepoint(Session *session, NamedSavepoint &sp)
  {
    return doReleaseSavepoint(session, sp);
  }

  bool hasTwoPhaseCommit()
  {
    return two_phase_commit;
  }

  /** 
   * The below static class methods wrap the interaction
   * of the vector of transactional storage engines.
   *
   * @todo kill these. they belong in TransactionServices.
   */
  /**
   * @todo Kill this one entirely.  It's implementation, not interface...
   */
  static int releaseTemporaryLatches(Session *session);
  static int startConsistentSnapshot(Session *session);

  /* Class Methods for operating on plugin */
  static bool addPlugin(plugin::TransactionalStorageEngine *engine);
  static void removePlugin(plugin::TransactionalStorageEngine *engine);

private:
  void setTransactionReadWrite(Session& session);

  /*
   * Indicates to a storage engine the start of a
   * new SQL statement.
   */
  virtual void doStartStatement(Session *session)
  {
    (void) session;
  }

  /*
   * Indicates to a storage engine the end of
   * the current SQL statement in the supplied
   * Session.
   */
  virtual void doEndStatement(Session *session)
  {
    (void) session;
  }
  /**
   * Implementing classes should override these to provide savepoint
   * functionality.
   */
  virtual int doSetSavepoint(Session *session, NamedSavepoint &savepoint)= 0;
  virtual int doRollbackToSavepoint(Session *session, NamedSavepoint &savepoint)= 0;
  virtual int doReleaseSavepoint(Session *session, NamedSavepoint &savepoint)= 0;
  
  /**
   * Commits either the "statement transaction" or the "normal transaction".
   *
   * @param[in] The Session
   * @param[in] true if it's a real commit, that makes persistent changes
   *            false if it's not in fact a commit but an end of the
   *            statement that is part of the transaction.
   * @note
   *
   * 'normal_transaction' is also false in auto-commit mode where 'end of statement'
   * and 'real commit' mean the same event.
   */
  virtual int doCommit(Session *session, bool normal_transaction)= 0;

  /**
   * Rolls back either the "statement transaction" or the "normal transaction".
   *
   * @param[in] The Session
   * @param[in] true if it's a real commit, that makes persistent changes
   *            false if it's not in fact a commit but an end of the
   *            statement that is part of the transaction.
   * @note
   *
   * 'normal_transaction' is also false in auto-commit mode where 'end of statement'
   * and 'real commit' mean the same event.
   */
  virtual int doRollback(Session *session, bool normal_transaction)= 0;
  virtual int doReleaseTemporaryLatches(Session *session)
  {
    (void) session;
    return 0;
  }
  virtual int doStartConsistentSnapshot(Session *session)
  {
    (void) session;
    return 0;
  }
  const bool two_phase_commit;
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_TRANSACTIONAL_STORAGE_ENGINE_H */
