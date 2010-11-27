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

#include "config.h"

#include "plugin/session_dictionary/dictionary.h"

#include <netdb.h>

#include "drizzled/pthread_globals.h"
#include "drizzled/plugin/client.h"
#include "drizzled/plugin/authorization.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/thread_var.h"

#include <set>

using namespace std;
using namespace drizzled;

ProcesslistTool::ProcesslistTool() :
  plugin::TableFunction("DATA_DICTIONARY", "PROCESSLIST")
{
  add_field("ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("USER", 16);
  add_field("HOST", NI_MAXHOST);
  add_field("DB");
  add_field("COMMAND", 16);
  add_field("TIME", plugin::TableFunction::NUMBER, 0, false);
  add_field("STATE", plugin::TableFunction::STRING, 256, true);
  add_field("INFO", plugin::TableFunction::STRING, PROCESS_LIST_WIDTH, true);
  add_field("HAS_GLOBAL_LOCK", plugin::TableFunction::BOOLEAN, PROCESS_LIST_WIDTH, false);
}

ProcesslistTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  now= time(NULL);
}

ProcesslistTool::Generator::~Generator()
{
}

bool ProcesslistTool::Generator::populate()
{
  drizzled::Session::shared_ptr tmp;

  while ((tmp= session_generator))
  {
    const SecurityContext *tmp_sctx= &tmp->getSecurityContext();
    const char *val;

    /* ID */
    push((int64_t) tmp->thread_id);


    /* USER */
    if (not tmp_sctx->getUser().empty())
      push(tmp_sctx->getUser());
    else 
      push(_("no user"));

    /* HOST */
    push(tmp_sctx->getIp());

    /* DB */
    if (! tmp->db.empty())
    {
      push(tmp->db);
    }
    else
    {
      push();
    }

    /* COMMAND */
    if ((val= const_cast<char *>(tmp->getKilled() == Session::KILL_CONNECTION ? "Killed" : 0)))
    {
      push(val);
    }
    else
    {
      push(command_name[tmp->command].str, command_name[tmp->command].length);
    }

    /* DRIZZLE_TIME */
    push(static_cast<uint64_t>(tmp->start_time ?  now - tmp->start_time : 0));

    /* STATE */
    val= (char*) (tmp->client->isWriting() ?
                  "Writing to net" :
                  tmp->client->isReading() ?
                  (tmp->command == COM_SLEEP ?
                   NULL : "Reading from net") :
                  tmp->get_proc_info() ? tmp->get_proc_info() :
                  tmp->getThreadVar() &&
                  tmp->getThreadVar()->current_cond ?
                  "Waiting on cond" : NULL);
    val ? push(val) : push();

    /* INFO */
    push(*tmp->getQueryString());

    /* HAS_GLOBAL_LOCK */
    push(static_cast<bool>(tmp->isGlobalReadLock()));

    return true;
  }

  return false;
}
