/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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
 * Defines the API of the default transaction log.
 *
 * @see drizzled/plugin/replicator.h
 * @see drizzled/plugin/applier.h
 *
 * @details
 *
 * The TransactionLog applies events it receives from the ReplicationServices
 * server component to a simple log file on disk.
 * 
 * Transactions are received in no guaranteed order and the command log
 * is in charge of writing these events to the log as they are received.
 */

#ifndef PLUGIN_TRANSACTION_LOG_TRANSACTION_LOG_H
#define PLUGIN_TRANSACTION_LOG_TRANSACTION_LOG_H

#include <drizzled/atomics.h>
#include <drizzled/replication_services.h>
#include <drizzled/plugin/transaction_replicator.h>
#include <drizzled/plugin/transaction_applier.h>

#include <vector>
#include <string>

class TransactionLog: public drizzled::plugin::TransactionApplier 
{
public:
  static const uint32_t HEADER_TRAILER_BYTES= sizeof(uint32_t) + /* 4-byte msg type header */
                                              sizeof(uint32_t) + /* 4-byte length header */
                                              sizeof(uint32_t); /* 4 byte checksum trailer */

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
private:
  int log_file; ///< Handle for our log file
  enum Status state; ///< The state the log is in
  drizzled::atomic<bool> do_checksum; ///< Do a CRC32 checksum when writing Transaction message to log?
  const std::string log_file_path; ///< Full path to the log file
  std::string log_file_name; ///< Name of the log file
  drizzled::atomic<off_t> log_offset; ///< Offset in log file where log will write next command
public:
  TransactionLog(std::string name_arg,
                 const std::string &in_log_file_path,
                 bool in_do_checksum);

  /** Destructor */
  ~TransactionLog();

  /**
   * Applies a Transaction to the serial log
   *
   * @note
   *
   * It is important to note that memory allocation for the 
   * supplied pointer is not guaranteed after the completion 
   * of this function -- meaning the caller can dispose of the
   * supplied message.  Therefore, appliers which are
   * implementing an asynchronous replication system must copy
   * the supplied message to their own controlled memory storage
   * area.
   *
   * @param Transaction message to be replicated
   */
  void apply(const drizzled::message::Transaction &to_apply);

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
   * Returns the file descriptor of the transaction log
   */
  int getLogFileDescriptor();
  
  /**
   * Returns the state that the log is in
   */
  inline enum Status getState()
  {
    return state;
  }

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
};

#endif /* PLUGIN_TRANSACTION_LOG_TRANSACTION_LOG_H */
