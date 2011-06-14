/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

#include <cstring>

#include <drizzled/visibility.h>

namespace drizzled
{

namespace plugin
{

extern size_t num_trx_monitored_objects;

/**
 * An abstract interface class for those objects which are tracked
 * by the TransactionServices component during operations in a
 * transaction.
 *
 * Note that both non-transactional plugin::StorageEngines, non-XA
 * plugin::TransactionalStorageEngines, and plugin::XaResourceManager
 * objects are all tracked by the transaction manager in TransactionServices.
 *
 * Implementing classes should inherit *publically* from
 * plugin::MonitoredInTransaction, as public inheritance means
 * "is a" and is the appropriate use here since all implementing classes
 * *are* monitored in a transaction...
 */
class DRIZZLED_API MonitoredInTransaction
{
public:

  MonitoredInTransaction();
  virtual ~MonitoredInTransaction() {}

  /**
   * Returns true if the class should participate
   * in the SQL transaction.
   */
  virtual bool participatesInSqlTransaction() const= 0;

  /**
   * Returns true if the class should participate
   * in the XA transaction.
   */
  virtual bool participatesInXaTransaction() const= 0;

  /**
   * Returns true if the class should be registered
   * for every XA transaction regardless of whether
   * the class modifies the server's state.
   *
   * @note
   *
   * As an example, the XaTransactionApplier plugin class returns
   * true for this virtual method.  Even though it does not
   * change the result of the transaction (it simply is logging
   * the changes made by other resource managers), the applier
   * plugin should be enlisted in all XA transactions in order
   * to be able to rollback or recover its logging activity
   * properly.
   */
  virtual bool alwaysRegisterForXaTransaction() const= 0;

  /**
   * Returns the "slot" or ID of the monitored resource
   */
  size_t getId() const
  {
    return id;
  }
private:
  /**
   * The ID or "slot" of the plugin.
   *
   * @todo
   *
   * Maybe move this into plugin::Plugin?  Only issue then is
   * that all plugins would have a ha_data slot, when only a few
   * actually need that.  Maybe create a plugin::NeedsSessionData?
   */
  size_t id;
};

} /* namespace plugin */
} /* namespace drizzled */

