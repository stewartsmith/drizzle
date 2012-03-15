/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <vector>
#include <drizzled/thr_lock.h>
#include <drizzled/locking/global.h>

namespace drizzled {

class DrizzleLock
{
public:
  Table **getTable()
  {
    return &table[0];
  }

  THR_LOCK_DATA **getLocks()
  {
    return &locks[0];
  }

  size_t sizeTable() const
  {
    return table.size();
  }

  void resizeTable(size_t arg)
  {
    table.resize(arg);
  }

  size_t sizeLock() const
  {
    return lock_count;
  }

  void resetLock()
  {
    lock_count= 0;
  }

  void setLock(size_t arg)
  {
    lock_count= arg;
  }

  void reset(void);
  void unlock(uint32_t count);

  DrizzleLock(size_t table_count_arg)
  {
    table.resize(table_count_arg);
    lock_count= table_count_arg * 2;
    locks.resize(lock_count);
  }

private:
  uint32_t lock_count;

  std::vector<Table *> table;
  std::vector<THR_LOCK_DATA *> locks;
  std::vector<THR_LOCK_DATA *> copy_of;
};

/* lockTables() and open_table() flags bits */
#define DRIZZLE_LOCK_IGNORE_GLOBAL_READ_LOCK      0x0001
#define DRIZZLE_LOCK_IGNORE_FLUSH                 0x0002
#define DRIZZLE_LOCK_NOTIFY_IF_NEED_REOPEN        0x0004
#define DRIZZLE_OPEN_TEMPORARY_ONLY               0x0008

} /* namespace drizzled */

