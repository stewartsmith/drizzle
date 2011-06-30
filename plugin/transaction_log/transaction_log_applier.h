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
 * Defines the API of the transaction log applier.
 *
 * @see drizzled/plugin/replicator.h
 * @see drizzled/plugin/applier.h
 */

#pragma once

#include <drizzled/replication_services.h>
#include <drizzled/plugin/transaction_applier.h>

#include "transaction_log_entry.h"

#include <vector>
#include <string>

namespace drizzled
{
  class Session;
}

class TransactionLog;
class TransactionLogIndex;
class WriteBuffer;

class TransactionLogApplier: public drizzled::plugin::TransactionApplier 
{
public:
  TransactionLogApplier(const std::string &name_arg,
                        TransactionLog *in_transaction_log,
                        TransactionLogIndex *in_transaction_log_index,
                        uint32_t in_num_write_buffers);

  /** Destructor */
  ~TransactionLogApplier();

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
   * @param Session descriptor
   * @param Transaction message to be replicated
   */
  drizzled::plugin::ReplicationReturnCode
  apply(drizzled::Session &in_session,
        const drizzled::message::Transaction &to_apply);
private:
  /* Don't allows these */
  TransactionLogApplier();
  TransactionLogApplier(const TransactionLogApplier &other);
  TransactionLogApplier &operator=(const TransactionLogApplier &other);
  /** 
   * This Applier owns the memory of the associated TransactionLog 
   * and its index - so we have to track it. 
   */
  TransactionLog *transaction_log;
  TransactionLogIndex *transaction_log_index;
  uint32_t num_write_buffers; ///< Number of write buffers used
  std::vector<WriteBuffer *> write_buffers; ///< array of write buffers

  /**
   * Returns the write buffer for the supplied session
   *
   * @param Session descriptor
   */
  WriteBuffer *getWriteBuffer(const drizzled::Session &session);
};

