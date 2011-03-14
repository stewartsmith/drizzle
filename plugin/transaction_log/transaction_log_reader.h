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
 * Defines the API of a simple reader of Transaction messages from the
 * Transaction log file.  
 *
 * @details
 *
 * This class is used by other plugins, for instance
 * the async_replication module, in order to read the transaction log and
 * return Transaction messages.
 */

#pragma once

#include <drizzled/plugin/transaction_reader.h>

class TransactionLog;

/**
 * A class which reads Transaction messages from the Transaction log file
 */
class TransactionLogReader :public drizzled::plugin::TransactionReader
{
private:
  /** The Transaction log object this reader uses */
  const TransactionLog &log;
public:
  TransactionLogReader(std::string name_arg, const TransactionLog &in_log)
    : drizzled::plugin::TransactionReader(name_arg), log(in_log)
  {}

  /** Destructor */
  ~TransactionLogReader() {}
  /**
   * Read and fill a Transaction message with the supplied
   * Transaction message global transaction ID.
   *
   * @param[in] Global transaction ID to find
   * @param[out] Pointer to a transaction message to fill
   *
   * @retval
   *  true if Transaction message was read successfully and the supplied pointer to message was filled
   * @retval
   *  false if not found or read successfully
   */
  bool read(const drizzled::ReplicationServices::GlobalTransactionId &to_read_trx_id, 
            drizzled::message::Transaction *to_fill);
};

