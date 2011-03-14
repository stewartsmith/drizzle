/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
 * Defines the API of a simple index for the transaction log
 */

#pragma once

#include "transaction_log.h"
#include "transaction_log_entry.h"

#include <pthread.h>
#include <string>

namespace drizzled { namespace message {class Transaction;}}

class TransactionLogIndex
{
public:
  explicit TransactionLogIndex(TransactionLog &in_log);
  ~TransactionLogIndex();
  /**
   * Returns the minimum end timestamp of a transaction
   * in the transaction log.
   */
  uint64_t getMinEndTimestamp() const;
  /**
   * Returns the maximum end timestamp of a transaction
   * in the transaction log.
   */
  uint64_t getMaxEndTimestamp() const;
  /**
   * Returns the minimum transaction ID of a transaction
   * in the transaction log.
   */
  uint64_t getMinTransactionId() const;
  /**
   * Returns the maximum transaction ID of a transaction
   * in the transaction log.
   */
  uint64_t getMaxTransactionId() const;
  /**
   * Returns the total number of entries in the transaction log
   */
  uint64_t getNumLogEntries() const;
  /**
   * Returns the total number of transaction entries in the transaction log
   */
  uint64_t getNumTransactionEntries() const;
  /**
   * Returns whether the index encountered an
   * error on its last action.
   */
  bool hasError() const;
  /**
   * Returns the current error message
   */
  const std::string &getErrorMessage() const;
  /**
   * Returns a reference to the index's collection
   * of log entry objects
   */
  TransactionLog::Entries &getEntries();
  /**
   * Returns a reference to the index's collection
   * of transaction entry objects
   */
  TransactionLog::TransactionEntries &getTransactionEntries();
  /**
   * Adds a new entry to the index of type Transaction message.
   *
   * @param[in] The transaction log entry
   * @param[in] The transaction message
   * @param[in] The checksum for the transaction message bytes
   */
  void addEntry(const TransactionLogEntry &entry,
                const drizzled::message::Transaction &transaction,
                uint32_t checksum);
  /**
   * Clears all data out of the transaction log
   * index.
   *
   * @note
   *
   * No locks are taken here.  Currently only used in debugging.
   */
  void clear();

  /* Some methods returning size in bytes of the index and its parts */
  size_t getTransactionEntriesSizeInBytes();
  size_t getEntriesSizeInBytes();
  size_t getSizeInBytes();
private:
  /* Don't allows these */
  TransactionLogIndex();
  TransactionLogIndex(const TransactionLogIndex &other);
  TransactionLogIndex &operator=(const TransactionLogIndex &other);
  /**
   * Helper function to open/create the index from 
   * the transaction log.
   */
  void open();
  /**
   * Clears the internal error state
   */
  void clearError();

  TransactionLog &log; ///< The transaction log instance
  int index_file; ///< File descriptor for the transaction log on-disk index file
  const std::string index_file_path; ///< Filename of the on-disk transaction log index
  bool has_error; ///< Index is in error mode?
  std::string error_message; ///< Current error message

  uint64_t min_end_timestamp; ///< Minimum end timestamp in log
  uint64_t max_end_timestamp; ///< Maximim end timestamp in log
  uint64_t min_transaction_id; ///< Minimum transaction ID in log
  uint64_t max_transaction_id; ///< Maximum transaction ID in log

  TransactionLog::Entries entries; ///< Collection of information about the entries in the log
  TransactionLog::TransactionEntries transaction_entries; ///<

  pthread_mutex_t index_lock; ///< The global index lock
};

