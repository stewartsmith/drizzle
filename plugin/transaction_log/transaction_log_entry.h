/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 *  Authors:
 *
 *  Jay Pipes <joinfu@sun.com>
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
 * Defines a simple structure representing a transaction log entry.
 */

#pragma once

#include <drizzled/replication_services.h>

#include <string>

/**
 * Represents a single entry in the transaction log.
 */
class TransactionLogEntry
{
public:
  TransactionLogEntry(drizzled::ReplicationServices::MessageType in_type,
                      off_t in_offset,
                      size_t length);
  ~TransactionLogEntry();
  /**
   * Returns a string representation of the entry type
   */
  const char *getTypeAsString() const;
  /**
   * Returns the entry's offset in the log
   */
  off_t getOffset() const;
  /**
   * Returns the length of the entry in bytes
   */
  size_t getLengthInBytes() const;
private:
  enum drizzled::ReplicationServices::MessageType type; ///< The type of the entry
  off_t offset; ///< Offset into the log file
  size_t length; ///< Length in bytes of the entry
};

class TransactionLogTransactionEntry
{
public:
  TransactionLogTransactionEntry(off_t in_offset,
                                 const drizzled::message::Transaction &transaction,
                                 uint32_t in_checksum);
  ~TransactionLogTransactionEntry();
  /**
   * Returns the entry's offset in the log
   */
  off_t getOffset() const;
  /**
   * Returns the transaction's server ID
   */
  uint32_t getServerId() const;
  /**
   * Returns the transaction's start timestamp
   */
  uint64_t getStartTimestamp() const;
  /**
   * Returns the transaction's end timestamp
   */
  uint64_t getEndTimestamp() const;
  /**
   * Returns the transaction's ID
   */
  uint64_t getTransactionId() const;
  /**
   * Returns the number of statements in the transaction
   */
  uint64_t getNumStatements() const;
  /**
   * Returns the checksum for the transaction message bytes
   */
  uint32_t getChecksum() const;
private:
  off_t offset; ///< Offset into the log file
  uint32_t server_id; ///< The server ID that this transaction came from
  uint64_t transaction_id; ///< The transaction's ID
  uint64_t start_timestamp; ///< The transaction's start timestamp
  uint64_t end_timestamp; ///< The transaction's end timestamp
  uint32_t num_statements; ///< Number of Statements in the transaction
  uint32_t checksum; ///< Checksum of the transaction message bytes
};

