/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include <drizzled/replication/mi.h>
#include CSTDINT_H
#include <drizzled/function/master_pos_wait.h>
#include <drizzled/session.h>

/**
  Wait until we are at or past the given position in the master binlog
  on the slave.
*/

int64_t Item_master_pos_wait::val_int()
{
  assert(fixed == 1);
  Session* session = current_session;
  String *log_name = args[0]->val_str(&value);
  int event_count= 0;

  null_value=0;
  if (session->slave_thread || !log_name || !log_name->length())
  {
    null_value = 1;
    return 0;
  }
  int64_t pos = (ulong)args[1]->val_int();
  int64_t timeout = (arg_count==3) ? args[2]->val_int() : 0 ;
  if ((event_count = active_mi->rli.wait_for_pos(session, log_name, pos, timeout)) == -2)
  {
    null_value = 1;
    event_count=0;
  }
  return event_count;
}

