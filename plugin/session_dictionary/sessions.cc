/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Sun Microsystems, Inc.
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

#include <config.h>

#include <plugin/session_dictionary/dictionary.h>

#include <netdb.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/thread_var.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/plugin/client.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/session/state.h>
#include <set>

using namespace std;
using namespace drizzled;

namespace session_dictionary {

Sessions::Sessions() :
  plugin::TableFunction("DATA_DICTIONARY", "SESSIONS")
{
  add_field("SESSION_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("SESSION_USERNAME", 16);
  add_field("SESSION_HOST", NI_MAXHOST);
  add_field("SESSION_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("SESSION_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("COMMAND", 16);
  add_field("STATE", plugin::TableFunction::STRING, 256, true);
  add_field("QUERY", plugin::TableFunction::STRING, PROCESS_LIST_WIDTH, true);
  add_field("HAS_GLOBAL_LOCK", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_INTERACTIVE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_CONSOLE", plugin::TableFunction::BOOLEAN, 0, false);
}

Sessions::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  session_generator(*getSession().user())
{
}

bool Sessions::Generator::populate()
{
  while (Session* tmp= session_generator)
  {
    boost::shared_ptr<session::State> state(tmp->state());
    identifier::user::ptr tmp_sctx= tmp->user();

    /* ID */
    push((int64_t) tmp->thread_id);

    /* USER */
    if (not tmp_sctx->username().empty())
      push(tmp_sctx->username());
    else 
      push(_("no user"));

    /* HOST */
    push(tmp_sctx->address());

    /* CATALOG */
    push(tmp->catalog().name());

    /* SCHEMA */
    util::string::ptr schema(tmp->schema());
    if (schema and not schema->empty())
    {
      push(*schema);
    }
    else
    {
      push();
    }

    /* COMMAND */
    if (tmp->getKilled() == Session::KILL_CONNECTION)
    {
      push("Killed");
    }
    else
    {
      push(getCommandName(tmp->command));
    }

    /* STATE */
    const char *step= tmp->get_proc_info();
    step ? push(step): push();

    /* QUERY */
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

    /* IS_INTERACTIVE */
    push(tmp->getClient()->isInteractive());

    /* IS_CONSOLE */
    push(tmp->getClient()->isConsole());

    return true;
  }

  return false;
}

} // namespace session_dictionary
