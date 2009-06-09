/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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

#ifndef DRIZZLED_TRANSACTION_SERVICES_H
#define DRIZZLED_TRANSACTION_SERVICES_H

#include "drizzled/atomics.h"
#include <vector>

/* some forward declarations needed */
class Session;
class Table;

namespace drizzled
{
  namespace plugin
  {
    class Replicator;
    class Applier;
  }
  namespace message
  {
    class Command;
  }
}

void add_replicator(drizzled::plugin::Replicator *replicator);
void remove_replicator(drizzled::plugin::Replicator *replicator);

void add_applier(drizzled::plugin::Applier *applier);
void remove_applier(drizzled::plugin::Applier *applier);

/**
 * This is a class which manages transforming internal 
 * transactional events into GPB messages and sending those
 * events out through registered replicators and appliers.
 */
namespace drizzled
{
class TransactionServices
{
private:
  /** 
   * Atomic boolean set to true if any *active* replicators
   * or appliers are actually registered.
   */
  atomic<bool> is_active;
  /** Our collection of replicator plugins */
  std::vector<drizzled::plugin::Replicator *> replicators;
  /** Our collection of applier plugins */
  std::vector<drizzled::plugin::Applier *> appliers;
  /**
   * Helper method which is called after any change in the
   * registered appliers or replicators to evaluate whether
   * any remaining plugins are actually active.
   * 
   * This method properly sets the is_active member variable.
   */
  void evaluateActivePlugins();
  /** 
   * Helper method which attaches a transaction context
   * the supplied command based on the supplied Session's
   * transaction information.
   */
  void setCommandTransactionContext(drizzled::message::Command *in_command, Session *in_session) const;
  /**
   * Helper method which pushes a constructed message out
   * to the registered replicator and applier plugins.
   *
   * @param Message to push out
   */
  void push(drizzled::message::Command *to_push);
public:
  /**
   * Constructor
   */
  TransactionServices();
  /**
   * Returns whether the TransactionServices object
   * is active.  In other words, does it have both
   * a replicator and an applier that are *active*?
   */
  bool isActive() const;
  /**
   * Attaches a replicator to our internal collection of
   * replicators.
   *
   * @param Pointer to a replicator to attach/register
   */
  void attachReplicator(drizzled::plugin::Replicator *in_replicator);
  /**
   * Detaches/unregisters a replicator with our internal
   * collection of replicators.
   *
   * @param Pointer to the replicator to detach
   */
  void detachReplicator(drizzled::plugin::Replicator *in_replicator);
  /**
   * Attaches a applier to our internal collection of
   * appliers.
   *
   * @param Pointer to a applier to attach/register
   */
  void attachApplier(drizzled::plugin::Applier *in_applier);
  /**
   * Detaches/unregisters a applier with our internal
   * collection of appliers.
   *
   * @param Pointer to the applier to detach
   */
  void detachApplier(drizzled::plugin::Applier *in_applier);
  /**
   * Creates a new StartTransaction GPB message and pushes
   * it to replicators.
   *
   * @param Pointer to the Session starting the transaction
   */
  void startTransaction(Session *in_session);
  /**
   * Creates a new CommitTransaction GPB message and pushes
   * it to replicators.
   *
   * @param Pointer to the Session committing the transaction
   */
  void commitTransaction(Session *in_session);
  /**
   * Creates a new RollbackTransaction GPB message and pushes
   * it to replicators.
   *
   * @param Pointer to the Session committing the transaction
   */
  void rollbackTransaction(Session *in_session);
  /**
   * Creates a new InsertRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has inserted a record
   * @param Pointer to the Table containing insert information
   */
  void insertRecord(Session *in_session, Table *in_table);
  /**
   * Creates a new UpdateRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has updated a record
   * @param Pointer to the Table containing update information
   * @param Pointer to the raw bytes representing the old record/row
   * @param Pointer to the raw bytes representing the new record/row 
   */
  void updateRecord(Session *in_session, 
                    Table *in_table, 
                    const unsigned char *old_record, 
                    const unsigned char *new_record);
  /**
   * Creates a new DeleteRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has deleted a record
   * @param Pointer to the Table containing delete information
   */
  void deleteRecord(Session *in_session, Table *in_table);
  /**
   * Creates a new RawSql GPB message and pushes it to 
   * replicators.
   *
   * @TODO With a real data dictionary, this really shouldn't
   * be needed.  CREATE TABLE would map to insertRecord call
   * on the I_S, etc.  Not sure what to do with administrative
   * commands like CHECK TABLE, though..
   *
   * @param Pointer to the Session which issued the statement
   * @param Query string
   * @param Length of the query string
   */
  void rawStatement(Session *in_session, const char *in_query, size_t in_query_len);
};

} /* end namespace drizzled */

#endif /* DRIZZLED_TRANSACTION_SERVICES_H */
