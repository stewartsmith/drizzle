/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *  Copyright (C) 2009-2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  Authors:
 *
 *    Jay Pipes <jaypipes@gmail.com>
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

#ifndef DRIZZLED_REPLICATION_SERVICES_H
#define DRIZZLED_REPLICATION_SERVICES_H

#include "drizzled/atomics.h"
#include "drizzled/plugin/replication.h"

#include <string>
#include <vector>
#include <utility>

#include "drizzled/visibility.h"

namespace drizzled
{

/* some forward declarations needed */
class Session;
class Table;

namespace plugin
{
  class TransactionReplicator;
  class TransactionApplier;
}
namespace message
{
  class Transaction;
}

/**
 * This is a class which manages transforming internal 
 * transactional events into GPB messages and sending those
 * events out through registered replicators and appliers.
 */
class DRIZZLED_API ReplicationServices
{
public:
  typedef uint64_t GlobalTransactionId;
  /**
   * Types of messages that can go in the transaction
   * log file.  Every time something is written into the
   * transaction log, it is preceded by a header containing
   * the type of message which follows.
   */
  enum MessageType
  {
    TRANSACTION= 1, /* A GPB Transaction Message */
    BLOB= 2 /* A BLOB value */
  };
  typedef std::pair<plugin::TransactionReplicator *, plugin::TransactionApplier *> ReplicationPair;
  typedef std::vector<ReplicationPair> ReplicationStreams;
  /**
   * Method which is called after plugins have been loaded but
   * before the first client connects.  It determines if the registration
   * of applier and replicator plugins is proper and pairs
   * the applier and requested replicator plugins into the replication
   * streams.
   *
   * @todo
   *
   * This is only necessary because we don't yet have plugin dependency
   * tracking...
   */
  bool evaluateRegisteredPlugins();
  /** 
   * Helper method which pushes a constructed message out to the registered
   * replicator and applier plugins.
   *
   * @param Session descriptor
   * @param Message to push out
   */
  plugin::ReplicationReturnCode pushTransactionMessage(Session &in_session,
                                                       message::Transaction &to_push);
  /**
   * Constructor
   */
  ReplicationServices();

  /**
   * Singleton method
   * Returns the singleton instance of ReplicationServices
   */
  static inline ReplicationServices &singleton()
  {
    static ReplicationServices replication_services;
    return replication_services;
  }

  /**
   * Returns whether the ReplicationServices object
   * is active.  In other words, does it have both
   * a replicator and an applier that are *active*?
   */
  bool isActive() const;

  /**
   * Returns the list of replication streams
   */
  ReplicationStreams &getReplicationStreams();

  /**
   * Attaches a replicator to our internal collection of
   * replicators.
   *
   * @param Pointer to a replicator to attach/register
   */
  void attachReplicator(plugin::TransactionReplicator *in_replicator);
  
  /**
   * Detaches/unregisters a replicator with our internal
   * collection of replicators.
   *
   * @param Pointer to the replicator to detach
   */
  void detachReplicator(plugin::TransactionReplicator *in_replicator);
  
  /**
   * Attaches a applier to our internal collection of
   * appliers.
   *
   * @param Pointer to a applier to attach/register
   * @param The name of the replicator to pair with
   */
  void attachApplier(plugin::TransactionApplier *in_applier, const std::string &requested_replicator);
  
  /**
   * Detaches/unregisters a applier with our internal
   * collection of appliers.
   *
   * @param Pointer to the applier to detach
   */
  void detachApplier(plugin::TransactionApplier *in_applier);

  /** 
   * Returns the timestamp of the last Transaction which was sent to an
   * applier.
   */
  uint64_t getLastAppliedTimestamp() const;
private:
  typedef std::vector<plugin::TransactionReplicator *> Replicators;
  typedef std::vector<std::pair<std::string, plugin::TransactionApplier *> > Appliers;
  /** 
   * Atomic boolean set to true if any *active* replicators
   * or appliers are actually registered.
   */
  bool is_active;
  /**
   * The timestamp of the last time a Transaction message was successfully
   * applied (sent to an Applier)
   */
  atomic<uint64_t> last_applied_timestamp;
  /** Our collection of registered replicator plugins */
  Replicators replicators;
  /** Our collection of registered applier plugins and their requested replicator plugin names */
  Appliers appliers;
  /** Our replication streams */
  ReplicationStreams replication_streams;
  /**
   * Strips underscores and lowercases supplied replicator name
   * or requested name, and appends the suffix "replicator" if missing...
   */
  void normalizeReplicatorName(std::string &name);
};

} /* namespace drizzled */

#endif /* DRIZZLED_REPLICATION_SERVICES_H */
