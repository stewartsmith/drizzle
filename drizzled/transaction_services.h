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

#ifndef DRIZZLED_TRANSACTION_SERVICES_H
#define DRIZZLED_TRANSACTION_SERVICES_H

/* some forward declarations needed */
class Session;

namespace drizzled
{
  namespace plugin
  {
    class StorageEngine;
  }

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

  /* these are called by storage engines */
  void trans_register_ha(Session *session, bool all, plugin::StorageEngine *engine);
};

} /* namespace drizzled */

#endif /* DRIZZLED_TRANSACTION_SERVICES_H */
