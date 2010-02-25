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

#ifndef DRIZZLED_TRANSACTION_SERVICES_H
#define DRIZZLED_TRANSACTION_SERVICES_H

namespace drizzled
{

/* some forward declarations needed */
namespace plugin
{
  class TransactionalStorageEngine;
}

class Session;
class NamedSavepoint;

/**
 * This is a class which manages the XA transaction processing
 * in the server
 */
class TransactionServices
{
public:
  /**
   * Constructor
   */
  TransactionServices() {}

  /**
   * Singleton method
   * Returns the singleton instance of TransactionServices
   */
  static inline TransactionServices &singleton()
  {
    static TransactionServices transaction_services;
    return transaction_services;
  }
  /* transactions: interface to plugin::StorageEngine functions */
  int ha_commit_one_phase(Session *session, bool all);
  int ha_rollback_trans(Session *session, bool all);

  /* transactions: these functions never call plugin::StorageEngine functions directly */
  int ha_commit_trans(Session *session, bool all);
  int ha_autocommit_or_rollback(Session *session, int error);

  /* savepoints */
  int ha_rollback_to_savepoint(Session *session, NamedSavepoint &sv);
  int ha_savepoint(Session *session, NamedSavepoint &sv);
  int ha_release_savepoint(Session *session, NamedSavepoint &sv);
  bool mysql_xa_recover(Session *session);

  /**
   * Marks a storage engine as participating in a statement
   * transaction.
   *
   * @note
   * 
   * This method is idempotent
   *
   * @todo
   *
   * This method should not be called more than once per resource
   * per statement, and therefore should not need to be idempotent.
   * Put in assert()s to test this.
   *
   * @param[in] Session pointer
   * @param[in] Resource which will be participating
   */
  void registerResourceForStatement(Session *session,
                                    plugin::TransactionalStorageEngine *engine);

  /**
   * Registers a resource manager in the "normal" transaction.
   *
   * @note
   *
   * This method is idempotent and must be idempotent
   * because it can be called both by the above 
   * TransactionServices::registerResourceForStatement(),
   * which occurs at the beginning of each SQL statement,
   * and also manually when a BEGIN WORK/START TRANSACTION
   * statement is executed. If the latter case (BEGIN WORK)
   * is called, then subsequent contained statement transactions
   * will call this method as well.
   *
   * @note
   *
   * This method checks to see if the supplied resource
   * is also registered in the statement transaction, and
   * if not, registers the resource in the statement
   * transaction.  This happens ONLY when the user has
   * called BEGIN WORK/START TRANSACTION, which is the only
   * time when this method is called except from the
   * TransactionServices::registerResourceForStatement method.
   */
  void registerResourceForTransaction(Session *session,
                                      plugin::TransactionalStorageEngine *engine);
};

} /* namespace drizzled */

#endif /* DRIZZLED_TRANSACTION_SERVICES_H */
