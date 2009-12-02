/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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
 * Implements the INFORMATION_SCHEMA views which allows querying the
 * state of the transaction log and its entries.
 *
 * There are three views defined for the transaction log:
 *
 * CREATE TABLE INFORMATION_SCHEMA.TRANSACTION_LOG (
 *   FILE_NAME VARCHAR NOT NULL
 * , FILE_LENGTH BIGINT NOT NULL
 * , NUM_LOG_ENTRIES BIGINT NOT NULL
 * , NUM_TRANSACTIONS BIGINT NOT NULL
 * , MIN_TRANSACTION_ID BIGINT NOT NULL
 * , MAX_TRANSACTION_ID BIGINT NOT NULL
 * , MIN_END_TIMESTAMP BIGINT NOT NULL
 * , MAX_END_TIMESTAMP BIGINT NOT NULL
 * );
 * 
 * CREATE TABLE INFORMATION_SCHEMA.TRANSACTION_LOG_ENTRIES (
 *   ENTRY_OFFSET BIGINT NOT NULL
 * , ENTRY_TYPE VARCHAR NOT NULL
 * , ENTRY_LENGTH BIGINT NOT NULL
 * );
 * 
 * CREATE TABLE INFORMATION_SCHEMA.TRANSACTION_LOG_TRANSACTIONS (
 *   ENTRY_OFFSET BIGINT NOT NULL
 * , TRANSACTION_ID BIGINT NOT NULL
 * , SERVER_ID INT NOT NULL
 * , START_TIMESTAMP BIGINT NOT NULL
 * , END_TIMESTAMP BIGINT NOT NULL
 * , NUM_STATEMENTS INT NOT NULL
 * , CHECKSUM BIGINT NOT NULL // had to be BIGINT b/c we have no unsigned int.
 * );
 */

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/show.h>

#include "transaction_log.h"
#include "transaction_log_entry.h"
#include "transaction_log_index.h"
#include "info_schema.h"

#include <string>
#include <vector>
#include <functional>

using namespace std;
using namespace drizzled;

extern TransactionLog *transaction_log; /* the singleton transaction log */
extern TransactionLogIndex *transaction_log_index; /* the singleton transaction log index */

class DeletePtr
{
public:
  template<typename T>
  inline void operator()(const T *ptr) const
  {
    delete ptr;
  }
};

/*
 * Vectors of columns for I_S tables.
 */
static vector<const plugin::ColumnInfo *> transaction_log_view_columns;
static vector<const plugin::ColumnInfo *> transaction_log_entries_view_columns;
static vector<const plugin::ColumnInfo *> transaction_log_transactions_view_columns;

/*
 * Methods for I_S tables.
 */
static plugin::InfoSchemaMethods *transaction_log_view_methods= NULL;
static plugin::InfoSchemaMethods *transaction_log_entries_view_methods= NULL;
static plugin::InfoSchemaMethods *transaction_log_transactions_view_methods= NULL;

/*
 * I_S tables.
 */
plugin::InfoSchemaTable *transaction_log_view= NULL;
plugin::InfoSchemaTable *transaction_log_entries_view= NULL;
plugin::InfoSchemaTable *transaction_log_transactions_view= NULL;

static bool createTransactionLogViewColumns(vector<const drizzled::plugin::ColumnInfo *> &cols);
static bool createTransactionLogEntriesViewColumns(vector<const drizzled::plugin::ColumnInfo *> &cols);
static bool createTransactionLogTransactionsViewColumns(vector<const drizzled::plugin::ColumnInfo *> &cols);

int TransactionLogViewISMethods::fillTable(Session *,
                                           Table *table,
                                           plugin::InfoSchemaTable *schema_table)
{
  if (transaction_log != NULL)
  {
    table->restoreRecordAsDefault();

    table->setWriteSet(0);
    table->setWriteSet(1);
    table->setWriteSet(2);
    table->setWriteSet(3);
    table->setWriteSet(4);
    table->setWriteSet(5);
    table->setWriteSet(6);
    table->setWriteSet(7);

    const string &filename= transaction_log->getLogFilename();
    table->field[0]->store(filename.c_str(), filename.length(), system_charset_info);

    /* Grab the file size of the log */
    struct stat file_stat;
    (void) stat(filename.c_str(), &file_stat);

    table->field[1]->store(static_cast<int64_t>(file_stat.st_size));

    table->field[2]->store(static_cast<int64_t>(transaction_log_index->getNumLogEntries()));
    table->field[3]->store(static_cast<int64_t>(transaction_log_index->getNumTransactionEntries()));
    table->field[4]->store(static_cast<int64_t>(transaction_log_index->getMinTransactionId()));
    table->field[5]->store(static_cast<int64_t>(transaction_log_index->getMaxTransactionId()));
    table->field[6]->store(static_cast<int64_t>(transaction_log_index->getMinEndTimestamp()));
    table->field[7]->store(static_cast<int64_t>(transaction_log_index->getMaxEndTimestamp()));

    /* store the actual record now */
    schema_table->addRow(table->record[0], table->s->reclength);
  }
  return 0;
}

int TransactionLogEntriesViewISMethods::fillTable(Session *,
                                                  Table *table,
                                                  plugin::InfoSchemaTable *schema_table)
{
  if (transaction_log != NULL)
  {
    /** @todo trigger indexing update here. */
    for (TransactionLog::Entries::iterator entry_iter= transaction_log_index->getEntries().begin();
         entry_iter != transaction_log_index->getEntries().end();
         ++entry_iter)
    {
      TransactionLogEntry &entry= *entry_iter;

      table->restoreRecordAsDefault();

      table->setWriteSet(0);
      table->setWriteSet(1);
      table->setWriteSet(2);

      table->field[0]->store(static_cast<int64_t>(entry.getOffset()));
      const char *type_string= entry.getTypeAsString();
      table->field[1]->store(type_string, strlen(type_string), system_charset_info);
      table->field[2]->store(static_cast<int64_t>(entry.getLengthInBytes()));

      /* store the actual record now */
      schema_table->addRow(table->record[0], table->s->reclength);
    }
  }
  return 0;
}

int TransactionLogTransactionsViewISMethods::fillTable(Session *,
                                                       Table *table,
                                                       plugin::InfoSchemaTable *schema_table)
{
  if (transaction_log != NULL)
  {
    /** @todo trigger indexing update here. */
    for (TransactionLog::TransactionEntries::iterator entry_iter= transaction_log_index->getTransactionEntries().begin();
         entry_iter != transaction_log_index->getTransactionEntries().end();
         ++entry_iter)
    {
      TransactionLogTransactionEntry &entry= *entry_iter;

      table->restoreRecordAsDefault();

      table->setWriteSet(0);
      table->setWriteSet(1);
      table->setWriteSet(2);
      table->setWriteSet(3);
      table->setWriteSet(4);
      table->setWriteSet(5);
      table->setWriteSet(6);

      table->field[0]->store(static_cast<int64_t>(entry.getOffset()));
      table->field[1]->store(static_cast<int64_t>(entry.getTransactionId()));
      table->field[2]->store(static_cast<int64_t>(entry.getServerId()));
      table->field[3]->store(static_cast<int64_t>(entry.getStartTimestamp()));
      table->field[4]->store(static_cast<int64_t>(entry.getEndTimestamp()));
      table->field[5]->store(static_cast<int64_t>(entry.getNumStatements()));
      table->field[6]->store(static_cast<int64_t>(entry.getChecksum()));

      /* store the actual record now */
      schema_table->addRow(table->record[0], table->s->reclength);
    }
  }
  return 0;
}

static bool createTransactionLogViewColumns(vector<const plugin::ColumnInfo *> &cols)
{
  /*
   * Create each column for the INFORMATION_SCHEMA.TRANSACTION_LOG view
   */
  const plugin::ColumnInfo *file_name_col= 
    new (nothrow) plugin::ColumnInfo("FILE_NAME",
                                      255,
                                      DRIZZLE_TYPE_VARCHAR,
                                      0,
                                      0, 
                                      "Filename of the transaction log");
  if (file_name_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "FILE_NAME", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *file_length_col= 
    new (nothrow) plugin::ColumnInfo("FILE_LENGTH",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Length in bytes of the transaction log");
  if (file_length_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "FILE_LENGTH", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *num_log_entries_col= 
    new (nothrow) plugin::ColumnInfo("NUM_LOG_ENTRIES",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Total number of entries in the transaction log");
  if (num_log_entries_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "NUM_LOG_ENTRIES", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *num_transactions_col= 
    new (nothrow) plugin::ColumnInfo("NUM_TRANSACTIONS",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Total number of transaction message entries in the transaction log");
  if (num_transactions_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "NUM_TRANSACTIONS", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *min_transaction_id_col= 
    new (nothrow) plugin::ColumnInfo("MIN_TRANSACTION_ID",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Minimum transaction ID in the transaction log");
  if (min_transaction_id_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "MIN_TRANSACTION_ID", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *max_transaction_id_col= 
    new (nothrow) plugin::ColumnInfo("MAX_TRANSACTION_ID",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Maximum transaction ID in the transaction log");
  if (max_transaction_id_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "MAX_TRANSACTION_ID", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *min_timestamp_col= 
    new (nothrow) plugin::ColumnInfo("MIN_END_TIMESTAMP",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Minimum end timestamp for a transaction in the transaction log");
  if (min_timestamp_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "MIN_END_TIMESTAMP", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *max_timestamp_col= 
    new (nothrow) plugin::ColumnInfo("MAX_END_TIMESTAMP",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Maximum end timestamp for a transaction in the transaction log");
  if (max_timestamp_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "MAX_END_TIMESTAMP", 
                  strerror(errno));
    return true;
  }

  cols.push_back(file_name_col);
  cols.push_back(file_length_col);
  cols.push_back(num_log_entries_col);
  cols.push_back(num_transactions_col);
  cols.push_back(min_transaction_id_col);
  cols.push_back(max_transaction_id_col);
  cols.push_back(min_timestamp_col);
  cols.push_back(max_timestamp_col);

  return false;
}

static bool createTransactionLogEntriesViewColumns(vector<const plugin::ColumnInfo *> &cols)
{
  /*
   * Create each column for the INFORMATION_SCHEMA.TRANSACTION_LOG_ENTRIES view
   */
  const plugin::ColumnInfo *entry_offset_col= 
    new (nothrow) plugin::ColumnInfo("ENTRY_OFFSET",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Offset of this entry into the transaction log");
  if (entry_offset_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "ENTRY_OFFSET", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *entry_type_col= 
    new (nothrow) plugin::ColumnInfo("ENTRY_TYPE",
                                      32,
                                      DRIZZLE_TYPE_VARCHAR,
                                      0,
                                      0, 
                                      "Type of this entry");
  if (entry_type_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "ENTRY_TYPE", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *entry_length_col= 
    new (nothrow) plugin::ColumnInfo("ENTRY_LENGTH",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Length in bytes of this entry");
  if (entry_length_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "ENTRY_LENGTH", 
                  strerror(errno));
    return true;
  }

  cols.push_back(entry_offset_col);
  cols.push_back(entry_type_col);
  cols.push_back(entry_length_col);

  return false;
}

static bool createTransactionLogTransactionsViewColumns(vector<const plugin::ColumnInfo *> &cols)
{
  /*
   * Create each column for the INFORMATION_SCHEMA.TRANSACTION_LOG_TRANSACTIONS view
   */
  const plugin::ColumnInfo *entry_offset_col= 
    new (nothrow) plugin::ColumnInfo("ENTRY_OFFSET",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Offset of this entry into the transaction log");
  if (entry_offset_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "ENTRY_OFFSET", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *transaction_id_col= 
    new (nothrow) plugin::ColumnInfo("TRANSACTION_ID",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Transaction ID for this entry");
  if (transaction_id_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "TRANSACTION_ID", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *server_id_col= 
    new (nothrow) plugin::ColumnInfo("SERVER_ID",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Server ID for this entry");
  if (server_id_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "SERVER_ID", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *start_timestamp_col= 
    new (nothrow) plugin::ColumnInfo("START_TIMESTAMP",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Start timestamp for this transaction");
  if (start_timestamp_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "START_TIMESTAMP", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *end_timestamp_col= 
    new (nothrow) plugin::ColumnInfo("END_TIMESTAMP",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "End timestamp for this transaction");
  if (end_timestamp_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "END_TIMESTAMP", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *num_statements_col= 
    new (nothrow) plugin::ColumnInfo("NUM_STATEMENTS",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Number of statements in this transaction");
  if (num_statements_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "NUM_STATEMENTS", 
                  strerror(errno));
    return true;
  }

  const plugin::ColumnInfo *checksum_col= 
    new (nothrow) plugin::ColumnInfo("CHECKSUM",
                                      8,
                                      DRIZZLE_TYPE_LONGLONG,
                                      0,
                                      0, 
                                      "Checksum of this transaction");
  if (checksum_col == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate ColumnInfo %s.  Got error: %s\n"), 
                  "CHECKSUM", 
                  strerror(errno));
    return true;
  }

  cols.push_back(entry_offset_col);
  cols.push_back(transaction_id_col);
  cols.push_back(server_id_col);
  cols.push_back(start_timestamp_col);
  cols.push_back(end_timestamp_col);
  cols.push_back(num_statements_col);
  cols.push_back(checksum_col);

  return false;
}

bool initViewColumns()
{
  if (createTransactionLogViewColumns(transaction_log_view_columns))
  {
    return true;
  }

  if (createTransactionLogEntriesViewColumns(transaction_log_entries_view_columns))
  {
    return true;
  }

  if (createTransactionLogTransactionsViewColumns(transaction_log_transactions_view_columns))
  {
    return true;
  }

  return false;
}

static void clearViewColumns(vector<const plugin::ColumnInfo *> &cols)
{
  for_each(cols.begin(), cols.end(), DeletePtr());
  cols.clear();
}

void cleanupViewColumns()
{
  clearViewColumns(transaction_log_view_columns);
  clearViewColumns(transaction_log_entries_view_columns);
  clearViewColumns(transaction_log_transactions_view_columns);
}

bool initViewMethods()
{
  transaction_log_view_methods= new (nothrow) TransactionLogViewISMethods();
  if (transaction_log_view_methods == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate TransactionLogViewISMethods.  Got error: %s\n"), 
                  strerror(errno));
    return true;
  }

  transaction_log_entries_view_methods= new (nothrow) TransactionLogEntriesViewISMethods();
  if (transaction_log_entries_view_methods == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate TransactionLogEntriesViewISMethods.  Got error: %s\n"), 
                  strerror(errno));
    return true;
  }

  transaction_log_transactions_view_methods= new (nothrow) TransactionLogTransactionsViewISMethods();
  if (transaction_log_transactions_view_methods == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate TransactionLogTransactionsViewISMethods.  Got error: %s\n"), 
                  strerror(errno));
    return true;
  }

  return false;
}

void cleanupViewMethods()
{
  delete transaction_log_view_methods;
  delete transaction_log_entries_view_methods;
  delete transaction_log_transactions_view_methods;
}

bool initViews()
{
  transaction_log_view= 
    new (nothrow) plugin::InfoSchemaTable("TRANSACTION_LOG",
                                          transaction_log_view_columns,
                                          -1, -1, false, false, 0,
                                          transaction_log_view_methods);
  if (transaction_log_view == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate InfoSchemaTable for %s.  Got error: %s\n"), 
                  "TRANSACTION_LOG",
                  strerror(errno));
    return true;
  }

  transaction_log_entries_view= 
    new (nothrow) plugin::InfoSchemaTable("TRANSACTION_LOG_ENTRIES",
                                          transaction_log_entries_view_columns,
                                          -1, -1, false, false, 0,
                                          transaction_log_entries_view_methods);
  if (transaction_log_entries_view == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate InfoSchemaTable for %s.  Got error: %s\n"), 
                  "TRANSACTION_LOG_ENTRIES",
                  strerror(errno));
    return true;
  }

  transaction_log_transactions_view= 
    new (nothrow) plugin::InfoSchemaTable("TRANSACTION_LOG_TRANSACTIONS",
                                          transaction_log_transactions_view_columns,
                                          -1, -1, false, false, 0,
                                          transaction_log_transactions_view_methods);
  if (transaction_log_transactions_view == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate InfoSchemaTable for %s.  Got error: %s\n"), 
                  "TRANSACTION_LOG_TRANSACTIONS",
                  strerror(errno));
    return true;
  }

  return false;
}

void cleanupViews()
{
  delete transaction_log_view;
  delete transaction_log_entries_view;
  delete transaction_log_transactions_view;
}
