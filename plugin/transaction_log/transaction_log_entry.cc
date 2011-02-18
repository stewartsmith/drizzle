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
 * Defines the implementation of a transaction log entry POD class
 */

#include <config.h>

#include "transaction_log_entry.h"

#include <drizzled/message/transaction.pb.h>

#include <string>
#include <map>

using namespace std;
using namespace drizzled;

static const char *entry_type_names[]= 
{
  "UNKNOWN",
  "TRANSACTION",
  "RAW BLOB"
};

TransactionLogEntry::TransactionLogEntry(enum ReplicationServices::MessageType in_type,
                                         off_t in_offset,
                                         size_t in_length) :
  type(in_type),
  offset(in_offset),
  length(in_length)
{}

TransactionLogEntry::~TransactionLogEntry()
{}

const char *TransactionLogEntry::getTypeAsString() const
{
  return entry_type_names[type];
}

off_t TransactionLogEntry::getOffset() const
{
  return offset;
}

size_t TransactionLogEntry::getLengthInBytes() const
{
  return length;
}

TransactionLogTransactionEntry::TransactionLogTransactionEntry(off_t in_offset,
                                                               const message::Transaction &transaction,
                                                               uint32_t in_checksum) :
  offset(in_offset),
  server_id(transaction.transaction_context().server_id()),
  transaction_id(transaction.transaction_context().transaction_id()),
  start_timestamp(transaction.transaction_context().start_timestamp()),
  end_timestamp(transaction.transaction_context().end_timestamp()),
  num_statements(transaction.statement_size()),
  checksum(in_checksum)
{
}

TransactionLogTransactionEntry::~TransactionLogTransactionEntry()
{}

off_t TransactionLogTransactionEntry::getOffset() const
{
  return offset;
}

uint64_t TransactionLogTransactionEntry::getTransactionId() const
{
  return transaction_id;
}

uint32_t TransactionLogTransactionEntry::getServerId() const
{
  return server_id;
}

uint64_t TransactionLogTransactionEntry::getStartTimestamp() const
{
  return start_timestamp;
}

uint64_t TransactionLogTransactionEntry::getEndTimestamp() const
{
  return end_timestamp;
}

uint64_t TransactionLogTransactionEntry::getNumStatements() const
{
  return num_statements;
}

uint32_t TransactionLogTransactionEntry::getChecksum() const
{
  return checksum;
}
