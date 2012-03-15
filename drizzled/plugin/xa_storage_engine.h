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

#pragma once

#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/plugin/xa_resource_manager.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

/**
 * A type of storage engine which supports distributed
 * transactions in the XA protocol.
 *
 * The real XA resource manager interface is in the
 * plugin::XaResourceManager class.  We would extend
 * XaResourceManager from plugin::Plugin but unfortunately
 * that would lead to member name ambiguity (because plugin::Plugin
 * has member data).  So, in this case, TransactionalStorageEngine
 * inherits from plugin::Plugin and XaResourceManager is a pure
 * virtual abstract base class with the X/Open XA distributed
 * transaction protocol interface for resource managers.
 */
class DRIZZLED_API XaStorageEngine :
  public TransactionalStorageEngine,
  public XaResourceManager
{
public:
  XaStorageEngine(const std::string &name_arg,
                  const std::bitset<HTON_BIT_SIZE> &flags_arg= HTON_NO_FLAGS);

  virtual ~XaStorageEngine();

  int startTransaction(Session *session, start_transaction_option_t options)
  {
    TransactionServices::registerResourceForTransaction(*session, this, this, this);
    return doStartTransaction(session, options);
  }

  void startStatement(Session *session)
  {
    TransactionServices::registerResourceForStatement(*session, this, this, this);
    doStartStatement(session);
  }

  /* 
   * The below are simple virtual overrides for the plugin::MonitoredInTransaction
   * interface.
   */
  bool participatesInSqlTransaction() const
  {
    return true; /* We DO participate in the SQL transaction */
  }
  bool participatesInXaTransaction() const
  {
    return true; /* We DO participate in the XA transaction */
  }
  bool alwaysRegisterForXaTransaction() const
  {
    return false; /* We only register in the XA transaction if the engine's data is modified */
  }

  /* Class Methods for operating on plugin */
  static bool addPlugin(plugin::XaStorageEngine *engine);
  static void removePlugin(plugin::XaStorageEngine *engine);

private:
  /*
   * Indicates to a storage engine the start of a
   * new SQL transaction.  This is called ONLY in the following
   * scenarios:
   *
   * 1) An explicit BEGIN WORK/START TRANSACTION is called
   * 2) After an explicit COMMIT AND CHAIN is called
   * 3) After an explicit ROLLBACK AND RELEASE is called
   * 4) When in AUTOCOMMIT mode and directly before a new
   *    SQL statement is started.
   *
   * Engines should typically use the doStartStatement()
   * and doEndStatement() methods to manage transaction state,
   * since the kernel ALWAYS notifies engines at the start
   * and end of statement transactions and at the end of the
   * normal transaction by calling doCommit() or doRollback().
   */
  virtual int doStartTransaction(Session *session, start_transaction_option_t options)
  {
    (void) session;
    (void) options;
    return 0;
  }

  /*
   * Indicates to a storage engine the start of a
   * new SQL statement.
   */
  virtual void doStartStatement(Session *session)
  {
    (void) session;
  }
};

} /* namespace plugin */
} /* namespace drizzled */

