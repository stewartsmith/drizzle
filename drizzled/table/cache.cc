/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "drizzled/identifier/table.h"
#include "drizzled/table.h"
#include "drizzled/table/concurrent.h"

#include "drizzled/table/cache.h"
#include "drizzled/table/unused.h"

namespace drizzled
{
namespace table
{

CacheMap &getCache(void)
{
  return Cache::singleton().getCache();
}

/*
  Remove table from the open table cache

  SYNOPSIS
  free_cache_entry()
  entry		Table to remove

  NOTE
  We need to have a lock on LOCK_open when calling this
*/

static void free_cache_entry(table::Concurrent *table)
{
  table->intern_close_table();
  if (not table->in_use)
  {
    getUnused().unlink(table);
  }

  delete table;
}

void remove_table(table::Concurrent *arg)
{
  CacheRange ppp;
  ppp= getCache().equal_range(arg->getShare()->getCacheKey());

  for (CacheMap::const_iterator iter= ppp.first;
         iter != ppp.second; ++iter)
  {
    table::Concurrent *found_table= (*iter).second;

    if (found_table == arg)
    {
      free_cache_entry(arg);
      getCache().erase(iter);
      return;
    }
  }
}

/*
  Wait until all threads has closed the tables in the list
  We have also to wait if there is thread that has a lock on this table even
  if the table is closed
*/

bool Cache::areTablesUsed(Table *table, bool wait_for_name_lock)
{
  do
  {
    const TableIdentifier::Key &key(table->getShare()->getCacheKey());

    table::CacheRange ppp= table::getCache().equal_range(key);

    for (table::CacheMap::const_iterator iter= ppp.first; iter != ppp.second; ++iter)
    {
      Table *search= (*iter).second;
      if (search->in_use == table->in_use)
        continue;                               // Name locked by this thread
      /*
        We can't use the table under any of the following conditions:
        - There is an name lock on it (Table is to be deleted or altered)
        - If we are in flush table and we didn't execute the flush
        - If the table engine is open and it's an old version
        (We must wait until all engines are shut down to use the table)
      */
      if ( (search->locked_by_name && wait_for_name_lock) ||
           (search->is_name_opened() && search->needs_reopen_or_name_lock()))
        return 1;
    }
  } while ((table=table->getNext()));
  return 0;
}

} /* namespace table */
} /* namespace drizzled */
