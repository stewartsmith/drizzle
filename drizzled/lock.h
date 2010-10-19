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

#ifndef DRIZZLED_LOCK_H
#define DRIZZLED_LOCK_H

#include <vector>
#include "drizzled/thr_lock.h"

namespace drizzled
{

class Session;
class Table;
class TableList;

class DrizzleLock
{
  std::vector<Table *> table;
  std::vector<THR_LOCK_DATA *> locks;
public:
  uint32_t table_count;
  uint32_t lock_count;

  Table **getTable()
  {
    return &table[0];
  }

  THR_LOCK_DATA **getLocks()
  {
    return &locks[0];
  }

  size_t sizeLock()
  {
    return lock_count;
  }

  size_t sizeTable()
  {
    return table_count;
  }

  void resetLock()
  {
    lock_count= 0;
  }

  void setLock(size_t arg)
  {
    lock_count= arg;
  }

  void setTable(size_t arg)
  {
    table_count= arg;
  }

  void reset(void);
  void unlock(uint32_t count);

  DrizzleLock(size_t table_count_arg, size_t lock_count_arg) :
    table_count(table_count_arg),
    lock_count(lock_count_arg)
  {
    table.resize(table_count);
    locks.resize(lock_count);
  }

};

DrizzleLock *mysql_lock_tables(Session *session, Table **table, uint32_t count,
                               uint32_t flags, bool *need_reopen);
/* mysql_lock_tables() and open_table() flags bits */
#define DRIZZLE_LOCK_IGNORE_GLOBAL_READ_LOCK      0x0001
#define DRIZZLE_LOCK_IGNORE_FLUSH                 0x0002
#define DRIZZLE_LOCK_NOTIFY_IF_NEED_REOPEN        0x0004
#define DRIZZLE_OPEN_TEMPORARY_ONLY               0x0008

void mysql_unlock_tables(Session *session, DrizzleLock *sql_lock);
void mysql_unlock_read_tables(Session *session, DrizzleLock *sql_lock);
void mysql_unlock_some_tables(Session *session, Table **table, uint32_t count);
void mysql_lock_remove(Session *session, Table *table);
void mysql_lock_abort(Session *session, Table *table);
bool mysql_lock_abort_for_thread(Session *session, Table *table);
bool lock_global_read_lock(Session *session);
void unlock_global_read_lock(Session *session);
bool wait_if_global_read_lock(Session *session, bool abort_on_refresh,
                              bool is_not_commit);
void start_waiting_global_read_lock(Session *session);
bool make_global_read_lock_block_commit(Session *session);
void broadcast_refresh(void);

/* Lock based on name */
void unlock_table_name(TableList *table_list);
void unlock_table_names(TableList *table_list, TableList *last_table);
bool lock_table_names_exclusively(Session *session, TableList *table_list);

} /* namespace drizzled */

#endif /* DRIZZLED_LOCK_H */
