/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include <plugin/data_engine/dictionary.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/session_list.h>
#include "drizzled/plugin/client.h"
#include "drizzled/internal/my_sys.h"

using namespace std;
using namespace drizzled;

ProcesslistTool::ProcesslistTool()
{
  message::Table::StorageEngine *engine;
  message::Table::TableOptions *table_options;

  schema.set_name("processlist");
  schema.set_type(message::Table::STANDARD);

  table_options= schema.mutable_options();
  table_options->set_collation_id(default_charset_info->number);
  table_options->set_collation(default_charset_info->name);

  engine= schema.mutable_engine();
  engine->set_name(engine_name);

  add_field(schema, "ID", message::Table::Field::BIGINT);
  add_field(schema, "USER", message::Table::Field::VARCHAR, 16);
  add_field(schema, "HOST", message::Table::Field::VARCHAR, 64);
  add_field(schema, "DB", message::Table::Field::VARCHAR, 64);
  add_field(schema, "COMMAND", message::Table::Field::VARCHAR, 16);
  add_field(schema, "TIME", message::Table::Field::BIGINT);
  add_field(schema, "STATE", message::Table::Field::VARCHAR, 16);
  add_field(schema, "INFO", message::Table::Field::VARCHAR, PROCESS_LIST_WIDTH);
}

ProcesslistTool::Generator::Generator()
{
  now= time(NULL);

  pthread_mutex_lock(&LOCK_thread_count);
  it= getSessionList().begin();
}

ProcesslistTool::Generator::~Generator()
{
  pthread_mutex_unlock(&LOCK_thread_count);
}

bool ProcesslistTool::Generator::populate(Field ** fields)
{
  Field **field= fields;
  const char *val;
  Session* tmp;
  Security_context *tmp_sctx;
  size_t length;

  if (it == getSessionList().end())
    return false;

  tmp= *it;
  tmp_sctx= &tmp->security_ctx;

  /* ID */
  (*field)->store((int64_t) tmp->thread_id, true);
  field++;


  /* USER */
  val= tmp_sctx->user.c_str() ? tmp_sctx->user.c_str() : "unauthenticated user";
  (*field)->store(val, strlen(val), default_charset_info);
  field++;

  /* HOST */
  (*field)->store(tmp_sctx->ip.c_str(), strlen(tmp_sctx->ip.c_str()), default_charset_info);
  field++;

  /* DB */
  if (! tmp->db.empty())
  {
    (*field)->store(tmp->db.c_str(), tmp->db.length(), default_charset_info);
  }
  else
  {
    (*field)->store("<none selected>", sizeof("<none selected>"), default_charset_info);
  }
  field++;

  /* COMMAND */
  if ((val= (char *) (tmp->killed == Session::KILL_CONNECTION ? "Killed" : 0)))
    (*field)->store(val, strlen(val), default_charset_info);
  else
    (*field)->store(command_name[tmp->command].str,
                    command_name[tmp->command].length, default_charset_info);
  field++;

  /* DRIZZLE_TIME */
  (*field)->store((uint32_t)(tmp->start_time ?
                             now - tmp->start_time : 0), true);
  field++;

  /* STATE */
  val= (char*) (tmp->client->isWriting() ?
                "Writing to net" :
                tmp->client->isReading() ?
                (tmp->command == COM_SLEEP ?
                 NULL : "Reading from net") :
                tmp->get_proc_info() ? tmp->get_proc_info() :
                tmp->mysys_var &&
                tmp->mysys_var->current_cond ?
                "Waiting on cond" : NULL);
  if (val)
  {
    (*field)->store(val, strlen(val), default_charset_info);
  }
  else
  {
    (*field)->store("unknown", sizeof("unknown"), default_charset_info);
  }
  field++;

  /* INFO */
  length= strlen(tmp->process_list_info);
  if (length)
  {
    (*field)->store(tmp->process_list_info, length, default_charset_info);
  }
  else
  {
    (*field)->store(" ", sizeof(" "), default_charset_info);
  }

  it++;

  return true;
}
