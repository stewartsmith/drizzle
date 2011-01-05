/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
  add_field("DB", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("COMMAND", 16);
  add_field("TIME", plugin::TableFunction::SIZE, 0, false);
  add_field("STATE", plugin::TableFunction::STRING, 256, true);
  add_field("INFO", plugin::TableFunction::STRING, PROCESS_LIST_WIDTH, true);
  add_field("HAS_GLOBAL_LOCK", plugin::TableFunction::BOOLEAN, 0, false);
}

ProcesslistTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  session_generator(*getSession().user())
{
}

ProcesslistTool::Generator::~Generator()
{
}

bool ProcesslistTool::Generator::populate()
{
  drizzled::Session::pointer tmp;

  while ((tmp= session_generator))
  {
    drizzled::Session::State::const_shared_ptr state(tmp->state());
    identifier::User::const_shared_ptr tmp_sctx= tmp->user();

    /* ID */
    push((int64_t) tmp->thread_id);

    /* USER */
    if (not tmp_sctx->username().empty())
      push(tmp_sctx->username());
    else 
      push(_("no user"));

    /* HOST */
    push(tmp_sctx->address());

    /* DB */
    drizzled::util::string::const_shared_ptr schema(tmp->schema());
    if (schema and not schema->empty())
    {
      push(*schema);
    }
    else
    {
      push();
    }

    /* COMMAND */
    const char *val= tmp->getKilled() == Session::KILL_CONNECTION ? "Killed" : NULL;
    if (val)
    {
      push(val);
    }
    else
    {
      push(command_name[tmp->command].str, command_name[tmp->command].length);
    }

    /* type::Time */
    boost::posix_time::time_duration duration_result;
    getSession().getTimeDifference(duration_result, getSession().start_timer());
    duration_result.is_negative() ? push(static_cast<uint64_t>(0)) : push(static_cast<uint64_t>(duration_result.total_seconds()));

    /* STATE */
    const char *step= tmp->get_proc_info();
    step ? push(step): push();

    /* INFO */
    if (state)
    {
      size_t length;
      const char *tmp_ptr= state->query(length);
      push(tmp_ptr, length);
    }
    else
    {
      push();
    }

    /* HAS_GLOBAL_LOCK */
    bool has_global_lock= tmp->isGlobalReadLock();
    push(has_global_lock);

    return true;
  }

  return false;
}
