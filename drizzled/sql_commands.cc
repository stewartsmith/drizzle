/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <drizzled/server_includes.h>
#include <mysys/hash.h>
#include <drizzled/logging.h>
#include <drizzled/db.h>
#include <drizzled/error.h>
#include <drizzled/nested_join.h>
#include <drizzled/query_id.h>
#include <drizzled/sql_parse.h>
#include <drizzled/data_home.h>
#include <drizzled/sql_base.h>
#include <drizzled/show.h>
#include <drizzled/info_schema.h>
#include <drizzled/rename.h>
#include <drizzled/function/time/unix_timestamp.h>
#include <drizzled/function/get_system_var.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/null.h>
#include <drizzled/session.h>
#include <drizzled/sql_load.h>
#include <drizzled/connect.h>
#include <drizzled/lock.h>
#include <drizzled/select_send.h>
#include <bitset>
#include <algorithm>

using namespace std;
using namespace drizzled;

int SqlCommand::execute()
{
  session->status_var.last_query_cost= 0.0;
  int res= execute_sqlcom_select(session, all_tables);

  return res;
}

int ShowStatusCommand::execute()
{
  system_status_var old_status_var= session->status_var;
  session->initial_status_var= &old_status_var;
  int res= execute_sqlcom_select(session, all_tables);
  /* Don't log SHOW STATUS commands to slow query log */
  session->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                             SERVER_QUERY_NO_GOOD_INDEX_USED);
  /*
   * Restore status variables, as we don't want 'SHOW STATUS' to
   * cause changes.
   */
  pthread_mutex_lock(show_lock);
  add_diff_to_status(&global_status_var,
                     &session->status_var,
                     &old_status_var);
  session->status_var= old_status_var;
  pthread_mutex_unlock(show_lock);

  return res;
}
