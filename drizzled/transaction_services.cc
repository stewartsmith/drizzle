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
 * TransactionServices is a bridge between modules and the kernel, and its
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
 */

#include "drizzled/server_includes.h"
#include "drizzled/transaction_services.h"
#include "drizzled/plugin/replicator.h"
#include "drizzled/plugin/applier.h"
#include "drizzled/message/transaction.pb.h"
#include "drizzled/message/table.pb.h"
#include "drizzled/gettext.h"
#include "drizzled/session.h"
#include "drizzled/plugin_registry.h"

#include <vector>

drizzled::TransactionServices transaction_services;

void add_replicator(drizzled::plugin::Replicator *replicator)
{
  transaction_services.attachReplicator(replicator);
}

void remove_replicator(drizzled::plugin::Replicator *replicator)
{
  transaction_services.detachReplicator(replicator);
}

void add_applier(drizzled::plugin::Applier *applier)
{
  transaction_services.attachApplier(applier);
}

void remove_applier(drizzled::plugin::Applier *applier)
{
  transaction_services.detachApplier(applier);
}

namespace drizzled
{

void TransactionServices::attachReplicator(drizzled::plugin::Replicator *in_replicator)
{
  replicators.push_back(in_replicator);
}

void TransactionServices::detachReplicator(drizzled::plugin::Replicator *in_replicator)
{
  replicators.erase(std::find(replicators.begin(), replicators.end(), in_replicator));
}

void TransactionServices::attachApplier(drizzled::plugin::Applier *in_applier)
{
  appliers.push_back(in_applier);
}

void TransactionServices::detachApplier(drizzled::plugin::Applier *in_applier)
{
  appliers.erase(std::find(appliers.begin(), appliers.end(), in_applier));
}

void TransactionServices::setCommandTransactionContext(drizzled::message::Command *in_command
                                                     , Session *in_session) const
{
  using namespace drizzled::message;

  TransactionContext *trx= in_command->mutable_transaction_context();
  trx->set_server_id(in_session->getServerId());
  trx->set_transaction_id(in_session->getTransactionId());
}

void TransactionServices::startTransaction(Session *in_session)
{
  using namespace drizzled::message;
  
  if (replicators.size() == 0 || appliers.size() == 0)
    return;
  
  Command command;
  command.set_type(Command::START_TRANSACTION);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);
  
  push(&command);
}

void TransactionServices::commitTransaction(Session *in_session)
{
  using namespace drizzled::message;
  
  if (replicators.size() == 0 || appliers.size() == 0)
    return;
  
  Command command;
  command.set_type(Command::COMMIT);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);
  
  push(&command);
}

void TransactionServices::rollbackTransaction(Session *in_session)
{
  using namespace drizzled::message;
  
  if (replicators.size() == 0 || appliers.size() == 0)
    return;
  
  Command command;
  command.set_type(Command::ROLLBACK);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);
  
  push(&command);
}

void TransactionServices::insertRecord(Session *in_session, Table *in_table)
{
  using namespace drizzled::message;
  
  if (replicators.size() == 0 || appliers.size() == 0)
    return;

  Command command;
  command.set_type(Command::INSERT);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  command.set_schema(schema_name);
  command.set_table(table_name);

  /* 
   * Now we construct the specialized InsertRecord command inside
   * the Command container...
   */
  InsertRecord *change_record= command.mutable_insert_record();

  Field *current_field;
  Field **table_fields= in_table->field;
  String *string_value= new (in_session->mem_root) String(100); /* 100 initially. field->val_str() is responsible for re-adjusting */
  string_value->set_charset(system_charset_info);

  Table::Field *cur_field;

  while ((current_field= *table_fields++) != NULL) 
  {
    cur_field= change_record->add_insert_field();
    cur_field->set_name(std::string(current_field->field_name));
    cur_field->set_type(Table::Field::VARCHAR); /* @TODO real types! */
    string_value= current_field->val_str(string_value);
    change_record->add_insert_value(std::string(string_value->c_ptr()));
    string_value->free(); /* I wish there was a clear() method... */
  }

  if (string_value)
    delete string_value; /* Is this needed with memroot allocation? */
  
  push(&command);
}

void TransactionServices::updateRecord(Session *in_session, Table *in_table, const unsigned char *, const unsigned char *)
{
  using namespace drizzled::message;
  
  if (replicators.size() == 0 || appliers.size() == 0)
    return;
  
  Command command;
  command.set_type(Command::UPDATE);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  command.set_schema(schema_name);
  command.set_table(table_name);

  /* 
   * Now we construct the specialized UpdateRecord command inside
   * the Command container...
   */
  //UpdateRecord *change_record= command.mutable_update_record();

  push(&command);
}

void TransactionServices::deleteRecord(Session *in_session, Table *in_table)
{
  using namespace drizzled::message;
  
  if (replicators.size() == 0 || appliers.size() == 0)
    return;
  
  Command command;
  command.set_type(Command::DELETE);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);

  const char *schema_name= in_table->getShare()->db.str;
  const char *table_name= in_table->getShare()->table_name.str;

  command.set_schema(schema_name);
  command.set_table(table_name);

  /* 
   * Now we construct the specialized DeleteRecord command inside
   * the Command container...
   */
  //DeleteRecord *change_record= command.mutable_delete_record();
  
  push(&command);
}

void TransactionServices::rawStatement(Session *in_session, const char *in_query, size_t in_query_len)
{
  using namespace drizzled::message;
  
  if (replicators.size() == 0 || appliers.size() == 0)
    return;
  
  Command command;
  command.set_type(Command::RAW_SQL);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);

  std::string query(in_query, in_query_len);
  command.set_sql(query);

  push(&command);
  
}

void TransactionServices::push(drizzled::message::Command *to_push)
{
  std::vector<drizzled::plugin::Replicator *>::iterator repl_iter= replicators.begin();
  std::vector<drizzled::plugin::Applier *>::iterator appl_start_iter, appl_iter;
  appl_start_iter= appliers.begin();

  drizzled::plugin::Replicator *cur_repl;
  drizzled::plugin::Applier *cur_appl;

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
      ++appl_iter;
    }
    ++repl_iter;
  }
}


} /* end namespace drizzled */
