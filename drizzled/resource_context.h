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

#include <drizzled/common_fwd.h>

namespace drizzled {

/**
 * Either statement transaction or normal transaction - related
 * session-specific resource manager data state.
 *
 * If a resource manager participates in a statement/transaction,
 * an instance of this class is present in
 * session->transaction.{stmt|all}.resource_contexts.
 *
 * When it's time to commit or rollback, each resource context
 * is used to access the resource manager's prepare()/commit()/rollback()
 * methods, and also to evaluate if a full two phase commit is
 * necessary.
 * 
 * @sa General description of transaction handling in drizzled/transaction_services.cc.
 */
class ResourceContext
{
public:
  ResourceContext() :
    monitored(NULL),
    xa_resource_manager(NULL),
    trx_storage_engine(NULL),
    modified_data(false)
  {}

  /** Clear, prepare for reuse. */
  void reset();

  /**
   * Marks that the underlying resource manager
   * has modified data state.
   */
  void markModifiedData();

  /**
   * Returns true if the underlying resource manager
   * has modified data state.
   */
  bool hasModifiedData() const;

  /**
   * Returns true if the underlying resource
   * manager has registered with the transaction
   * manager for this transaction.
   */
  bool isStarted() const;

  /** 
   * Mark this context as modifying data if the argument has also modified data
   */
  void coalesceWith(const ResourceContext *stmt_trx);

  /**
   * Returns the underlying descriptor for the resource
   * this context tracks.
   */
  plugin::MonitoredInTransaction *getMonitored() const
  {
    return monitored;
  }

  /**
   * Sets the underlying descriptor for the resource
   */
  void setMonitored(plugin::MonitoredInTransaction *in_monitored)
  {
    monitored= in_monitored;
  }

  /**
   * Returns the underlying transactional storage engine
   * this context tracks or NULL if not SQL transactional capable.
   */
  plugin::TransactionalStorageEngine *getTransactionalStorageEngine() const
  {
    return trx_storage_engine;
  }

  /**
   * Sets the underlying transactional storage engine
   */
  void setTransactionalStorageEngine(plugin::TransactionalStorageEngine *in_trx_storage_engine)
  {
    trx_storage_engine= in_trx_storage_engine;
  }

  /**
   * Returns the underlying XA resource manager
   * this context tracks or NULL if not XA capable.
   */
  plugin::XaResourceManager *getXaResourceManager() const
  {
    return xa_resource_manager;
  }

  /**
   * Sets the underlying xa resource manager
   */
  void setXaResourceManager(plugin::XaResourceManager *in_xa_resource_manager)
  {
    xa_resource_manager= in_xa_resource_manager;
  }
private:
  /**
   * A descriptor of the monitored resource
   */
  plugin::MonitoredInTransaction *monitored;
  /**
   * The XA resource manager or NULL if not XA capable.
   */
  plugin::XaResourceManager *xa_resource_manager;
  /**
   * The transactional storage engine or NULL if not SQL transaction capable.
   */
  plugin::TransactionalStorageEngine *trx_storage_engine;
  /**
   * Whether the underlying resource manager has changed
   * some data state.
   */
  bool modified_data;
};

} /* namespace drizzled */

