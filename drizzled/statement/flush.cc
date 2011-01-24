/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/lock.h>
#include <drizzled/statement/flush.h>
#include "drizzled/sql_table.h"
#include "drizzled/plugin/logging.h"

namespace drizzled
{

bool statement::Flush::execute()
{
  /*
   * reloadCache() will tell us if we are allowed to write to the
   * binlog or not.
   */
  if (not reloadCache())
  {
    /*
     * We WANT to write and we CAN write.
     * ! we write after unlocking the table.
     *
     * Presumably, RESET and binlog writing doesn't require synchronization
     */
    write_bin_log(getSession(), *getSession()->getQueryString());
    getSession()->my_ok();
  }

  return false;
}

bool statement::Flush::reloadCache()
{
  bool result= false;
  TableList *tables= (TableList *) getSession()->lex->select_lex.table_list.first;

  if (flush_log)
  {
    if (plugin::StorageEngine::flushLogs(NULL))
    {
      result= true;
    }
  }
  /*
    Note that if REFRESH_READ_LOCK bit is set then REFRESH_TABLES is set too
    (see sql_yacc.yy)
  */
  if (flush_tables || flush_tables_with_read_lock)
  {
    if (getSession() && flush_tables_with_read_lock)
    {
      if (getSession()->lockGlobalReadLock())
      {
        return true; /* Killed */
      }
      result= getSession()->close_cached_tables(tables, true, true);

      if (getSession()->makeGlobalReadLockBlockCommit()) /* Killed */
      {
        /* Don't leave things in a half-locked state */
        getSession()->unlockGlobalReadLock();
        return true;
      }
    }
    else
    {
      result= getSession()->close_cached_tables(tables, true, false);
    }
  }

  if (getSession() && flush_status)
  {
    getSession()->refresh_status();
  }

  if (getSession() && flush_global_status)
  {
    memset(&current_global_counters, 0, sizeof(current_global_counters));
    plugin::Logging::resetStats(getSession());
    getSession()->refresh_status();
  }

  return result;
}

}
