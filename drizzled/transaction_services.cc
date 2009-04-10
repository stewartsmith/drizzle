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

#include <vector>

drizzled::TransactionServices transaction_services;

/**
 * @TODO
 *
 * We're going to start simple at first, meaning that the 
 * below are the global vectors of replicators and appliers. The
 * end goal is to have the TransactionServices have a register method
 * which allows modules to register Replicator or Applier *factories*, 
 * which will allow TransactionServices to attach and detach a replicator/applier
 * to a Session, instead of the current global vector.
 */
int replicator_initializer(st_plugin_int *plugin)
{
  drizzled::plugin::Replicator *repl= NULL;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init(&repl))
    {
      /* TRANSLATORS: The leading word "replicator" is the name
        of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("replicator plugin '%s' init() failed"),
                    plugin->name.str);
      return 1;
    }
  }

  if (repl == NULL)
    return 1;

  transaction_services.attachReplicator(repl);
  plugin->data= repl;

  return 0;
}

int replicator_finalizer(st_plugin_int *plugin)
{
  drizzled::plugin::Replicator *repl= static_cast<drizzled::plugin::Replicator *>(plugin->data);
  
  assert(repl);

  transaction_services.detachReplicator(repl);

  if (plugin->plugin->deinit)
    (void) plugin->plugin->deinit(repl);

  return 0;
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
  
  Command command;
  command.set_type(Command::START_TRANSACTION);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);
  
  push(&command);
}

void TransactionServices::commitTransaction(Session *in_session)
{
  using namespace drizzled::message;
  
  Command command;
  command.set_type(Command::COMMIT);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);
  
  push(&command);
}

void TransactionServices::rollbackTransaction(Session *in_session)
{
  using namespace drizzled::message;
  
  Command command;
  command.set_type(Command::ROLLBACK);
  command.set_timestamp(in_session->getCurrentTimestamp());

  setCommandTransactionContext(&command, in_session);
  
  push(&command);
}

void TransactionServices::insertRecord(Session *in_session, Table *in_table)
{
  using namespace drizzled::message;
  
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
  
  push(&command);
}

void TransactionServices::updateRecord(Session *in_session, Table *in_table, const unsigned char *, const unsigned char *)
{
  using namespace drizzled::message;
  
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

#ifdef oldcode
/* This gets called by plugin_foreach once for each loaded replicator plugin */
static bool replicator_session_iterate(Session *session, plugin_ref plugin, void *)
{
  drizzled::plugin::Replicator *repl= plugin_data(plugin, drizzled::plugin::Replicator *);

  if (! repl || ! repl->isActive())
    return false;

  /* call this loaded replicator plugin's session_init method */
  if (! repl->initSession(session))
  {
    /* TRANSLATORS: The leading word "replicator" is the name
      of the plugin api, and so should not be translated. */
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("replicator plugin '%s' session_init() failed"),
                  (char *)plugin_name(plugin));
    return true;
  }
  return false;
}

/*
  This call is called once at the begining of each transaction.
*/
extern StorageEngine *binlog_engine;
bool replicator_session_init(Session *session)
{
  bool foreach_rv;

  if (session->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
    trans_register_ha(session, true, binlog_engine);
  trans_register_ha(session, false, binlog_engine);

  /* 
    call replicator_session_iterate
    once for each loaded replicator plugin
  */
  foreach_rv= plugin_foreach(session, replicator_session_iterate,
                             DRIZZLE_REPLICATOR_PLUGIN, NULL);

  return foreach_rv;
}
/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the session.
   So we will take all the additional paramters of replicator_do2,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
*/
enum repl_row_exec_t
{
  repl_insert,
  repl_update,
  repl_delete
};

typedef struct replicator_row_parms_st
{
  repl_row_exec_t type;
  Table *table;
  const unsigned char *before;
  const unsigned char *after;
} replicator_row_parms_st;


/* This gets called by plugin_foreach once for each loaded replicator plugin */
static bool replicator_do_row_iterate (Session *session, plugin_ref plugin, void *p)
{
  drizzled::plugin::Replicator *repl= plugin_data(plugin, drizzled::plugin::Replicator *);

  if (! repl || ! repl->isActive())
    return false;

  replicator_row_parms_st *params= static_cast<replicator_row_parms_st *>(p);

  /* 
   * We create a container ChangeRecord and then specialize the change
   * record depending on the actual event which occurred...
   */
  drizzled::message::ChangeRecord change_record;
  /** 
   * When inserting a row, we want to pass the replicator only the
   * specific information it needs, which is the name of the schema, 
   * the name of the table, the list of field names in the field list of
   * the INSERT expression, and the values of this written row as strings.
   *
   * @TODO  Eventually, it would be better to simply pass a Table 
   *        proto message instead of both a schema name and a table
   *        name.
   *
   * @TODO  Better to not have to pass string values...
   *
   * @TODO  Pass pointers here instead of copies of values/field names?
   *        For large inserts, might be in trouble of running out of 
   *        stack space? Not sure...
   *
   * @TODO  Ugh, get rid of the friggin custom String shit.
   */
  const char *schema_name= params->table->getShare()->db.str;
  const char *table_name= params->table->getShare()->table_name.str;

  change_record.set_schema(schema_name);
  change_record.set_table(table_name);
  switch (params->type) 
  {
    case repl_insert:
    {

      drizzled::message::InsertRecord *irecord= change_record.mutable_insert_record();

      std::vector<std::string> values;
      std::vector<std::string> field_names;

      Field *current_field;
      Field **table_fields= params->table->field;
      String *string_value= new (session->mem_root) String(100); /* 100 initially. field->val_str() is responsible for re-adjusting */
      string_value->set_charset(system_charset_info);

      while ((current_field= *table_fields++) != NULL) 
      {

        field_names.push_back(std::string(current_field->field_name));
        string_value= current_field->val_str(string_value);
        values.push_back(std::string(string_value->c_ptr()));
        string_value->free(); /* I wish there was a clear() method... */
      }

      repl->replicate(appl, &change_record);
      if (string_value)
        delete string_value;
      break;
    }
    case repl_update:
    {
      if (! repl->updateRow(session, params->table, params->before, params->after))
      {
        /* TRANSLATORS: The leading word "replicator" is the name
          of the plugin api, and so should not be translated. */
        errmsg_printf(ERRMSG_LVL_ERROR,
                      _("replicator plugin '%s' row_update() failed"),
                      (char *)plugin_name(plugin));

        return true;
      }
      break;
    }
    case repl_delete:
    {
      if (! repl->deleteRow(session, params->table))
      {
        /* TRANSLATORS: The leading word "replicator" is the name
          of the plugin api, and so should not be translated. */
        errmsg_printf(ERRMSG_LVL_ERROR,
                      _("replicator plugin '%s' row_delete() failed"),
                      (char *)plugin_name(plugin));

        return true;
      }
      break;
    }
  }
  return false;
}

/* This is the replicator_do_row entry point.
   This gets called by the rest of the Drizzle server code */
static bool replicator_do_row (Session *session,
                               replicator_row_parms_st *params)
{
  bool foreach_rv;

  foreach_rv= plugin_foreach(session, replicator_do_row_iterate,
                             DRIZZLE_REPLICATOR_PLUGIN, params);
  return foreach_rv;
}

bool replicator_write_row(Session *session, Table *table)
{
  replicator_row_parms_st param;

  param.type= repl_insert;
  param.table= table;
  param.after= NULL;
  param.before= NULL;

  return replicator_do_row(session, &param);
}

bool replicator_update_row(Session *session, Table *table,
                           const unsigned char *before,
                           const unsigned char *after)
{
  replicator_row_parms_st param;

  param.type= repl_update;
  param.table= table;
  param.after= after;
  param.before= before;

  return replicator_do_row(session, &param);
}

bool replicator_delete_row(Session *session, Table *table)
{
  replicator_row_parms_st param;

  param.type= repl_delete;
  param.table= table;
  param.after= NULL;
  param.before= NULL;

  return replicator_do_row(session, &param);
}

/*
  Here be Dragons!

  Ok, not so much dragons, but this is where we handle either commits or rollbacks of
  statements.
*/
typedef struct replicator_row_end_st
{
  bool autocommit;
  bool commit;
} replicator_row_end_st;

/* We call this to end a statement (on each registered plugin) */
static bool replicator_end_transaction_iterate (Session *session, plugin_ref plugin, void *p)
{
  drizzled::plugin::Replicator *repl= plugin_data(plugin, drizzled::plugin::Replicator *);
  replicator_row_end_st *params= static_cast<replicator_row_end_st *>(p);

  if (! repl || ! repl->isActive())
    return false;

  /* call this loaded replicator plugin's replicator_func1 function pointer */
  if (! repl->endTransaction(session, params->autocommit, params->commit))
  {
    /* TRANSLATORS: The leading word "replicator" is the name
      of the plugin api, and so should not be translated. */
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("replicator plugin '%s' end_transaction() failed"),
                  (char *)plugin_name(plugin));
    return true;
  }
  return false;
}

bool replicator_end_transaction(Session *session, bool autocommit, bool commit)
{
  bool foreach_rv;
  replicator_row_end_st params;

  params.autocommit= autocommit;
  params.commit= commit;

  /* We need to free any data we did an init of for the session */
  foreach_rv= plugin_foreach(session, replicator_end_transaction_iterate,
                             DRIZZLE_REPLICATOR_PLUGIN, (void *) &params);

  return foreach_rv;
}

/*
  If you can do real 2PC this is your hook poing to know that the event is coming.

  Always true for the moment.

*/
bool replicator_prepare(Session *)
{
  return false;
}

/*
  Replicate statement.
*/
typedef struct replicator_statement_st
{
  const char *query;
  size_t query_length;
} replicator_statement_st;

/* We call this to begin a statement (on each registered plugin) */
static bool replicator_statement_iterate(Session *session, plugin_ref plugin, void *p)
{
  drizzled::plugin::Replicator *repl= plugin_data(plugin, drizzled::plugin::Replicator *);
  replicator_statement_st *params= (replicator_statement_st *)p;

  if (! repl || ! repl->isActive())
    return false;

  /* call this loaded replicator plugin's replicator_func1 function pointer */
  if (! repl->beginStatement(session, params->query, params->query_length))
  {
    /* TRANSLATORS: The leading word "replicator" is the name
      of the plugin api, and so should not be translated. */
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("replicator plugin '%s' statement() failed"),
                  (char *)plugin_name(plugin));
    return true;
  }
  return false;
}

bool replicator_statement(Session *session, const char *query, size_t query_length)
{
  bool foreach_rv;
  replicator_statement_st params;
  
  params.query= query;
  params.query_length= query_length;

  /* We need to free any data we did an init of for the session */
  foreach_rv= plugin_foreach(session, replicator_statement_iterate,
                             DRIZZLE_REPLICATOR_PLUGIN, (void *) &params);

  return foreach_rv;
}
#endif /* oldcode */

} /* end namespace drizzled */
