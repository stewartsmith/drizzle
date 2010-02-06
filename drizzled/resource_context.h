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

#ifndef DRIZZLED_RESOURCE_CONTEXT_H
#define DRIZZLED_RESOURCE_CONTEXT_H

#include <cstddef>

namespace drizzled
{

namespace plugin
{
class StorageEngine;
}

/**
 * Either statement transaction or normal transaction - related
 * session-specific storage engine data.
 *
 * If a storage engine participates in a statement/transaction,
 * an instance of this class is present in
 * session->transaction.{stmt|all}.resource_contexts.
 *
 * When it's time to commit or rollback, each element of ha_list
 * is used to access resource manager's prepare()/commit()/rollback()
 * methods, and also to evaluate if a full two phase commit is
 * necessary.
 * 
 * @sa General description of transaction handling in drizzled/transaction_services.cc.
 */
class ResourceContext
{
public:
  ResourceContext() :
    resource(NULL),
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
   * Returns the underlying resource manager
   * this context tracks.
   */
  drizzled::plugin::StorageEngine *getResource() const
  {
    return resource;
  }

  /**
   * Sets the underlying resource
   */
  void setResource(drizzled::plugin::StorageEngine *in_engine)
  {
    resource= in_engine;
  }
private:
  /**
    Although a given ResourceContext instance is always used
    for the same resource manager, 'resource' is not-NULL only when the
    corresponding resource manager is a part of a transaction.
  */
  drizzled::plugin::StorageEngine *resource;
  /**
   * Whether the underlying resource manager has changed
   * some data state.
   */
  bool modified_data;
};

} /* namespace drizzled */

#endif /* DRIZZLED_RESOURCE_CONTEXT_H */
