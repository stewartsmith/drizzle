/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 * @file
 *
 * Defines the API of a class which acts as an index into a Command log file.
 *
 * @details
 *
 * This class allows lookups of log offsets based on global transaction ID as
 * well as timestamps.  The former lookup is used by the CommandLogReader class
 * and the latter lookup is used indirectly by Publisher and Subscriber plugins.
 */

#ifndef DRIZZLE_PLUGIN_COMMAND_LOG_INDEX_H
#define DRIZZLE_PLUGIN_COMMAND_LOG_INDEX_H

#include "command_log.h"

#include <drizzled/replication_services.h>

#include <pthread.h>
#include <map>

/**
 * A class which acts as an index into a log file containing
 * Command messages
 */
class CommandLogIndex
{
private:
  /** Lowest transaction ID of a command contained in the log file */
  drizzled::ReplicationServices::GlobalTransactionId min_transaction_id;
  /** Highest transaction ID of a command contained in the log file */
  drizzled::ReplicationServices::GlobalTransactionId max_transaction_id;
  /** A lock for the index */
  pthread_mutex_t lock;
  /** */
  std::map<drizzled::ReplicationServices::GlobalTransactionId, off_t> trx_id_offset_map;
public:
  CommandLogIndex();
  /** Destructor */
  ~CommandLogIndex();
  /** 
   * Returns whether this log file contains a command with a specific
   * global transaction ID.
   *
   * @param[in] global transaction ID to lookup
   *
   * @retval
   *  true if this log file contains the command needed
   * @retval
   *  false otherwise
   */
  bool contains(const drizzled::ReplicationServices::GlobalTransactionId &to_find);
  /**
   * Supply a global transaction ID and an offset into the log file, stores
   * a record of the transaction ID for lookup...
   *
   * @param[in] Global transaction ID
   * @param[in] Offset in log file where to start reading for this command
   */
  void addRecord(const drizzled::ReplicationServices::GlobalTransactionId &in_trx_id,
                 const off_t in_offset);
  /**
   * Returns the offset in the log file of the command with
   * the supplied global transaction ID.
   *
   * @note No bounds checking is done on the key.  It is assumed the caller
   * has already done bounds checking with the containsTransactionId() method.
   *
   * @param[in] Global transaction ID of command to find offset for
   */
  off_t getOffset(const drizzled::ReplicationServices::GlobalTransactionId &to_find);
};

#endif /* DRIZZLE_PLUGIN_COMMAND_LOG_INDEX_H */
