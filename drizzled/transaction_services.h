/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
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

#pragma once

#include <drizzled/atomics.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/identifier.h>
#include <drizzled/message/schema.h>
#include <drizzled/message/table.h>
#include <drizzled/session.h>

#include <drizzled/visibility.h>

namespace drizzled {

/**
 * This is a class which manages the XA transaction processing
 * in the server
 */
class DRIZZLED_API TransactionServices
{
public:
  static const size_t DEFAULT_RECORD_SIZE= 100;
  
  /**
   * Returns true if the transaction manager should construct
   * Transaction and Statement messages, false otherwise.
   */
  static bool shouldConstructMessages();

  /**
   * Finalizes a Statement message and sets the Session's statement
   * message to NULL.
   *
   * @param statement The statement to initialize
   * @param session The Session object processing this statement
   */
  static void finalizeStatementMessage(message::Statement&, Session&);

  /**
   * Creates a new InsertRecord GPB message and pushes it to
   * replicators.
   *
   * @param session Session object which has inserted a record
   * @param table Table object containing insert information
   *
   * Grr, returning "true" here on error because of the cursor
   * reversed bool return crap...fix that.
   */
  static bool insertRecord(Session&, Table &in_table);

  /**
   * Creates a new UpdateRecord GPB message and pushes it to
   * replicators.
   *
   * @param session Session object which has updated a record
   * @param table Table object containing update information
   * @param old_record Pointer to the raw bytes representing the old record/row
   * @param new_record Pointer to the raw bytes representing the new record/row 
   */
  static void updateRecord(Session&, 
                    Table&, 
                    const unsigned char *old_record, 
                    const unsigned char *new_record);

  /**
   * Creates a new DeleteRecord GPB message and pushes it to
   * replicators.
   *
   * @param session Session object which has deleted a record
   * @param table Table object containing delete information
   * @param use_update_record If true, uses the values from the update row instead
   */
  static void deleteRecord(Session&, Table&, bool use_update_record= false);

  /**
   * Creates a CreateSchema Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] session Session object which issued the statement
   * @param[in] schema message::Schema message describing new schema
   */
  static void createSchema(Session&, const message::Schema &schema);

  /**
   * Creates a DropSchema Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] session Session object which issued the statement
   * @param[in] identifier Identifier for the schema to drop
   */
  static void dropSchema(Session&,
                  const identifier::Schema& identifier,
                  message::schema::const_reference schema);

  /**
   * Creates an AlterSchema Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] session Session object which issued the statement
   * @param[in] old_schema Original schema definition
   * @param[in] new_schema New schema definition
   */
  static void alterSchema(Session&,
                   const message::Schema &old_schema,
                   const message::Schema &new_schema);

  /**
   * Creates a CreateTable Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] session Session object which issued the statement
   * @param[in] table message::Table message describing new schema
   */
  static void createTable(Session&, const message::Table&);

  /**
   * Creates a DropTable Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] session Session object which issued the statement
   * @param[in] table Identifier for the table being dropped
   * @param[in] if_exists Did the user specify an IF EXISTS clause?
   */
  static void dropTable(Session&,
                 const identifier::Table& identifier,
                 message::table::const_reference table,
                 bool if_exists);

  /**
   * Creates a TruncateTable Statement GPB message and adds it
   * to the Session's active Transaction GPB message for pushing
   * out to the replicator streams.
   *
   * @param[in] session Session object which issued the statement
   * @param[in] table The Table being truncated
   */
  static void truncateTable(Session&, Table&);

  /**
   * Creates a new RawSql GPB message and pushes it to 
   * replicators.
   *
   * @TODO With a real data dictionary, this really shouldn't
   * be needed.  CREATE TABLE would map to insertRecord call
   * on the I_S, etc.  Not sure what to do with administrative
   * commands like CHECK TABLE, though..
   *
   * @param session Session object which issued the statement
   * @param query Query string
   * @param schema Schema for the table affected by the raw SQL.
   */
  static void rawStatement(Session&, const std::string &query, const std::string &schema);

  static void rawStatement(Session& session, const std::string& query)
  {
    rawStatement(session, query, "");
  }

  /* transactions: interface to plugin::StorageEngine functions */
  static int rollbackTransaction(Session&, bool all);

  /**
   * Commit the current transaction.
   *
   * @retval 0 ok
   * @retval 1 transaction was rolled back
   * @retval 2 error during commit, data may be inconsistent
   *
   * @todo
   * Since we don't support nested statement transactions in 5.0,
   * we can't commit or rollback stmt transactions while we are inside
   * stored functions or triggers. So we simply do nothing now.
   * This should be fixed in later ( >= 5.1) releases.
   */
  static int commitTransaction(Session&, bool all);

  /**
   * This is used to commit or rollback a single statement depending on
   * the value of error.
   *
   * @note
   * Note that if the autocommit is on, then the following call inside
   * InnoDB will commit or rollback the whole transaction (= the statement). The
   * autocommit mechanism built into InnoDB is based on counting locks, but if
   * the user has used LOCK TABLES then that mechanism does not know to do the
   * commit.
   */
  static int autocommitOrRollback(Session&, int error);

  /* savepoints */
  static int rollbackToSavepoint(Session&, NamedSavepoint &sv);
  static int setSavepoint(Session&, NamedSavepoint &sv);
  static int releaseSavepoint(Session&, NamedSavepoint &sv);

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
   * @param[in] session Session object
   * @param[in] monitored Descriptor for the resource which will be participating
   * @param[in] engine Pointer to the TransactionalStorageEngine resource
   */
  static void registerResourceForStatement(Session&, plugin::MonitoredInTransaction*, plugin::TransactionalStorageEngine*);

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
   * @param[in] session Session object
   * @param[in] monitored Descriptor for the resource which will be participating
   * @param[in] engine Pointer to the TransactionalStorageEngine resource
   * @param[in] resource_manager Pointer to the XaResourceManager resource manager
   */
  static void registerResourceForStatement(Session&,
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
  static void registerResourceForTransaction(Session&, plugin::MonitoredInTransaction*, plugin::TransactionalStorageEngine*);

  static void registerResourceForTransaction(Session&, plugin::MonitoredInTransaction*, plugin::TransactionalStorageEngine*, plugin::XaResourceManager*);

  static void allocateNewTransactionId();
 
  /**************
   * Events API
   **************/

  /**
   * Send server startup event.
   *
   * @param session Session object
   *
   * @retval true Success
   * @retval false Failure
   */
  static bool sendStartupEvent(Session&);

  /**
   * Send server shutdown event.
   *
   * @param session Session object
   *
   * @retval true Success
   * @retval false Failure
   */
  static bool sendShutdownEvent(Session&);

private:

  /**
   * Method which returns the active Transaction message
   * for the supplied Session.  If one is not found, a new Transaction
   * message is allocated, initialized, and returned. It is possible that
   * we may want to NOT increment the transaction id for a new Transaction
   * object (e.g., splitting up Transactions into smaller chunks). The
   * should_inc_trx_id flag controls if we do this.
   *
   * @param session The Session object processing the transaction
   * @param should_inc_trx_id If true, increments the transaction id for a new trx
   */
  static message::Transaction *getActiveTransactionMessage(Session&, bool should_inc_trx_id= true);

  /** 
   * Method which attaches a transaction context
   * the supplied transaction based on the supplied Session's
   * transaction information.  This method also ensure the
   * transaction message is attached properly to the Session object
   *
   * @param transaction The transaction message to initialize
   * @param session The Session object processing this transaction
   * @param should_inc_trx_id If true, increments the transaction id for a new trx
   */
  static void initTransactionMessage(message::Transaction&, Session&, bool should_inc_trx_id);
  
  /**
   * Helper method which initializes a Statement message
   *
   * @param statement The statement to initialize
   * @param type The type of the statement
   * @param session The Session object processing this statement
   */
  static void initStatementMessage(message::Statement&, message::Statement::Type type, const Session&);

  /** 
   * Helper method which finalizes data members for the 
   * supplied transaction's context.
   *
   * @param transaction The transaction message to finalize 
   * @param session The Session object processing this transaction
   */
  static void finalizeTransactionMessage(message::Transaction&, const Session&);

  /**
   * Helper method which deletes transaction memory and
   * unsets Session's transaction and statement messages.
   */
  static void cleanupTransactionMessage(message::Transaction *transaction, Session&);
  
  /** Helper method which returns an initialized Statement message for methods
   * doing insertion of data.
   *
   * @param[in] session Session object doing the processing
   * @param[in] table Table object being inserted into
   * @param[out] next_segment_id The next Statement segment id to be used
   */
  static message::Statement &getInsertStatement(Session&, Table&, uint32_t *next_segment_id);
  
  /**
   * Helper method which initializes the header message for
   * insert operations.
   *
   * @param[in,out] statement Statement message container to modify
   * @param[in] session Session object doing the processing
   * @param[in] table Table object being inserted into
   */
  static void setInsertHeader(message::Statement&,
                       const Session&,
                       Table&);
  /**
   * Helper method which returns an initialized Statement
   * message for methods doing updates of data.
   *
   * @param[in] session Session object doing the processing
   * @param[in] table Table object being updated
   * @param[in] old_record Pointer to the old data in the record
   * @param[in] new_record Pointer to the new data in the record
   * @param[out] next_segment_id The next Statement segment id to be used
   */
  static message::Statement &getUpdateStatement(Session&,
                                         Table&,
                                         const unsigned char *old_record, 
                                         const unsigned char *new_record,
                                         uint32_t *next_segment_id);
  /**
   * Helper method which initializes the header message for
   * update operations.
   *
   * @param[in,out] statement Statement message container to modify
   * @param[in] session Session object doing the processing
   * @param[in] table Table object being updated
   * @param[in] old_record Pointer to the old data in the record
   * @param[in] new_record Pointer to the new data in the record
   */
  static void setUpdateHeader(message::Statement&,
                       const Session&,
                       Table&,
                       const unsigned char *old_record, 
                       const unsigned char *new_record);

  /**
   * Helper method which returns an initialized Statement
   * message for methods doing deletion of data.
   *
   * @param[in] session Session object doing the processing
   * @param[in] table Table object being deleted from
   * @param[out] next_segment_id The next Statement segment id to be used
   */
  static message::Statement &getDeleteStatement(Session&, Table&, uint32_t *next_segment_id);
  
  /**
   * Helper method which initializes the header message for
   * insert operations.
   *
   * @param[in,out] statement Statement message container to modify
   * @param[in] session Session object doing the processing
   * @param[in] table Table object being deleted from
   */
  static void setDeleteHeader(message::Statement&, const Session&, Table&);

  /** 
   * Commits a normal transaction (see above) and pushes the transaction
   * message out to the replicators.
   *
   * @param session Session object committing the transaction
   */
  static int commitTransactionMessage(Session&);

  /** 
   * Marks the current active transaction message as being rolled back and
   * pushes the transaction message out to replicators.
   *
   * @param session Session object committing the transaction
   */
  static void rollbackTransactionMessage(Session&);

  /**
   * Rolls back the current statement, deleting the last Statement out of
   * the current Transaction message.
   *
   * @param session Session object committing the transaction
   *
   * @note This depends on having clear statement boundaries (i.e., one
   * Statement message per actual SQL statement).
   */
  static void rollbackStatementMessage(Session&);

  /**
   * Checks if a field has been updated 
   *
   * @param current_field Pointer to the field to check if it is updated 
   * @param table Table object containing update information
   * @param old_record Pointer to the raw bytes representing the old record/row
   * @param new_record Pointer to the raw bytes representing the new record/row
   */
  static bool isFieldUpdated(Field *current_field,
                      Table&,
                      const unsigned char *old_record,
                      const unsigned char *new_record);

  /**
   * Create a Transaction that contains event information and send it off.
   *
   * This differs from other uses of Transaction in that we don't use the
   * message associated with Session. We create a totally new message and
   * use it.
   *
   * @param session Session object
   * @param event Event message to send
   *
   * @note Used by the public Events API.
   *
   * @returns Non-zero on error
   */
  static int sendEvent(Session&, const message::Event &event);

  /**
   * Makes a given Transaction message segmented.
   *
   * The given Transaction message will have its segment information set
   * appropriately and a new Transaction message, containing the same
   * transaction ID as the supplied Transaction, and is created.
   *
   * @param session Session object
   * @param transaction Transaction message to segment.
   *
   * @returns Returns a pointer to a new Transaction message ready for use.
   */
  static message::Transaction *segmentTransactionMessage(Session&, message::Transaction*);

  static int commitPhaseOne(Session&, bool all);

  static uint64_t getCurrentTransactionId(Session&);
};

} /* namespace drizzled */

