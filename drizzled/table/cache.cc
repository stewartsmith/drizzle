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
#include <drizzled/table/cache.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <drizzled/identifier.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/sys_var.h>
#include <drizzled/table.h>
#include <drizzled/table/concurrent.h>
#include <drizzled/table/unused.h>

namespace drizzled {
namespace table {

CacheMap Cache::cache;
boost::mutex Cache::_mutex;

CacheMap& getCache()
{
  return Cache::getCache();
}

/*
  Remove table from the open table cache

  SYNOPSIS
  free_cache_entry()
  entry		Table to remove

  NOTE
  We need to have a lock on table::Cache::mutex() when calling this
*/

static void free_cache_entry(table::Concurrent *table)
{
  table->intern_close_table();
  if (not table->in_use)
  {
    getUnused().unlink(table);
  }

  boost::checked_delete(table);
}

void remove_table(table::Concurrent *arg)
{
  CacheRange ppp;
  ppp= getCache().equal_range(arg->getShare()->getCacheKey());

  for (CacheMap::const_iterator iter= ppp.first;
         iter != ppp.second; ++iter)
  {
    table::Concurrent *found_table= iter->second;

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
    const identifier::Table::Key &key(table->getShare()->getCacheKey());

    table::CacheRange ppp= table::getCache().equal_range(key);

    for (table::CacheMap::const_iterator iter= ppp.first; iter != ppp.second; ++iter)
    {
      Table *search= iter->second;
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

/*
  Invalidate any cache entries that are for some DB

  SYNOPSIS
  removeSchema()
  db		Database name. This will be in lower case if
  lower_case_table_name is set

NOTE:
We can't use hash_delete when looping hash_elements. We mark them first
and afterwards delete those marked unused.
*/

void Cache::removeSchema(const identifier::Schema &schema_identifier)
{
  boost::mutex::scoped_lock scopedLock(_mutex);

  for (table::CacheMap::const_iterator iter= table::getCache().begin();
       iter != table::getCache().end();
       iter++)
  {
    table::Concurrent *table= iter->second;

    if (not schema_identifier.getPath().compare(table->getShare()->getSchemaName()))
    {
      table->getMutableShare()->resetVersion();			/* Free when thread is ready */
      if (not table->in_use)
        table::getUnused().relink(table);
    }
  }

  table::getUnused().cullByVersion();
}

/*
  Mark all entries with the table as deleted to force an reopen of the table

  The table will be closed (not stored in cache) by the current thread when
  close_thread_tables() is called.

  PREREQUISITES
  Lock on table::Cache::mutex()()

  RETURN
  0  This thread now have exclusive access to this table and no other thread
  can access the table until close_thread_tables() is called.
  1  Table is in use by another thread
*/

bool Cache::removeTable(Session& session, const identifier::Table &identifier, uint32_t flags)
{
  const identifier::Table::Key &key(identifier.getKey());
  bool result= false;
  bool signalled= false;

  for (;;)
  {
    result= signalled= false;

    table::CacheRange ppp;
    ppp= table::getCache().equal_range(key);

    for (table::CacheMap::const_iterator iter= ppp.first;
         iter != ppp.second; ++iter)
    {
      table::Concurrent *table= iter->second;
      Session *in_use;

      table->getMutableShare()->resetVersion();		/* Free when thread is ready */
      if (not (in_use= table->in_use))
      {
        table::getUnused().relink(table);
      }
      else if (in_use != &session)
      {
        /*
          Mark that table is going to be deleted from cache. This will
          force threads that are in lockTables() (but not yet
          in thr_multi_lock()) to abort it's locks, close all tables and retry
        */
        if (table->is_name_opened())
        {
          result= true;
        }
        /*
          Now we must abort all tables locks used by this thread
          as the thread may be waiting to get a lock for another table.
          Note that we need to hold table::Cache::mutex() while going through the
          list. So that the other thread cannot change it. The other
          thread must also hold table::Cache::mutex() whenever changing the
          open_tables list. Aborting the MERGE lock after a child was
          closed and before the parent is closed would be fatal.
        */
        for (Table *session_table= in_use->open_tables.open_tables_;
             session_table ;
             session_table= session_table->getNext())
        {
          /* Do not handle locks of MERGE children. */
          if (session_table->db_stat)	// If table is open
            signalled|= session.abortLockForThread(session_table);
        }
      }
      else
      {
        result= result || (flags & RTFC_OWNED_BY_Session_FLAG);
      }
    }

    table::getUnused().cullByVersion();

    /* Remove table from table definition cache if it's not in use */
    table::instance::release(identifier);

    if (result && (flags & RTFC_WAIT_OTHER_THREAD_FLAG))
    {
      /*
        Signal any thread waiting for tables to be freed to
        reopen their tables
      */
      locking::broadcast_refresh();
      if (not (flags & RTFC_CHECK_KILLED_FLAG) || not session.getKilled())
      {
        dropping_tables++;
        if (likely(signalled))
        {
          boost::mutex::scoped_lock scoped(table::Cache::mutex(), boost::adopt_lock_t());
          COND_refresh.wait(scoped);
          scoped.release();
        }
        else
        {
          /*
            It can happen that another thread has opened the
            table but has not yet locked any table at all. Since
            it can be locked waiting for a table that our thread
            has done LOCK Table x WRITE on previously, we need to
            ensure that the thread actually hears our signal
            before we go to sleep. Thus we wait for a short time
            and then we retry another loop in the
            table::Cache::removeTable routine.
          */
          boost::xtime xt;
          xtime_get(&xt, boost::TIME_UTC);
          xt.sec += 10;
          boost::mutex::scoped_lock scoped(table::Cache::mutex(), boost::adopt_lock_t());
          COND_refresh.timed_wait(scoped, xt);
          scoped.release();
        }
        dropping_tables--;
        continue;
      }
    }
    break;
  }

  return result;
}


void Cache::insert(table::Concurrent* arg)
{
  CacheMap::iterator returnable= cache.insert(std::make_pair(arg->getShare()->getCacheKey(), arg));
	assert(returnable != cache.end());
}

} /* namespace table */
} /* namespace drizzled */
