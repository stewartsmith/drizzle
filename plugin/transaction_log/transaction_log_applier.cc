/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  Authors:
 *
 *    Jay Pipes <jaypipes@gmail.com>
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
 * Defines the implementation of the transaction applier plugin
 * for the transaction log.
 *
 * @see drizzled/plugin/transaction_replicator.h
 * @see drizzled/plugin/transaction_applier.h
 *
 * @details
 *
 * The TransactionLogApplier::apply() method constructs the entry
 * in the transaction log from the supplied Transaction message and
 * asks its associated TransactionLog object to write this entry.
 *
 * Upon a successful write, the applier adds some information about
 * the written transaction to the transaction log index.
 */

#include <config.h>
#include "write_buffer.h"
#include "transaction_log.h"
#include "transaction_log_applier.h"
#include "transaction_log_index.h"

#include <vector>

#include <drizzled/message/transaction.pb.h>
#include <drizzled/util/functors.h>
#include <drizzled/session.h>

using namespace std;
using namespace drizzled;

TransactionLogApplier *transaction_log_applier= NULL; /* The singleton transaction log applier */

TransactionLogApplier::TransactionLogApplier(const string &name_arg,
                                             TransactionLog *in_transaction_log,
                                             TransactionLogIndex *in_transaction_log_index,
                                             uint32_t in_num_write_buffers) :
  plugin::TransactionApplier(name_arg),
  transaction_log(in_transaction_log),
  transaction_log_index(in_transaction_log_index),
  num_write_buffers(in_num_write_buffers),
  write_buffers()
{
  /* 
   * Create each of the buffers we need for undo log entries 
   */
  write_buffers.reserve(num_write_buffers);
  for (size_t x= 0; x < num_write_buffers; ++x)
  {
    write_buffers.push_back(new WriteBuffer());
  }
}

TransactionLogApplier::~TransactionLogApplier()
{
  for_each(write_buffers.begin(),
           write_buffers.end(),
           DeletePtr());
  write_buffers.clear();
  delete transaction_log;
  delete transaction_log_index;
}

WriteBuffer *TransactionLogApplier::getWriteBuffer(const Session &session)
{
  return write_buffers[session.getSessionId() % num_write_buffers];
}

plugin::ReplicationReturnCode
TransactionLogApplier::apply(Session &in_session,
                             const message::Transaction &to_apply)
{
  size_t entry_size= TransactionLog::getLogEntrySize(to_apply);
  WriteBuffer *write_buffer= getWriteBuffer(in_session);

  uint32_t checksum;

  write_buffer->lock();
  write_buffer->resize(entry_size);
  uint8_t *bytes= write_buffer->getRawBytes();
  bytes= transaction_log->packTransactionIntoLogEntry(to_apply,
                                                     bytes,
                                                     &checksum);

  off_t written_to= transaction_log->writeEntry(bytes, entry_size);
  write_buffer->unlock();

  /* Add an entry to the index describing what was just applied */
  transaction_log_index->addEntry(TransactionLogEntry(ReplicationServices::TRANSACTION,
                                                      written_to,
                                                      entry_size),
                                  to_apply,
                                  checksum);
  return plugin::SUCCESS;
}
