/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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
 * @file Server-side utility which is responsible for managing the 
 * communication between the kernel, replicator plugins, and applier plugins.
 *
 * ReplicationServices is a bridge between modules and the kernel, and its
 * primary function is to take internal events (for instance the start of a 
 * transaction, the changing of a record, or the rollback of a transaction) 
 * and construct GPB Messages that are passed to the registered replicator and
 * applier plugins.
 *
 * The reason for this functionality is to encapsulate all communication
 * between the kernel and the replicator/applier plugins into GPB Messages.
 * Instead of the plugin having to understand the (often fluidly changing)
 * mechanics of the kernel, all the plugin needs to understand is the message
 * format, and GPB messages provide a nice, clear, and versioned format for 
 * these messages.
 *
 * @see /drizzled/message/transaction.proto
 *
 * @TODO
 *
 * We really should store the raw bytes in the messages, not the
 * String value of the Field.  But, to do that, the
 * statement_transform library needs first to be updated
 * to include the transformation code to convert raw
 * Drizzle-internal Field byte representation into something
 * plugins can understand.
 */

#include "drizzled/server_includes.h"
#include "drizzled/replication_services.h"
#include "drizzled/plugin/transaction_replicator.h"
#include "drizzled/plugin/transaction_applier.h"
#include "drizzled/message/transaction.pb.h"
#include "drizzled/message/table.pb.h"
#include "drizzled/gettext.h"
#include "drizzled/session.h"

#include <vector>

using namespace std;
using namespace drizzled;

static message::Table::Field::FieldType internalFieldTypeToFieldProtoType(enum enum_field_types in_type);

ReplicationServices::ReplicationServices()
{
  is_active= false;
}

void ReplicationServices::evaluateActivePlugins()
{
  /* 
   * We loop through replicators and appliers, evaluating
   * whether or not there is at least one active replicator
   * and one active applier.  If not, we set is_active
   * to false.
   */
  bool tmp_is_active= false;

  if (replicators.empty() || appliers.empty())
  {
    is_active= false;
    return;
  }

  /* 
   * Determine if any remaining replicators and if those
   * replicators are active...if not, set is_active
   * to false
   */
  vector<plugin::TransactionReplicator *>::iterator repl_iter= replicators.begin();
  while (repl_iter != replicators.end())
  {
    if ((*repl_iter)->isActive())
    {
      tmp_is_active= true;
      break;
    }
    ++repl_iter;
  }
  if (! tmp_is_active)
  {
    /* No active replicators. Set is_active to false and exit. */
    is_active= false;
    return;
  }

  /* 
   * OK, we know there's at least one active replicator.
   *
   * Now determine if any remaining replicators and if those
   * replicators are active...if not, set is_active
   * to false
   */
  vector<plugin::TransactionApplier *>::iterator appl_iter= appliers.begin();
  while (appl_iter != appliers.end())
  {
    if ((*appl_iter)->isActive())
    {
      is_active= true;
      return;
    }
    ++appl_iter;
  }
  /* If we get here, there are no active appliers */
  is_active= false;
}

void ReplicationServices::attachReplicator(plugin::TransactionReplicator *in_replicator)
{
  replicators.push_back(in_replicator);
  evaluateActivePlugins();
}

void ReplicationServices::detachReplicator(plugin::TransactionReplicator *in_replicator)
{
  replicators.erase(std::find(replicators.begin(), replicators.end(), in_replicator));
  evaluateActivePlugins();
}

void ReplicationServices::attachApplier(plugin::TransactionApplier *in_applier)
{
  appliers.push_back(in_applier);
  evaluateActivePlugins();
}

void ReplicationServices::detachApplier(plugin::TransactionApplier *in_applier)
{
  appliers.erase(std::find(appliers.begin(), appliers.end(), in_applier));
  evaluateActivePlugins();
}

bool ReplicationServices::isActive() const
{
  return is_active;
}

message::Transaction *ReplicationServices::getActiveTransaction(Session *in_session) const
{
  message::Transaction *transaction= in_session->getTransactionMessage();

  if (unlikely(transaction == NULL))
  {
    /* 
     * Allocate and initialize a new transaction message 
     * for this Session object.  Session is responsible for
     * deleting transaction message when done with it.
     */
    transaction= new (nothrow) message::Transaction();
    initTransaction(*transaction, in_session);
    in_session->setTransactionMessage(transaction);
    return transaction;
  }
  else
    return transaction;
}

void ReplicationServices::initTransaction(message::Transaction &in_transaction,
                                          Session *in_session) const
{
  message::TransactionContext *trx= in_transaction.mutable_transaction_context();
  trx->set_server_id(in_session->getServerId());
  trx->set_transaction_id(in_session->getTransactionId());
  trx->set_start_timestamp(in_session->getCurrentTimestamp());
}

void ReplicationServices::finalizeTransaction(message::Transaction &in_transaction,
                                              Session *in_session) const
{
  message::TransactionContext *trx= in_transaction.mutable_transaction_context();
  trx->set_end_timestamp(in_session->getCurrentTimestamp());
}

void ReplicationServices::cleanupTransaction(message::Transaction *in_transaction,
                                             Session *in_session) const
{
  delete in_transaction;
  in_session->setTransactionMessage(NULL);
  in_session->setStatementMessage(NULL);
}

void ReplicationServices::startNormalTransaction(Session *in_session)
{
  if (! is_active)
    return;

  /* Safeguard...other transactions should have already been closed */
  message::Transaction *transaction= in_session->getTransactionMessage();
  assert(transaction == NULL);
  
  /* 
   * A "normal" transaction for the replication services component
   * is simply a Transaction message, nothing more...so we create a
   * new message and attach it to the Session object.
   *
   * Allocate and initialize a new transaction message 
   * for this Session object.  This memory is deleted when the
   * transaction is pushed out to replicators.  Session is NOT
   * responsible for deleting this memory.
   */
  transaction= new (nothrow) message::Transaction();
  initTransaction(*transaction, in_session);
  in_session->setTransactionMessage(transaction);
}

void ReplicationServices::commitNormalTransaction(Session *in_session)
{
  if (! is_active)
    return;

  /* If there is an active statement message, finalize it */
  message::Statement *statement= in_session->getStatementMessage();

  if (statement != NULL)
  {
    finalizeStatement(*statement, in_session);
  }
  
  message::Transaction* transaction= getActiveTransaction(in_session);

  finalizeTransaction(*transaction, in_session);
  
  push(*transaction);

  cleanupTransaction(transaction, in_session);
}

void ReplicationServices::initStatement(message::Statement &statement,
                                        message::Statement::Type in_type,
                                        Session *in_session) const
{
  statement.set_type(in_type);
  statement.set_start_timestamp(in_session->getCurrentTimestamp());
  /** @TODO Set sql string optionally */
}

void ReplicationServices::finalizeStatement(message::Statement &statement,
                                            Session *in_session) const
{
  statement.set_end_timestamp(in_session->getCurrentTimestamp());
}

void ReplicationServices::rollbackTransaction(Session *in_session)
{
  if (! is_active)
    return;
  
  message::Transaction *transaction= getActiveTransaction(in_session);

  /* 
   * We clear the current transaction, reset its transaction ID properly, 
   * then set its type to ROLLBACK and push it out to the replicators.
   */
  transaction->Clear();
  initTransaction(*transaction, in_session);

  message::Statement *statement= transaction->add_statement();

  initStatement(*statement, message::Statement::ROLLBACK, in_session);
  finalizeStatement(*statement, in_session);

  finalizeTransaction(*transaction, in_session);
  
  push(*transaction);

  cleanupTransaction(transaction, in_session);
}

message::Statement &ReplicationServices::getInsertStatement(Session *in_session,
                                                            Table *in_table) const
{
  message::Statement *statement= in_session->getStatementMessage();

  if (statement == NULL)
  {
    message::Transaction *transaction= getActiveTransaction(in_session);
    /* 
     * Transaction message initialized and set, but no statement created
     * yet.  We construct one and initialize it, here, then return the
     * message after attaching the new Statement message pointer to the 
     * Session for easy retrieval later...
     */
    statement= transaction->add_statement();
    setInsertHeader(*statement, in_session, in_table);
    in_session->setStatementMessage(statement);
  }
  return *statement;
}

void ReplicationServices::setInsertHeader(message::Statement &statement,
                                          Session *in_session,
                                          Table *in_table) const
{
  initStatement(statement, message::Statement::INSERT, in_session);

  /* 
   * Now we construct the specialized InsertHeader message inside
   * the generalized message::Statement container...
   */
  /* Set up the insert header */
  message::InsertHeader *header= statement.mutable_insert_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  table_metadata->set_schema_name(schema_name);
  table_metadata->set_table_name(table_name);

  Field *current_field;
  Field **table_fields= in_table->field;

  message::FieldMetadata *field_metadata;

  /* We will read all the table's fields... */
  in_table->setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    field_metadata= header->add_field_metadata();
    field_metadata->set_name(current_field->field_name);
    field_metadata->set_type(internalFieldTypeToFieldProtoType(current_field->type()));
  }
}

void ReplicationServices::insertRecord(Session *in_session, Table *in_table)
{
  if (! is_active)
    return;

  message::Statement &statement= getInsertStatement(in_session, in_table);

  message::InsertData *data= statement.mutable_insert_data();
  data->set_segment_id(1);
  data->set_end_segment(true);
  message::InsertRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= in_table->field;

  String *string_value= new (in_session->mem_root) String(ReplicationServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  /* We will read all the table's fields... */
  in_table->setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    string_value= current_field->val_str(string_value);
    record->add_insert_value(string_value->c_ptr());
    string_value->free();
  }
}

message::Statement &ReplicationServices::getUpdateStatement(Session *in_session,
                                                            Table *in_table,
                                                            const unsigned char *old_record, 
                                                            const unsigned char *new_record) const
{
  message::Statement *statement= in_session->getStatementMessage();

  if (statement == NULL)
  {
    message::Transaction *transaction= getActiveTransaction(in_session);
    /* 
     * Transaction message initialized and set, but no statement created
     * yet.  We construct one and initialize it, here, then return the
     * message after attaching the new Statement message pointer to the 
     * Session for easy retrieval later...
     */
    statement= transaction->add_statement();
    setUpdateHeader(*statement, in_session, in_table, old_record, new_record);
    in_session->setStatementMessage(statement);
  }
  return *statement;
}

void ReplicationServices::setUpdateHeader(message::Statement &statement,
                                          Session *in_session,
                                          Table *in_table,
                                          const unsigned char *old_record, 
                                          const unsigned char *new_record) const
{
  initStatement(statement, message::Statement::UPDATE, in_session);

  /* 
   * Now we construct the specialized UpdateHeader message inside
   * the generalized message::Statement container...
   */
  /* Set up the update header */
  message::UpdateHeader *header= statement.mutable_update_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  table_metadata->set_schema_name(schema_name);
  table_metadata->set_table_name(table_name);

  Field *current_field;
  Field **table_fields= in_table->field;
  String *string_value= new (in_session->mem_root) String(ReplicationServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  message::FieldMetadata *field_metadata;

  /* We will read all the table's fields... */
  in_table->setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    /*
     * We add the "key field metadata" -- i.e. the fields which is
     * the primary key for the table.
     */
    if (in_table->s->primary_key == current_field->field_index)
    {
      field_metadata= header->add_key_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(internalFieldTypeToFieldProtoType(current_field->type()));
    }

    /*
     * The below really should be moved into the Field API and Record API.  But for now
     * we do this crazy pointer fiddling to figure out if the current field
     * has been updated in the supplied record raw byte pointers.
     */
    const unsigned char *old_ptr= (const unsigned char *) old_record + (ptrdiff_t) (current_field->ptr - in_table->record[0]); 
    const unsigned char *new_ptr= (const unsigned char *) new_record + (ptrdiff_t) (current_field->ptr - in_table->record[0]); 

    uint32_t field_length= current_field->pack_length(); /** @TODO This isn't always correct...check varchar diffs. */

    if (memcmp(old_ptr, new_ptr, field_length) != 0)
    {
      /* Field is changed from old to new */
      field_metadata= header->add_set_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(internalFieldTypeToFieldProtoType(current_field->type()));

      /* Store the original "read bit" for this field */
      bool is_read_set= current_field->isReadSet();

      /* We need to mark that we will "read" this field... */
      in_table->setReadSet(current_field->field_index);

      /* Read the string value of this field's contents */
      string_value= current_field->val_str(string_value);

      /* 
       * Reset the read bit after reading field to its original state.  This 
       * prevents the field from being included in the WHERE clause
       */
      current_field->setReadSet(is_read_set);

      header->add_set_value(string_value->c_ptr());
      string_value->free();
    }
  }
}
void ReplicationServices::updateRecord(Session *in_session,
                                       Table *in_table, 
                                       const unsigned char *old_record, 
                                       const unsigned char *new_record)
{
  if (! is_active)
    return;

  message::Statement &statement= getUpdateStatement(in_session, in_table, old_record, new_record);

  message::UpdateData *data= statement.mutable_update_data();
  data->set_segment_id(1);
  data->set_end_segment(true);
  message::UpdateRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= in_table->field;
  String *string_value= new (in_session->mem_root) String(ReplicationServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  while ((current_field= *table_fields++) != NULL) 
  {
    /* 
     * Add the WHERE clause values now...the fields which return true
     * for isReadSet() are in the WHERE clause.  For tables with no
     * primary or unique key, all fields will be returned.
     */
    if (current_field->isReadSet())
    {
      string_value= current_field->val_str(string_value);
      record->add_key_value(string_value->c_ptr());
      /**
       * @TODO Store optional old record value in the before data member
       */
      string_value->free();
    }
  }
}

message::Statement &ReplicationServices::getDeleteStatement(Session *in_session,
                                                            Table *in_table) const
{
  message::Statement *statement= in_session->getStatementMessage();

  if (statement == NULL)
  {
    message::Transaction *transaction= getActiveTransaction(in_session);
    /* 
     * Transaction message initialized and set, but no statement created
     * yet.  We construct one and initialize it, here, then return the
     * message after attaching the new Statement message pointer to the 
     * Session for easy retrieval later...
     */
    statement= transaction->add_statement();
    setDeleteHeader(*statement, in_session, in_table);
    in_session->setStatementMessage(statement);
  }
  return *statement;
}

void ReplicationServices::setDeleteHeader(message::Statement &statement,
                                          Session *in_session,
                                          Table *in_table) const
{
  initStatement(statement, message::Statement::DELETE, in_session);

  /* 
   * Now we construct the specialized DeleteHeader message inside
   * the generalized message::Statement container...
   */
  message::DeleteHeader *header= statement.mutable_delete_header();
  message::TableMetadata *table_metadata= header->mutable_table_metadata();

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  table_metadata->set_schema_name(schema_name);
  table_metadata->set_table_name(table_name);

  Field *current_field;
  Field **table_fields= in_table->field;

  message::FieldMetadata *field_metadata;

  while ((current_field= *table_fields++) != NULL) 
  {
    /* 
     * Add the WHERE clause values now...the fields which return true
     * for isReadSet() are in the WHERE clause.  For tables with no
     * primary or unique key, all fields will be returned.
     */
    if (current_field->isReadSet())
    {
      field_metadata= header->add_key_field_metadata();
      field_metadata->set_name(current_field->field_name);
      field_metadata->set_type(internalFieldTypeToFieldProtoType(current_field->type()));
    }
  }
}

void ReplicationServices::deleteRecord(Session *in_session, Table *in_table)
{
  if (! is_active)
    return;

  message::Statement &statement= getDeleteStatement(in_session, in_table);

  message::DeleteData *data= statement.mutable_delete_data();
  data->set_segment_id(1);
  data->set_end_segment(true);
  message::DeleteRecord *record= data->add_record();

  Field *current_field;
  Field **table_fields= in_table->field;
  String *string_value= new (in_session->mem_root) String(ReplicationServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  while ((current_field= *table_fields++) != NULL) 
  {
    /* 
     * Add the WHERE clause values now...the fields which return true
     * for isReadSet() are in the WHERE clause.  For tables with no
     * primary or unique key, all fields will be returned.
     */
    if (current_field->isReadSet())
    {
      string_value= current_field->val_str(string_value);
      record->add_key_value(string_value->c_ptr());
      /**
       * @TODO Store optional old record value in the before data member
       */
      string_value->free();
    }
  }
}

void ReplicationServices::rawStatement(Session *in_session, const char *in_query, size_t in_query_len)
{
  if (! is_active)
    return;
  
  message::Transaction *transaction= getActiveTransaction(in_session);
  message::Statement *statement= transaction->add_statement();

  initStatement(*statement, message::Statement::RAW_SQL, in_session);
  string query(in_query, in_query_len);
  statement->set_sql(query);
  finalizeStatement(*statement, in_session);

  finalizeTransaction(*transaction, in_session);
  
  push(*transaction);

  cleanupTransaction(transaction, in_session);
}

void ReplicationServices::push(drizzled::message::Transaction &to_push)
{
  vector<plugin::TransactionReplicator *>::iterator repl_iter= replicators.begin();
  vector<plugin::TransactionApplier *>::iterator appl_start_iter, appl_iter;
  appl_start_iter= appliers.begin();

  plugin::TransactionReplicator *cur_repl;
  plugin::TransactionApplier *cur_appl;

  while (repl_iter != replicators.end())
  {
    cur_repl= *repl_iter;
    if (! cur_repl->isActive())
    {
      ++repl_iter;
      continue;
    }
    
    appl_iter= appl_start_iter;
    while (appl_iter != appliers.end())
    {
      cur_appl= *appl_iter;

      if (! cur_appl->isActive())
      {
        ++appl_iter;
        continue;
      }

      cur_repl->replicate(cur_appl, to_push);
      
      /* 
       * We update the timestamp for the last applied Transaction so that
       * publisher plugins can ask the replication services when the
       * last known applied Transaction was using the getLastAppliedTimestamp()
       * method.
       */
      last_applied_timestamp.fetch_and_store(to_push.transaction_context().end_timestamp());
      ++appl_iter;
    }
    ++repl_iter;
  }
}

static message::Table::Field::FieldType internalFieldTypeToFieldProtoType(enum enum_field_types in_type)
{
  switch (in_type)
  {
    case DRIZZLE_TYPE_LONGLONG:
      return message::Table::Field::BIGINT;
    case DRIZZLE_TYPE_LONG:
      return message::Table::Field::INTEGER;
    case DRIZZLE_TYPE_NEWDECIMAL:
      return message::Table::Field::DECIMAL;
    case DRIZZLE_TYPE_DOUBLE:
      return message::Table::Field::DOUBLE;
    case DRIZZLE_TYPE_DATE:
      return message::Table::Field::DATE;
    case DRIZZLE_TYPE_DATETIME:
      return message::Table::Field::DATETIME;
    case DRIZZLE_TYPE_TIMESTAMP:
      return message::Table::Field::TIMESTAMP;
    case DRIZZLE_TYPE_VARCHAR:
      return message::Table::Field::VARCHAR;
    default:
      return message::Table::Field::VARCHAR;
  }
}
