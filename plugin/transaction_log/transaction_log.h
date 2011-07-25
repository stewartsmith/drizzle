/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  Authors:
 *
 *  Jay Pipes <jaypipes@gmail.com.com>
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
 * Defines the API of the transaction log file descriptor.
 *
 * @details
 *
 * Basically, the TransactionLog is a descriptor for a log
 * file containing transaction messages that is written by
 * the TransactionLogApplier plugin(s).
 */

#pragma once

#include <drizzled/atomics.h>
#include <drizzled/replication_services.h>

#include "transaction_log_entry.h"

#include <vector>
#include <string>

class TransactionLog
{
public:
  typedef std::vector<TransactionLogEntry> Entries;
  typedef std::vector<TransactionLogTransactionEntry> TransactionEntries;
  /**
   * The state the log is in
   */
  enum Status
  {
    CRASHED= 0,
    OFFLINE, /* Default state, uninited. */
    ONLINE,
    WRITING
  };
  static const uint32_t FLUSH_FREQUENCY_OS= 0; ///< Rely on operating system to sync log file
  static const uint32_t FLUSH_FREQUENCY_EVERY_WRITE= 1; //< Sync on every write to the log file
  static const uint32_t FLUSH_FREQUENCY_EVERY_SECOND= 2; ///< Sync no more than once a second
public:
  TransactionLog(const std::string &in_log_file_path,
                 uint32_t in_flush_frequency,
                 bool in_do_checksum);

  /** Destructor */
  ~TransactionLog();

  /**
   * Returns the current offset into the log
   */
  inline off_t getLogOffset()
  {
    return log_offset;
  }

  /**
   * Returns the filename of the transaction log
   */
  const std::string &getLogFilename();

  /**
   * Returns the filename of the transaction log
   */
  const std::string &getLogFilepath();
  
  /**
   * Returns the state that the log is in
   */
  inline enum Status getState()
  {
    return state;
  }

  /**
   * Static helper method which returns the transaction
   * log entry size in bytes of a given transaction
   * message.
   *
   * @param[in] Transaction message
   */
  static size_t getLogEntrySize(const drizzled::message::Transaction &trx);

  /**
   * Method which packs into a raw byte buffer
   * a transaction log entry.  Supplied buffer should
   * be of adequate size.
   *
   * Returns a pointer to the start of the original
   * buffer.
   *
   * @param[in]   Transaction message to pack
   * @param[in]   Raw byte buffer
   * @param[out]  Pointer to storage for checksum of message
   */
  uint8_t *packTransactionIntoLogEntry(const drizzled::message::Transaction &trx,
                                       uint8_t *buffer,
                                       uint32_t *checksum_out);

  /**
   * Writes a chunk of data to the log file of a specified
   * length and returns the offset at which the chunk of
   * data was written.
   *
   * @param[in] Bytes to write
   * @param[in[ Length of bytes to write
   *
   * @retval
   *  Returns the write offset if the write succeeded, OFF_T_MAX otherwise.
   */
  off_t writeEntry(const uint8_t *data, size_t data_length);

  /**
   * Truncates the existing log file
   *
   * @note 
   *
   * This is only called currently during debugging and testing of the 
   * command log...when the global command_log_truncate variable is 
   * set to anything other than false, this is called.
   */
  void truncate();

  /**
   * Takes a global transaction ID and a reference to a string to fill
   * with the name of the log file which contains the command with the 
   * transaction ID.  If the transaction ID is contained in a log file, 
   * the function returns true, false otherwise.
   *
   * @param[in] Global transaction ID to search on
   * @param[inout] String to fill with name of logfile containing command with
   *               the needed transaction ID
   *
   * @retval
   *  true if found
   * @retval
   *  false otherwise
   */
  bool findLogFilenameContainingTransactionId(const drizzled::ReplicationServices::GlobalTransactionId &to_find,
                                              std::string &out_filename) const;

  /**
   * Returns whether the log is currently in error.
   */
  bool hasError() const;

  /**
   * Returns the log's current error message
   */
  const std::string &getErrorMessage() const;
private:
  static const uint32_t HEADER_TRAILER_BYTES= sizeof(uint32_t) + /* 4-byte msg type header */
                                              sizeof(uint32_t) + /* 4-byte length header */
                                              sizeof(uint32_t); /* 4 byte checksum trailer */

  /* Don't allows these */
  TransactionLog();
  TransactionLog(const TransactionLog &other);
  TransactionLog &operator=(const TransactionLog &other);
  /**
   * Clears the current error message
   */
  void clearError();
  /**
   * Helper method which synchronizes/flushes the transaction log file
   * according to the transaction_log_flush_frequency system variable
   *
   * @retval
   *   0 == Success
   * @retval
   *   >0 == Failure. Error code.
   */
  int syncLogFile();

  int log_file; ///< Handle for our log file
  Status state; ///< The state the log is in
  const std::string log_file_path; ///< Full path to the log file
  std::string log_file_name; ///< Name of the log file
  drizzled::atomic<off_t> log_offset; ///< Offset in log file where log will write next command
  bool has_error; ///< Is the log in error?
  std::string error_message; ///< Current error message
  uint32_t flush_frequency; ///< Determines behaviour of syncing log file
  time_t last_sync_time; ///< Last time the log file was synced (only set in FLUSH_FREQUENCY_EVERY_SECOND)
  bool do_checksum; ///< Do a CRC32 checksum when writing Transaction message to log?
};

