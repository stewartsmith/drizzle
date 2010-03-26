/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (c) 2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
 * @file Transaction processing code
 */

#ifndef DRIZZLED_TRANSACTION_SERVICES_H
#define DRIZZLED_TRANSACTION_SERVICES_H

#include "drizzled/message/transaction.pb.h"

namespace drizzled
{

/* some forward declarations needed */
namespace plugin
{
  class MonitoredInTransaction;
  class XaResourceManager;
  class TransactionalStorageEngine;
}

class Session;
class NamedSavepoint;

/**
 * This is a class which manages the XA transaction processing
 * in the server
 */
class TransactionServices
{
public:
  static const size_t DEFAULT_RECORD_SIZE= 100;
  /**
   * Constructor
   */
  TransactionServices() {}

  /**
   * Singleton method
   * Returns the singleton instance of TransactionServices
   */
  static inline TransactionServices &singleton()
  {
    static TransactionServices transaction_services;
    return transaction_services;
  }
  /**
   * Method which returns the active Transaction message
   * for the supplied Session.  If one is not found, a new Transaction
   * message is allocated, initialized, and returned.
   *
   * @param The session processing the transaction
   */
  message::Transaction *getActiveTransactionMessage(Session *in_session);
  /** 
   * Method which attaches a transaction context
   * the supplied transaction based on the supplied Session's
   * transaction information.  This method also ensure the
   * transaction message is attached properly to the Session object
   *
   * @param The transaction message to initialize
   * @param The Session processing this transaction
   */
  void initTransactionMessage(message::Transaction &in_transaction, Session *in_session);
  /** 
   * Helper method which finalizes data members for the 
   * supplied transaction's context.
   *
   * @param The transaction message to finalize 
   * @param The Session processing this transaction
   */
  void finalizeTransactionMessage(message::Transaction &in_transaction, Session *in_session);
  /**
   * Helper method which deletes transaction memory and
   * unsets Session's transaction and statement messages.
   */
  void cleanupTransactionMessage(message::Transaction *in_transaction,
                                 Session *in_session);
  /**
   * Helper method which initializes a Statement message
   *
   * @param The statement to initialize
   * @param The type of the statement
   * @param The session processing this statement
   */
  void initStatementMessage(message::Statement &statement,
                            message::Statement::Type in_type,
                            Session *in_session);
  /**
   * Finalizes a Statement message and sets the Session's statement
   * message to NULL.
   *
   * @param The statement to initialize
   * @param The session processing this statement
   */
  void finalizeStatementMessage(message::Statement &statement,
                                Session *in_session);
  /** Helper method which returns an initialized Statement message for methods
   * doing insertion of data.
   *
   * @param[in] Pointer to the Session doing the processing
   * @param[in] Pointer to the Table object being inserted into
   */
  message::Statement &getInsertStatement(Session *in_session,
                                         Table *in_table);

  /**
   * Helper method which initializes the header message for
   * insert operations.
   *
   * @param[inout] Statement message container to modify
   * @param[in] Pointer to the Session doing the processing
   * @param[in] Pointer to the Table being inserted into
   */
  void setInsertHeader(message::Statement &statement,
                       Session *in_session,
                       Table *in_table);
  /**
   * Helper method which returns an initialized Statement
   * message for methods doing updates of data.
   *
   * @param[in] Pointer to the Session doing the processing
   * @param[in] Pointer to the Table object being updated
   * @param[in] Pointer to the old data in the record
   * @param[in] Pointer to the new data in the record
   */
  message::Statement &getUpdateStatement(Session *in_session,
                                         Table *in_table,
                                         const unsigned char *old_record, 
                                         const unsigned char *new_record);
  /**
   * Helper method which initializes the header message for
   * update operations.
   *
   * @param[inout] Statement message container to modify
   * @param[in] Pointer to the Session doing the processing
   * @param[in] Pointer to the Table being updated
   * @param[in] Pointer to the old data in the record
   * @param[in] Pointer to the new data in the record
   */
  void setUpdateHeader(message::Statement &statement,
                       Session *in_session,
                       Table *in_table,
                       const unsigned char *old_record, 
                       const unsigned char *new_record);
  /**
   * Helper method which returns an initialized Statement
   * message for methods doing deletion of data.
   *
   * @param[in] Pointer to the Session doing the processing
   * @param[in] Pointer to the Table object being deleted from
   */
  message::Statement &getDeleteStatement(Session *in_session,
                                         Table *in_table);

  /**
   * Helper method which initializes the header message for
   * insert operations.
   *
   * @param[inout] Statement message container to modify
   * @param[in] Pointer to the Session doing the processing
   * @param[in] Pointer to the Table being deleted from
   */
  void setDeleteHeader(message::Statement &statement,
                       Session *in_session,
                       Table *in_table);
  /** 
   * Commits a normal transaction (see above) and pushes the transaction
   * message out to the replicators.
   *
   * @param Pointer to the Session committing the transaction
   */
  void commitTransactionMessage(Session *in_session);
  /** 
   * Marks the current active transaction message as being rolled back and
   * pushes the transaction message out to replicators.
   *
   * @param Pointer to the Session committing the transaction
   */
  void rollbackTransactionMessage(Session *in_session);
  /**
   * Creates a new InsertRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has inserted a record
   * @param Pointer to the Table containing insert information
   *
   * Grr, returning "true" here on error because of the cursor
   * reversed bool return crap...fix that.
   */
  bool insertRecord(Session *in_session, Table *in_table);
  /**
   * Creates a new UpdateRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has updated a record
   * @param Pointer to the Table containing update information
   * @param Pointer to the raw bytes representing the old record/row
   * @param Pointer to the raw bytes representing the new record/row 
   */
  void updateRecord(Session *in_session, 
                    Table *in_table, 
                    const unsigned char *old_record, 
                    const unsigned char *new_record);
  /**
   * Creates a new DeleteRecord GPB message and pushes it to
   * replicators.
   *
   * @param Pointer to the Session which has deleted a record
   * @param Pointer to the Table containing delete information
   */
  void deleteRecord(Session *in_session, Table *in_table);
  /**
   * Creates a CreateSchema Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] Pointer to the Session which issued the statement
   * @param[in] message::Schema message describing new schema
   */
  void createSchema(Session *in_session, const message::Schema &schema);
  /**
   * Creates a DropSchema Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] Pointer to the Session which issued the statement
   * @param[in] message::Schema message describing new schema
   */
  void dropSchema(Session *in_session, const std::string &schema_name);
  /**
   * Creates a CreateTable Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] Pointer to the Session which issued the statement
   * @param[in] message::Table message describing new schema
   */
  void createTable(Session *in_session, const message::Table &table);
  /**
   * Creates a DropTable Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] Pointer to the Session which issued the statement
   * @param[in] The schema of the table being dropped
   * @param[in] The table name of the table being dropped
   * @param[in] Did the user specify an IF EXISTS clause?
   */
  void dropTable(Session *in_session,
                     const std::string &schema_name,
                     const std::string &table_name,
                     bool if_exists);
  /**
   * Creates a TruncateTable Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] Pointer to the Session which issued the statement
   * @param[in] The Table being truncated
   */
  void truncateTable(Session *in_session, Table *in_table);
  /**
   * Creates a new RawSql GPB message and pushes it to 
   * replicators.
   *
   * @TODO With a real data dictionary, this really shouldn't
   * be needed.  CREATE TABLE would map to insertRecord call
   * on the I_S, etc.  Not sure what to do with administrative
   * commands like CHECK TABLE, though..
   *
   * @param Pointer to the Session which issued the statement
   * @param Query string
   */
  void rawStatement(Session *in_session, const std::string &query);
  /* transactions: interface to plugin::StorageEngine functions */
  int commitPhaseOne(Session *session, bool all);
  int rollbackTransaction(Session *session, bool all);

  /* transactions: these functions never call plugin::StorageEngine functions directly */
  int commitTransaction(Session *session, bool all);
  int autocommitOrRollback(Session *session, int error);

  /* savepoints */
  int rollbackToSavepoint(Session *session, NamedSavepoint &sv);
  int setSavepoint(Session *session, NamedSavepoint &sv);
  int releaseSavepoint(Session *session, NamedSavepoint &sv);

  /**
   * Marks a storage engine as participating in a statement
   * transaction.
   *
   * @note
   * 
   * This method is idempotent
   *
   * @todo
   *
   * This method should not be called more than once per resource
   * per statement, and therefore should not need to be idempotent.
   * Put in assert()s to test this.
   *
   * @param[in] Session pointer
   * @param[in] Descriptor for the resource which will be participating
   * @param[in] Pointer to the TransactionalStorageEngine resource
   */
  void registerResourceForStatement(Session *session,
                                    plugin::MonitoredInTransaction *monitored,
                                    plugin::TransactionalStorageEngine *engine);

  /**
   * Marks an XA storage engine as participating in a statement
   * transaction.
   *
   * @note
   * 
   * This method is idempotent
   *
   * @todo
   *
   * This method should not be called more than once per resource
   * per statement, and therefore should not need to be idempotent.
   * Put in assert()s to test this.
   *
   * @param[in] Session pointer
   * @param[in] Descriptor for the resource which will be participating
   * @param[in] Pointer to the TransactionalStorageEngine resource
   * @param[in] Pointer to the XaResourceManager resource manager
   */
  void registerResourceForStatement(Session *session,
                                    plugin::MonitoredInTransaction *monitored,
                                    plugin::TransactionalStorageEngine *engine,
                                    plugin::XaResourceManager *resource_manager);

  /**
   * Registers a resource manager in the "normal" transaction.
   *
   * @note
   *
   * This method is idempotent and must be idempotent
   * because it can be called both by the above 
   * TransactionServices::registerResourceForStatement(),
   * which occurs at the beginning of each SQL statement,
   * and also manually when a BEGIN WORK/START TRANSACTION
   * statement is executed. If the latter case (BEGIN WORK)
   * is called, then subsequent contained statement transactions
   * will call this method as well.
   *
   * @note
   *
   * This method checks to see if the supplied resource
   * is also registered in the statement transaction, and
   * if not, registers the resource in the statement
   * transaction.  This happens ONLY when the user has
   * called BEGIN WORK/START TRANSACTION, which is the only
   * time when this method is called except from the
   * TransactionServices::registerResourceForStatement method.
   */
  void registerResourceForTransaction(Session *session,
                                      plugin::MonitoredInTransaction *monitored,
                                      plugin::TransactionalStorageEngine *engine);
  void registerResourceForTransaction(Session *session,
                                      plugin::MonitoredInTransaction *monitored,
                                      plugin::TransactionalStorageEngine *engine,
                                      plugin::XaResourceManager *resource_manager);
};

} /* namespace drizzled */

#endif /* DRIZZLED_TRANSACTION_SERVICES_H */
