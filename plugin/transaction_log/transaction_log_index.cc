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
 * Defines the implementation of a simple index into a transaction log.
 */

#include <config.h>
#include <drizzled/message/transaction.pb.h>

#include "transaction_log_index.h"

#include <pthread.h>

using namespace std;
using namespace drizzled;

TransactionLogIndex *transaction_log_index= NULL; /* The singleton transaction log index */

TransactionLogIndex::TransactionLogIndex(TransactionLog &in_log) :
  log(in_log),
  index_file(-1),
  index_file_path(),
  has_error(false),
  error_message(),
  min_end_timestamp(0),
  max_end_timestamp(0),
  min_transaction_id(0),
  max_transaction_id(0),
  entries(),
  transaction_entries()
{
  (void) pthread_mutex_init(&index_lock, NULL);
  entries.reserve(1024);
  transaction_entries.reserve(1024);
  open();
}

TransactionLogIndex::~TransactionLogIndex()
{
  entries.clear();
  transaction_entries.clear();
  pthread_mutex_destroy(&index_lock);
}

void TransactionLogIndex::clear()
{
  pthread_mutex_lock(&index_lock);
  min_end_timestamp= 0;
  max_end_timestamp= 0;
  min_transaction_id= 0;
  max_transaction_id= 0;
  entries.clear();
  transaction_entries.clear();
  clearError();
  pthread_mutex_unlock(&index_lock);
}

void TransactionLogIndex::open()
{

}

bool TransactionLogIndex::hasError() const
{
  return has_error;
}

void TransactionLogIndex::clearError()
{
  has_error= false;
  error_message.clear();
}

const std::string &TransactionLogIndex::getErrorMessage() const
{
  return error_message;
}

uint64_t TransactionLogIndex::getMinEndTimestamp() const
{
  return min_end_timestamp;
}

uint64_t TransactionLogIndex::getMaxEndTimestamp() const
{
  return max_end_timestamp;
}

uint64_t TransactionLogIndex::getMinTransactionId() const
{
  return min_transaction_id;
}

uint64_t TransactionLogIndex::getMaxTransactionId() const
{
  return max_transaction_id;
}

uint64_t TransactionLogIndex::getNumLogEntries() const
{
  return entries.size();
}

uint64_t TransactionLogIndex::getNumTransactionEntries() const
{
  return transaction_entries.size();
}

TransactionLog::Entries &TransactionLogIndex::getEntries()
{
  return entries;
}

TransactionLog::TransactionEntries &TransactionLogIndex::getTransactionEntries()
{
  return transaction_entries;
}

size_t TransactionLogIndex::getTransactionEntriesSizeInBytes()
{
  return transaction_entries.capacity() * sizeof(TransactionLog::TransactionEntries::value_type);
}

size_t TransactionLogIndex::getEntriesSizeInBytes()
{
  return entries.capacity() * sizeof(TransactionLog::Entries::value_type);
}


size_t TransactionLogIndex::getSizeInBytes()
{
  return sizeof(this) + getEntriesSizeInBytes() + getTransactionEntriesSizeInBytes();
}

void TransactionLogIndex::addEntry(const TransactionLogEntry &entry,
                                   const message::Transaction &transaction,
                                   uint32_t checksum)
{
  pthread_mutex_lock(&index_lock);
  if (entries.empty())
  {
    /* First entry...set the minimums */
    min_transaction_id= transaction.transaction_context().transaction_id();
    min_end_timestamp= transaction.transaction_context().end_timestamp();
  }
  max_transaction_id= transaction.transaction_context().transaction_id();
  max_end_timestamp= transaction.transaction_context().end_timestamp();
  entries.push_back(entry);
  transaction_entries.push_back(TransactionLogTransactionEntry(entry.getOffset(),
                                                               transaction,
                                                               checksum));
  pthread_mutex_unlock(&index_lock);
}
