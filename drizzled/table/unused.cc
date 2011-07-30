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

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <drizzled/identifier.h>
#include <drizzled/sql_base.h>
#include <drizzled/set_var.h>
#include <drizzled/table/cache.h>
#include <drizzled/table/unused.h>

namespace drizzled {

extern uint64_t table_cache_size;

namespace table {

UnusedTables &getUnused(void)
{
  static UnusedTables unused_tables;

  return unused_tables;
}

void UnusedTables::cull()
{
  /* Free cache if too big */
  while (table::getCache().size() > table_cache_size && getTable())
    remove_table(getTable());
}

void UnusedTables::cullByVersion()
{
  while (getTable() && not getTable()->getShare()->getVersion())
    remove_table(getTable());
}

void UnusedTables::link(Concurrent *table)
{
  if (getTable())
  {
    table->setNext(getTable());		/* Link in last */
    table->setPrev(getTable()->getPrev());
    getTable()->setPrev(table);
    table->getPrev()->setNext(table);
  }
  else
  {
    table->setPrev(setTable(table));
    table->setNext(table->getPrev());
    assert(table->getNext() == table && table->getPrev() == table);
  }
}


void UnusedTables::unlink(Concurrent *table)
{
  table->unlink();

  /* Unlink the table from "unused_tables" list. */
  if (table == getTable())
  {  // First unused
    setTable(getTable()->getNext()); // Remove from link
    if (table == getTable())
      setTable(NULL);
  }
}

/* move table first in unused links */

void UnusedTables::relink(Concurrent *table)
{
  if (table != getTable())
  {
    table->unlink();

    table->setNext(getTable());			/* Link in unused tables */
    table->setPrev(getTable()->getPrev());
    getTable()->getPrev()->setNext(table);
    getTable()->setPrev(table);
    setTable(table);
  }
}

void UnusedTables::clear()
{
  while (getTable())
    remove_table(getTable());
}

} /* namespace table */
} /* namespace drizzled */
