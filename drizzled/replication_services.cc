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
 * @see /drizzled/message/replication.proto
 */

#include "drizzled/server_includes.h"
#include "drizzled/replication_services.h"
#include "drizzled/plugin/command_replicator.h"
#include "drizzled/plugin/command_applier.h"
#include "drizzled/message/replication.pb.h"
#include "drizzled/message/table.pb.h"
#include "drizzled/gettext.h"
#include "drizzled/session.h"
#include "drizzled/plugin/registry.h"

#include <vector>

using namespace std;
using namespace drizzled;

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
  vector<plugin::CommandReplicator *>::iterator repl_iter= replicators.begin();
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
  vector<plugin::CommandApplier *>::iterator appl_iter= appliers.begin();
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

void ReplicationServices::attachReplicator(plugin::CommandReplicator *in_replicator)
{
  replicators.push_back(in_replicator);
  evaluateActivePlugins();
}

void ReplicationServices::detachReplicator(plugin::CommandReplicator *in_replicator)
{
  replicators.erase(std::find(replicators.begin(), replicators.end(), in_replicator));
  evaluateActivePlugins();
}

void ReplicationServices::attachApplier(plugin::CommandApplier *in_applier)
{
  appliers.push_back(in_applier);
  evaluateActivePlugins();
}

void ReplicationServices::detachApplier(plugin::CommandApplier *in_applier)
{
  appliers.erase(std::find(appliers.begin(), appliers.end(), in_applier));
  evaluateActivePlugins();
}

bool ReplicationServices::isActive() const
{
  return is_active;
}

void ReplicationServices::setCommandTransactionContext(message::Command &in_command,
                                                       Session *in_session) const
{
  message::TransactionContext *trx= in_command.mutable_transaction_context();
  trx->set_server_id(in_session->getServerId());
  trx->set_transaction_id(in_session->getTransactionId());

  in_command.set_session_id((uint32_t) in_session->getSessionId());
}

void ReplicationServices::startTransaction(Session *in_session)
{
  if (! is_active)
    return;
  
  message::Command command;
  command.set_type(message::Command::START_TRANSACTION);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(command, in_session);
  
  push(command);
}

void ReplicationServices::commitTransaction(Session *in_session)
{
  if (! is_active)
    return;
  
  message::Command command;
  command.set_type(message::Command::COMMIT);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(command, in_session);
  
  push(command);
}

void ReplicationServices::rollbackTransaction(Session *in_session)
{
  if (! is_active)
    return;
  
  message::Command command;
  command.set_type(message::Command::ROLLBACK);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(command, in_session);
  
  push(command);
}

void ReplicationServices::insertRecord(Session *in_session, Table *in_table)
{
  if (! is_active)
    return;

  message::Command command;
  command.set_type(message::Command::INSERT);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(command, in_session);

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  command.set_schema(schema_name);
  command.set_table(table_name);

  /* 
   * Now we construct the specialized InsertRecord command inside
   * the message::Command container...
   */
  message::InsertRecord *change_record= command.mutable_insert_record();

  Field *current_field;
  Field **table_fields= in_table->field;
  String *string_value= new (in_session->mem_root) String(ReplicationServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  message::Table::Field *current_proto_field;

  /* We will read all the table's fields... */
  in_table->setReadSet();

  while ((current_field= *table_fields++) != NULL) 
  {
    current_proto_field= change_record->add_insert_field();
    current_proto_field->set_name(current_field->field_name);
    current_proto_field->set_type(message::Table::Field::VARCHAR); /* @TODO real types! */
    string_value= current_field->val_str(string_value);
    change_record->add_insert_value(string_value->c_ptr());
    string_value->free();
  }
  
  push(command);
}

void ReplicationServices::updateRecord(Session *in_session,
                                       Table *in_table, 
                                       const unsigned char *old_record, 
                                       const unsigned char *new_record)
{
  if (! is_active)
    return;
  
  message::Command command;
  command.set_type(message::Command::UPDATE);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(command, in_session);

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  command.set_schema(schema_name);
  command.set_table(table_name);

  /* 
   * Now we construct the specialized UpdateRecord command inside
   * the message::Command container...
   */
  message::UpdateRecord *change_record= command.mutable_update_record();

  Field *current_field;
  Field **table_fields= in_table->field;
  String *string_value= new (in_session->mem_root) String(ReplicationServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  message::Table::Field *current_proto_field;

  while ((current_field= *table_fields++) != NULL) 
  {
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
      current_proto_field= change_record->add_update_field();
      current_proto_field->set_name(current_field->field_name);
      current_proto_field->set_type(message::Table::Field::VARCHAR); /* @TODO real types! */

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

      change_record->add_after_value(string_value->c_ptr());
      string_value->free();
    }

    /* 
     * Add the WHERE clause values now...the fields which return true
     * for isReadSet() are in the WHERE clause.  For tables with no
     * primary or unique key, all fields will be returned.
     */
    if (current_field->isReadSet())
    {
      current_proto_field= change_record->add_where_field();
      current_proto_field->set_name(current_field->field_name);
      current_proto_field->set_type(message::Table::Field::VARCHAR); /* @TODO real types! */
      string_value= current_field->val_str(string_value);
      change_record->add_where_value(string_value->c_ptr());
      string_value->free();
    }
  }

  push(command);
}

void ReplicationServices::deleteRecord(Session *in_session, Table *in_table)
{
  if (! is_active)
    return;
  
  message::Command command;
  command.set_type(message::Command::DELETE);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(command, in_session);

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  command.set_schema(schema_name);
  command.set_table(table_name);

  /* 
   * Now we construct the specialized DeleteRecord command inside
   * the message::Command container...
   */
  message::DeleteRecord *change_record= command.mutable_delete_record();
 
  Field *current_field;
  Field **table_fields= in_table->field;
  String *string_value= new (in_session->mem_root) String(ReplicationServices::DEFAULT_RECORD_SIZE);
  string_value->set_charset(system_charset_info);

  message::Table::Field *current_proto_field;

  while ((current_field= *table_fields++) != NULL)
  {
    /*
     * Add the WHERE clause values now...the fields which return true
     * for isReadSet() are in the WHERE clause.  For tables with no
     * primary or unique key, all fields will be returned.
     */
    if (current_field->isReadSet())
    {
      current_proto_field= change_record->add_where_field();
      current_proto_field->set_name(current_field->field_name);
      current_proto_field->set_type(message::Table::Field::VARCHAR); /* @TODO real types! */
      string_value= current_field->val_str(string_value);
      change_record->add_where_value(string_value->c_ptr());
      string_value->free();
    }
  }
 
  push(command);
}

void ReplicationServices::rawStatement(Session *in_session, const char *in_query, size_t in_query_len)
{
  if (! is_active)
    return;
  
  message::Command command;
  command.set_type(message::Command::RAW_SQL);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(command, in_session);

  string query(in_query, in_query_len);
  command.set_sql(query);

  push(command);
}

void ReplicationServices::push(drizzled::message::Command &to_push)
{
  vector<plugin::CommandReplicator *>::iterator repl_iter= replicators.begin();
  vector<plugin::CommandApplier *>::iterator appl_start_iter, appl_iter;
  appl_start_iter= appliers.begin();

  plugin::CommandReplicator *cur_repl;
  plugin::CommandApplier *cur_appl;

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
       * We update the timestamp for the last applied Command so that
       * publisher plugins can ask the replication services when the
       * last known applied Command was using the getLastAppliedTimestamp()
       * method.
       */
      last_applied_timestamp.fetch_and_store(to_push.timestamp());
      ++appl_iter;
    }
    ++repl_iter;
  }
}
