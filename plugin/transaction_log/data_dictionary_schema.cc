/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  Authors:
 *
 *  Jay Pipes <joinfu@sun.com>
 *  Joseph Daly <skinny.moey@gmail.com>
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
 * Implements the DATA_DICTIONARY views which allows querying the
 * state of the transaction log and its entries.
 *
 * There are three views defined for the transaction log:
 *
 * CREATE TABLE DATA_DICTIONARY.TRANSACTION_LOG (
 *   FILE_NAME VARCHAR NOT NULL
 * , FILE_LENGTH BIGINT NOT NULL
 * , NUM_LOG_ENTRIES BIGINT NOT NULL
 * , NUM_TRANSACTIONS BIGINT NOT NULL
 * , MIN_TRANSACTION_ID BIGINT NOT NULL
 * , MAX_TRANSACTION_ID BIGINT NOT NULL
 * , MIN_END_TIMESTAMP BIGINT NOT NULL
 * , MAX_END_TIMESTAMP BIGINT NOT NULL
 * , INDEX_SIZE_IN_BYTES BIGINT NOT NULL
 * );
 *
 * CREATE TABLE DATA_DICTIONARY.TRANSACTION_LOG_ENTRIES (
 *   ENTRY_OFFSET BIGINT NOT NULL
 * , ENTRY_TYPE VARCHAR NOT NULL
 * , ENTRY_LENGTH BIGINT NOT NULL
 * );
 *
 * CREATE TABLE DATA_DICTIONARY.TRANSACTION_LOG_TRANSACTIONS (
 *   ENTRY_OFFSET BIGINT NOT NULL
 * , TRANSACTION_ID BIGINT NOT NULL
 * , SERVER_ID BIGINT NOT NULL
 * , START_TIMESTAMP BIGINT NOT NULL
 * , END_TIMESTAMP BIGINT NOT NULL
 * , NUM_STATEMENTS BIGINT NOT NULL
 * , CHECKSUM BIGINT NOT NULL 
 * );
 */

#include <config.h>

#include "data_dictionary_schema.h"
#include "transaction_log_index.h"

#include <fcntl.h>
#include <sys/stat.h>

using namespace std;
using namespace drizzled;

extern TransactionLog *transaction_log; /* the singleton transaction log */
extern TransactionLogIndex *transaction_log_index; /* the singleton transaction log index */

/*
 *
 * TRANSACTION_LOG_INFO
 *
 */

TransactionLogTool::TransactionLogTool() :
  plugin::TableFunction("DATA_DICTIONARY", "TRANSACTION_LOG")
{
  add_field("FILE_NAME");
  add_field("FILE_LENGTH", plugin::TableFunction::NUMBER);
  add_field("NUM_LOG_ENTRIES", plugin::TableFunction::NUMBER);
  add_field("NUM_TRANSACTIONS", plugin::TableFunction::NUMBER);
  add_field("MIN_TRANSACTION_ID", plugin::TableFunction::NUMBER);
  add_field("MAX_TRANSACTION_ID", plugin::TableFunction::NUMBER);
  add_field("MIN_END_TIMESTAMP", plugin::TableFunction::NUMBER);
  add_field("MAX_END_TIMESTAMP", plugin::TableFunction::NUMBER);
  add_field("INDEX_SIZE_IN_BYTES", plugin::TableFunction::NUMBER);
}

TransactionLogTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  is_done= false;
}

bool TransactionLogTool::Generator::populate()
{
  if (is_done)
  {
    return false;
  }

  const string &filename= transaction_log->getLogFilename();
  push(filename.c_str());
  
  /* Grab the file size of the log */
  struct stat file_stat;
  (void) stat(filename.c_str(), &file_stat);
  push(file_stat.st_size);

  push(transaction_log_index->getNumLogEntries());
  push(transaction_log_index->getNumTransactionEntries());
  push(transaction_log_index->getMinTransactionId());
  push(transaction_log_index->getMaxTransactionId());
  push(transaction_log_index->getMinEndTimestamp());
  push(transaction_log_index->getMaxEndTimestamp()); 
  push(static_cast<uint64_t>(transaction_log_index->getSizeInBytes()));

  is_done= true;
  return true;
}

/*
 *
 * TRANSACTION_LOG_ENTRIES view
 *
 */

TransactionLogEntriesTool::TransactionLogEntriesTool() :
  plugin::TableFunction("DATA_DICTIONARY", "TRANSACTION_LOG_ENTRIES")
{
  add_field("ENTRY_OFFSET", plugin::TableFunction::NUMBER);
  add_field("ENTRY_TYPE");
  add_field("ENTRY_LENGTH", plugin::TableFunction::NUMBER);
}

TransactionLogEntriesTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  it= transaction_log_index->getEntries().begin();
  end= transaction_log_index->getEntries().end(); 
}

bool TransactionLogEntriesTool::Generator::populate()
{
  if (it == end)
  { 
    return false;
  } 

  TransactionLogEntry &entry= *it;

  push(entry.getOffset());
  push(entry.getTypeAsString());
  push(static_cast<uint64_t>(entry.getLengthInBytes()));

  it++;

  return true;
}

/*
 *
 * TRANSACTION_LOG_TRANSACTIONS view
 *
 */

TransactionLogTransactionsTool::TransactionLogTransactionsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "TRANSACTION_LOG_TRANSACTIONS")
{
  add_field("ENTRY_OFFSET", plugin::TableFunction::NUMBER);
  add_field("TRANSACTION_ID", plugin::TableFunction::NUMBER);
  add_field("SERVER_ID", plugin::TableFunction::NUMBER);
  add_field("START_TIMESTAMP", plugin::TableFunction::NUMBER);
  add_field("END_TIMESTAMP", plugin::TableFunction::NUMBER);
  add_field("NUM_STATEMENTS", plugin::TableFunction::NUMBER);
  add_field("CHECKSUM",  plugin::TableFunction::NUMBER);
}

TransactionLogTransactionsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  it= transaction_log_index->getTransactionEntries().begin();
  end= transaction_log_index->getTransactionEntries().end();
}

bool TransactionLogTransactionsTool::Generator::populate()
{
  if (it == end)
  {
    return false;
  }

  TransactionLogTransactionEntry &entry= *it;

  push(entry.getOffset());
  push(entry.getTransactionId());
  push(static_cast<uint64_t>(entry.getServerId()));
  push(entry.getStartTimestamp());
  push(entry.getEndTimestamp());
  push(static_cast<uint64_t>(entry.getNumStatements()));
  push(static_cast<uint64_t>(entry.getChecksum()));

  it++;

  return true;
}
