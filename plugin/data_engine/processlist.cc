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

#include <plugin/data_engine/processlist.h>

#include <netdb.h>

#include "drizzled/pthread_globals.h"
#include "drizzled/session.h"
#include "drizzled/session_list.h"
#include "drizzled/plugin/client.h"
#include "drizzled/internal/my_sys.h"

using namespace std;
using namespace drizzled;

ProcesslistTool::ProcesslistTool() :
  plugin::TableFunction("DATA_DICTIONARY", "PROCESSLIST")
{
  add_field("ID", plugin::TableFunction::NUMBER);
  add_field("USER", 16);
  add_field("HOST", NI_MAXHOST);
  add_field("DB");
  add_field("COMMAND", 16);
  add_field("TIME", plugin::TableFunction::NUMBER);
  add_field("STATE");
  add_field("INFO", PROCESS_LIST_WIDTH);
}

ProcesslistTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  now= time(NULL);

  pthread_mutex_lock(&LOCK_thread_count);
  it= getSessionList().begin();
}

ProcesslistTool::Generator::~Generator()
{
  pthread_mutex_unlock(&LOCK_thread_count);
}

bool ProcesslistTool::Generator::populate()
{
  const char *val;
  Session* tmp;
  Security_context *tmp_sctx;

  if (it == getSessionList().end())
    return false;

  tmp= *it;
  tmp_sctx= &tmp->security_ctx;

  /* ID */
  push((int64_t) tmp->thread_id);


  /* USER */
  val= tmp_sctx->user.c_str() ? tmp_sctx->user.c_str() : "unauthenticated user";
  push(val);

  /* HOST */
  push(tmp_sctx->ip.c_str());

  /* DB */
  if (! tmp->db.empty())
  {
    push(tmp->db.c_str());
  }
  else
  {
    push("<none selected>");
  }

  /* COMMAND */
  if ((val= const_cast<char *>(tmp->killed == Session::KILL_CONNECTION ? "Killed" : 0)))
  {
    push(val);
  }
  else
  {
    push(command_name[tmp->command].str, command_name[tmp->command].length);
  }

  /* DRIZZLE_TIME */
  push((uint32_t)(tmp->start_time ?  now - tmp->start_time : 0));

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
  push(val ? val : "unknown");

  /* INFO */
  size_t length= strlen(tmp->process_list_info);
  if (length)
  {
    push(tmp->process_list_info, length);
  }
  else
  {
    push("");
  }

  it++;

  return true;
}
