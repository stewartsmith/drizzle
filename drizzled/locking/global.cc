/* 
    Copyright (C) 2011 Brian Aker
    Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


/**
  @file

  @note this is out of date, just for historical reference 

  Locking functions for drizzled.

  Because of the new concurrent inserts, we must first get external locks
  before getting internal locks.  If we do it in the other order, the status
  information is not up to date when called from the lock handler.

  GENERAL DESCRIPTION OF LOCKING

  When not using LOCK TABLES:

  - For each SQL statement lockTables() is called for all involved
    tables.
    - lockTables() will call
      cursor->external_lock(session,locktype) for each table.
      This is followed by a call to thr_multi_lock() for all tables.

  - When statement is done, we call unlockTables().
    This will call DrizzleLock::unlock() followed by
    table_handler->external_lock(session, F_UNLCK) for each table.

  - Note that unlockTables() may be called several times as
    MySQL in some cases can free some tables earlier than others.

  - The above is true both for normal and temporary tables.

  - Temporary non transactional tables are never passed to thr_multi_lock()
    and we never call external_lock(session, F_UNLOCK) on these.

  When using LOCK TABLES:

  - LOCK Table will call lockTables() for all tables.
    lockTables() will call
    table_handler->external_lock(session,locktype) for each table.
    This is followed by a call to thr_multi_lock() for all tables.

  - For each statement, we will call table_handler->start_stmt(Session)
    to inform the table handler that we are using the table.

    The tables used can only be tables used in LOCK TABLES or a
    temporary table.

  - When statement is done, we will call ha_commit_stmt(session);

  - When calling UNLOCK TABLES we call unlockTables() for all
    tables used in LOCK TABLES

  If table_handler->external_lock(session, locktype) fails, we call
  table_handler->external_lock(session, F_UNLCK) for each table that was locked,
  excluding one that caused failure. That means handler must cleanup itself
  in case external_lock() fails.

  @todo
  Change to use malloc() ONLY when using LOCK TABLES command or when
  we are forced to use mysql_lock_merge.
*/
#include <config.h>

#include <fcntl.h>

#include <drizzled/error.h>
#include <drizzled/thr_lock.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/util/test.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/table/cache.h>

#include <set>
#include <vector>
#include <algorithm>
#include <functional>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/condition_variable.hpp>

using namespace std;

namespace drizzled
{

static boost::mutex LOCK_global_read_lock;
static boost::condition_variable_any COND_global_read_lock;

/**
  @defgroup Locking Locking
  @{
*/

static void print_lock_error(int error, const char *);

/*
  Lock tables.

  SYNOPSIS
    lockTables()
    tables                      An array of pointers to the tables to lock.
    count                       The number of tables to lock.
    flags                       Options:
      DRIZZLE_LOCK_IGNORE_GLOBAL_READ_LOCK      Ignore a global read lock
      DRIZZLE_LOCK_IGNORE_FLUSH                 Ignore a flush tables.
      DRIZZLE_LOCK_NOTIFY_IF_NEED_REOPEN        Instead of reopening altered
                                              or dropped tables by itself,
                                              lockTables() should
                                              notify upper level and rely
                                              on caller doing this.

  RETURN
    A lock structure pointer on success.
    NULL on error or if some tables should be reopen.
*/

/* Map the return value of thr_lock to an error from errmsg.txt */
static drizzled::error_t thr_lock_errno_to_mysql[]=
{ EE_OK, EE_ERROR_FIRST, ER_LOCK_WAIT_TIMEOUT, ER_LOCK_DEADLOCK };


/**
  Reset lock type in lock data and free.

  @param mysql_lock Lock structures to reset.

  @note After a locking error we want to quit the locking of the table(s).
        The test case in the bug report for Bug #18544 has the following
        cases: 1. Locking error in lock_external() due to InnoDB timeout.
        2. Locking error in get_lock_data() due to missing write permission.
        3. Locking error in wait_if_global_read_lock() due to lock conflict.

  @note In all these cases we have already set the lock type into the lock
        data of the open table(s). If the table(s) are in the open table
        cache, they could be reused with the non-zero lock type set. This
        could lead to ignoring a different lock type with the next lock.

  @note Clear the lock type of all lock data. This ensures that the next
        lock request will set its lock type properly.
*/

static void reset_lock_data_and_free(DrizzleLock*& lock)
{
  lock->reset();
  delete lock;
  lock= NULL;
}

DrizzleLock *Session::lockTables(Table **tables, uint32_t count, uint32_t flags)
{
  DrizzleLock *sql_lock;
  Table *write_lock_used;
  vector<plugin::StorageEngine *> involved_engines;

  do
  {
    if (! (sql_lock= get_lock_data(tables, count, true, &write_lock_used)))
      break;

    if (global_read_lock && write_lock_used and (not (flags & DRIZZLE_LOCK_IGNORE_GLOBAL_READ_LOCK)))
    {
      /*
	Someone has issued LOCK ALL TABLES FOR READ and we want a write lock
	Wait until the lock is gone
      */
      if (wait_if_global_read_lock(1, 1))
      {
        /* Clear the lock type of all lock data to avoid reusage. */
        reset_lock_data_and_free(sql_lock);
	break;
      }

      if (open_tables.version != g_refresh_version)
      {
        /* Clear the lock type of all lock data to avoid reusage. */
        reset_lock_data_and_free(sql_lock);
	break;
      }
    }
    
    set_proc_info("Notify start statement");
    /*
     * Here, we advise all storage engines involved in the
     * statement that we are starting a new statement
     */
    if (sql_lock->sizeTable())
    {
      size_t num_tables= sql_lock->sizeTable();
      plugin::StorageEngine *engine;
      std::set<size_t> involved_slots;

      for (size_t x= 1; x <= num_tables; x++, tables++)
      {
        engine= (*tables)->cursor->getEngine();

        if (involved_slots.count(engine->getId()) > 0)
          continue; /* already added to involved engines */

        involved_engines.push_back(engine);
        involved_slots.insert(engine->getId());
      }

      for_each(involved_engines.begin(),
               involved_engines.end(),
               bind2nd(mem_fun(&plugin::StorageEngine::startStatement), this));
    }

    set_proc_info("External lock");
    /*
     * Here, the call to lock_external() informs the
     * all engines for all tables used in this statement
     * of the type of lock that Drizzle intends to take on a 
     * specific table.
     */
    if (sql_lock->sizeTable() && lock_external(sql_lock->getTable(), sql_lock->sizeTable()))
    {
      /* Clear the lock type of all lock data to avoid reusage. */
      reset_lock_data_and_free(sql_lock);
      break;
    }
    set_proc_info("Table lock");
    /* Copy the lock data array. thr_multi_lock() reorders its contens. */
    memcpy(sql_lock->getLocks() + sql_lock->sizeLock(),
           sql_lock->getLocks(),
           sql_lock->sizeLock() * sizeof(*sql_lock->getLocks()));

    /* Lock on the copied half of the lock data array. */
    drizzled::error_t rc;
    rc= thr_lock_errno_to_mysql[(int) thr_multi_lock(*this,
                                                     sql_lock->getLocks() +
                                                     sql_lock->sizeLock(),
                                                     sql_lock->sizeLock(),
                                                     this->lock_id)];
    if (rc)                                 /* a timeout or a deadlock */
    {
      if (sql_lock->sizeTable())
        unlock_external(sql_lock->getTable(), sql_lock->sizeTable());
      reset_lock_data_and_free(sql_lock);
      my_error(rc, MYF(0));
    }
  } while(0);

  set_proc_info(0);
  if (getKilled())
  {
    send_kill_message();
    if (sql_lock)
    {
      unlockTables(sql_lock);
      sql_lock= NULL;
    }
  }

  times.set_time_after_lock();

  return sql_lock;
}


int Session::lock_external(Table **tables, uint32_t count)
{
  int lock_type,error;
  for (uint32_t i= 1 ; i <= count ; i++, tables++)
  {
    assert((*tables)->reginfo.lock_type >= TL_READ);
    lock_type=F_WRLCK;				/* Lock exclusive */
    if ((*tables)->db_stat & HA_READ_ONLY ||
      ((*tables)->reginfo.lock_type >= TL_READ &&
      (*tables)->reginfo.lock_type <= TL_READ_NO_INSERT))
      lock_type=F_RDLCK;

    if ((error=(*tables)->cursor->ha_external_lock(this,lock_type)))
    {
      print_lock_error(error, (*tables)->cursor->getEngine()->getName().c_str());
      while (--i)
      {
        tables--;
        (*tables)->cursor->ha_external_lock(this, F_UNLCK);
        (*tables)->current_lock=F_UNLCK;
      }
      return error;
    }
    else
    {
      (*tables)->db_stat &= ~ HA_BLOCK_LOCK;
      (*tables)->current_lock= lock_type;
    }
  }
  return 0;
}


void Session::unlockTables(DrizzleLock *sql_lock)
{
  if (sql_lock->sizeLock())
    sql_lock->unlock(sql_lock->sizeLock());
  if (sql_lock->sizeTable())
    unlock_external(sql_lock->getTable(), sql_lock->sizeTable());
  delete sql_lock;
}

/**
  Unlock some of the tables locked by lockTables.

  This will work even if get_lock_data fails (next unlock will free all)
*/

void Session::unlockSomeTables(Table **table, uint32_t count)
{
  DrizzleLock *sql_lock;
  Table *write_lock_used;
  if ((sql_lock= get_lock_data(table, count, false,
                               &write_lock_used)))
    unlockTables(sql_lock);
}


/**
  unlock all tables locked for read.
*/

void Session::unlockReadTables(DrizzleLock *sql_lock)
{
  uint32_t i,found;

  /* Move all write locks first */
  THR_LOCK_DATA **lock_local= sql_lock->getLocks();
  for (i=found=0 ; i < sql_lock->sizeLock(); i++)
  {
    if (sql_lock->getLocks()[i]->type >= TL_WRITE_ALLOW_READ)
    {
      std::swap(*lock_local, sql_lock->getLocks()[i]);
      lock_local++;
      found++;
    }
  }
  /* unlock the read locked tables */
  if (i != found)
  {
    thr_multi_unlock(lock_local, i - found);
    sql_lock->setLock(found);
  }

  /* Then do the same for the external locks */
  /* Move all write locked tables first */
  Table **table= sql_lock->getTable();
  for (i=found=0 ; i < sql_lock->sizeTable() ; i++)
  {
    assert(sql_lock->getTable()[i]->lock_position == i);
    if ((uint32_t) sql_lock->getTable()[i]->reginfo.lock_type >= TL_WRITE_ALLOW_READ)
    {
      std::swap(*table, sql_lock->getTable()[i]);
      table++;
      found++;
    }
  }
  /* Unlock all read locked tables */
  if (i != found)
  {
    unlock_external(table, i - found);
    sql_lock->resizeTable(found);
  }
  /* Fix the lock positions in Table */
  table= sql_lock->getTable();
  found= 0;
  for (i= 0; i < sql_lock->sizeTable(); i++)
  {
    Table *tbl= *table;
    tbl->lock_position= table - sql_lock->getTable();
    tbl->lock_data_start= found;
    found+= tbl->lock_count;
    table++;
  }
}


/**
  Try to find the table in the list of locked tables.
  In case of success, unlock the table and remove it from this list.

  @note This function has a legacy side effect: the table is
  unlocked even if it is not found in the locked list.
  It's not clear if this side effect is intentional or still
  desirable. It might lead to unmatched calls to
  unlock_external(). Moreover, a discrepancy can be left
  unnoticed by the storage engine, because in
  unlock_external() we call handler::external_lock(F_UNLCK) only
  if table->current_lock is not F_UNLCK.

  @param  session             thread context
  @param  locked          list of locked tables
  @param  table           the table to unlock
  @param  always_unlock   specify explicitly if the legacy side
                          effect is desired.
*/

void Session::removeLock(Table *table)
{
  unlockSomeTables(&table, /* table count */ 1);
}


/** Abort all other threads waiting to get lock in table. */

void Session::abortLock(Table *table)
{
  DrizzleLock *locked;
  Table *write_lock_used;

  if ((locked= get_lock_data(&table, 1, false,
                             &write_lock_used)))
  {
    for (uint32_t x= 0; x < locked->sizeLock(); x++)
      locked->getLocks()[x]->lock->abort_locks();
    delete locked;
  }
}


/**
  Abort one thread / table combination.

  @param session	   Thread handler
  @param table	   Table that should be removed from lock queue

  @retval
    0  Table was not locked by another thread
  @retval
    1  Table was locked by at least one other thread
*/

bool Session::abortLockForThread(Table *table)
{
  bool result= false;
  Table* write_lock_used;
  if (DrizzleLock* locked= get_lock_data(&table, 1, false, &write_lock_used))
  {
    for (uint32_t i= 0; i < locked->sizeLock(); i++)
    {
      if (locked->getLocks()[i]->lock->abort_locks_for_thread(table->in_use->thread_id))
        result= true;
    }
    delete locked;
  }
  return result;
}

/** Unlock a set of external. */

int Session::unlock_external(Table **table, uint32_t count)
{
  int error;

  int error_code=0;
  do
  {
    if ((*table)->current_lock != F_UNLCK)
    {
      (*table)->current_lock = F_UNLCK;
      if ((error=(*table)->cursor->ha_external_lock(this, F_UNLCK)))
      {
	error_code=error;
	print_lock_error(error_code, (*table)->cursor->getEngine()->getName().c_str());
      }
    }
    table++;
  } while (--count);
  return error_code;
}


/**
  Get lock structures from table structs and initialize locks.

  @param session		    Thread handler
  @param table_ptr	    Pointer to tables that should be locks
  @param should_lock		    One of:
           - false      : If we should send TL_IGNORE to store lock
           - true       : Store lock info in Table
  @param write_lock_used   Store pointer to last table with WRITE_ALLOW_WRITE
*/

DrizzleLock *Session::get_lock_data(Table **table_ptr, uint32_t count,
                                    bool should_lock, Table **write_lock_used)
{
  uint32_t lock_count;
  THR_LOCK_DATA **locks, **locks_buf, **locks_start;
  Table **to, **table_buf;

  *write_lock_used=0;
  for (uint32_t i= lock_count= 0 ; i < count ; i++)
  {
    Table *t= table_ptr[i];

    if (! (t->getEngine()->check_flag(HTON_BIT_SKIP_STORE_LOCK)))
    {
      lock_count++;
    }
  }

  /*
    Allocating twice the number of pointers for lock data for use in
    thr_mulit_lock(). This function reorders the lock data, but cannot
    update the table values. So the second part of the array is copied
    from the first part immediately before calling thr_multi_lock().
  */
  DrizzleLock *sql_lock= new DrizzleLock(lock_count);

  if (not sql_lock)
    return NULL;

  locks= locks_buf= sql_lock->getLocks();
  to= table_buf= sql_lock->getTable();

  for (uint32_t i= 0; i < count ; i++)
  {
    Table *table;
    thr_lock_type lock_type;

    if (table_ptr[i]->getEngine()->check_flag(HTON_BIT_SKIP_STORE_LOCK))
      continue;

    table= table_ptr[i];
    lock_type= table->reginfo.lock_type;
    assert (lock_type != TL_WRITE_DEFAULT);
    if (lock_type >= TL_WRITE_ALLOW_WRITE)
    {
      *write_lock_used=table;
      if (table->db_stat & HA_READ_ONLY)
      {
	my_error(ER_OPEN_AS_READONLY, MYF(0), table->getAlias());
        /* Clear the lock type of the lock data that are stored already. */
        sql_lock->setLock(locks - sql_lock->getLocks());
        reset_lock_data_and_free(sql_lock);
	return NULL;
      }
    }
    locks_start= locks;
    locks= table->cursor->store_lock(this, locks, should_lock ? lock_type : TL_IGNORE);
    if (should_lock)
    {
      table->lock_position=   (uint32_t) (to - table_buf);
      table->lock_data_start= (uint32_t) (locks_start - locks_buf);
      table->lock_count=      (uint32_t) (locks - locks_start);
      assert(table->lock_count == 1);
    }
    *to++= table;
  }
  /*
    We do not use 'tables', because there are cases where store_lock()
    returns less locks than lock_count() claimed. This can happen when
    a FLUSH TABLES tries to abort locks from a MERGE table of another
    thread. When that thread has just opened the table, but not yet
    attached its children, it cannot return the locks. lock_count()
    always returns the number of locks that an attached table has.
    This is done to avoid the reverse situation: If lock_count() would
    return 0 for a non-attached MERGE table, and that table becomes
    attached between the calls to lock_count() and store_lock(), then
    we would have allocated too little memory for the lock data. Now
    we may allocate too much, but better safe than memory overrun.
    And in the FLUSH case, the memory is released quickly anyway.
  */
  sql_lock->setLock(locks - locks_buf);

  return sql_lock;
}


/**
  Put a not open table with an old refresh version in the table cache.

  @param table_list		Lock first table in this list
  @param check_in_use           Do we need to check if table already in use by us

  @note
    One must have a lock on table::Cache::mutex()!

  @warning
    If you are going to update the table, you should use
    lock_and_wait_for_table_name(removed) instead of this function as this works
    together with 'FLUSH TABLES WITH READ LOCK'

  @note
    This will force any other threads that uses the table to release it
    as soon as possible.

  @return
    < 0 error
  @return
    == 0 table locked
  @return
    > 0  table locked, but someone is using it
*/

int Session::lock_table_name(TableList *table_list)
{
  identifier::Table identifier(table_list->getSchemaName(), table_list->getTableName());
  {
    /* Only insert the table if we haven't insert it already */
    table::CacheRange ppp= table::getCache().equal_range(identifier.getKey());
    for (table::CacheMap::const_iterator iter= ppp.first; iter != ppp.second; ++iter)
    {
      Table *table= iter->second;
      if (table->reginfo.lock_type < TL_WRITE)
        continue;
      if (table->in_use == this)
      {
        table->getMutableShare()->resetVersion();                  // Ensure no one can use this
        table->locked_by_name= true;
        return 0;
      }
    }
  }

  table::Placeholder *table= &table_cache_insert_placeholder(identifier);
  table_list->table= reinterpret_cast<Table*>(table);

  /* Return 1 if table is in use */
  return (test(table::Cache::removeTable(*this, identifier, RTFC_NO_FLAG)));
}


void TableList::unlock_table_name()
{
  if (table)
  {
    table::remove_table(static_cast<table::Concurrent *>(table));
    locking::broadcast_refresh();
  }
}


static bool locked_named_table(TableList *table_list)
{
  for (; table_list; table_list=table_list->next_local)
  {
    Table *table= table_list->table;
    if (table)
    {
      Table *save_next= table->getNext();
      table->setNext(NULL);
      bool result= table::Cache::areTablesUsed(table_list->table, 0);
      table->setNext(save_next);
      if (result)
        return 1;
    }
  }
  return 0;					// All tables are locked
}


bool Session::wait_for_locked_table_names(TableList *table_list)
{
  bool result= false;

#if 0
  assert(ownership of table::Cache::mutex());
#endif

  while (locked_named_table(table_list))
  {
    if (getKilled())
    {
      result= true;
      break;
    }
    wait_for_condition(table::Cache::mutex(), COND_refresh);
    table::Cache::mutex().lock(); /* Wait for a table to unlock and then lock it */
  }
  return result;
}


/**
  Lock all tables in list with a name lock.

  REQUIREMENTS
  - One must have a lock on table::Cache::mutex() when calling this

  @param table_list		Names of tables to lock

  @retval
    0	ok
  @retval
    1	Fatal error (end of memory ?)
*/

bool Session::lock_table_names(TableList *table_list)
{
  bool got_all_locks= true;
  for (TableList* lock_table= table_list; lock_table; lock_table= lock_table->next_local)
  {
    int got_lock= lock_table_name(lock_table);
    if (got_lock < 0)
    {
      table_list->unlock_table_names(table_list);
      return true; // Fatal error
    }
    if (got_lock)
      got_all_locks= false;				// Someone is using table
  }

  /* If some table was in use, wait until we got the lock */
  if (not got_all_locks && wait_for_locked_table_names(table_list))
  {
    table_list->unlock_table_names(table_list);
    return true;
  }
  return false;
}


/**
  Unlock all tables in list with a name lock.

  @param table_list Names of tables to lock.

  @note
    This function needs to be protected by table::Cache::mutex(). If we're
    under LOCK TABLES, this function does not work as advertised. Namely,
    it does not exclude other threads from using this table and does not
    put an exclusive name lock on this table into the table cache.

  @see lock_table_names
  @see unlock_table_names

  @retval TRUE An error occured.
  @retval FALSE Name lock successfully acquired.
*/

bool Session::lock_table_names_exclusively(TableList *table_list)
{
  if (lock_table_names(table_list))
    return true;

  /*
    Upgrade the table name locks from semi-exclusive to exclusive locks.
  */
  for (TableList *table= table_list; table; table= table->next_global)
  {
    if (table->table)
      table->table->open_placeholder= 1;
  }
  return false;
}


/**
  Unlock all tables in list with a name lock.

  @param
    table_list		Names of tables to unlock
  @param
    last_table		Don't unlock any tables after this one.
			        (default 0, which will unlock all tables)

  @note
    One must have a lock on table::Cache::mutex() when calling this.

  @note
    This function will broadcast refresh signals to inform other threads
    that the name locks are removed.

  @retval
    0	ok
  @retval
    1	Fatal error (end of memory ?)
*/

void TableList::unlock_table_names(TableList *last_table)
{
  for (TableList *table_iter= this; table_iter != last_table; table_iter= table_iter->next_local)
  {
    table_iter->unlock_table_name();
  }
  locking::broadcast_refresh();
}


static void print_lock_error(int error, const char *table)
{
  drizzled::error_t textno;
  switch (error) 
  {
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  case HA_ERR_LOCK_DEADLOCK:
    textno=ER_LOCK_DEADLOCK;
    break;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  default:
    textno=ER_CANT_LOCK;
    break;
  }

  if ( textno == ER_ILLEGAL_HA )
    my_error(textno, MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG), table);
  else
    my_error(textno, MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG), error);
}


/****************************************************************************
  Handling of global read locks

  Taking the global read lock is TWO steps (2nd step is optional; without
  it, COMMIT of existing transactions will be allowed):
  lock_global_read_lock() THEN make_global_read_lock_block_commit().

  The global locks are handled through the global variables:
  global_read_lock
    count of threads which have the global read lock (i.e. have completed at
    least the first step above)
  global_read_lock_blocks_commit
    count of threads which have the global read lock and block
    commits (i.e. are in or have completed the second step above)
  waiting_for_read_lock
    count of threads which want to take a global read lock but cannot
  protect_against_global_read_lock
    count of threads which have set protection against global read lock.

  access to them is protected with a mutex LOCK_global_read_lock

  (XXX: one should never take table::Cache::mutex() if LOCK_global_read_lock is
  taken, otherwise a deadlock may occur. Other mutexes could be a
  problem too - grep the code for global_read_lock if you want to use
  any other mutex here) Also one must not hold table::Cache::mutex() when calling
  wait_if_global_read_lock(). When the thread with the global read lock
  tries to close its tables, it needs to take table::Cache::mutex() in
  close_thread_table().

  How blocking of threads by global read lock is achieved: that's
  advisory. Any piece of code which should be blocked by global read lock must
  be designed like this:
  - call to wait_if_global_read_lock(). When this returns 0, no global read
  lock is owned; if argument abort_on_refresh was 0, none can be obtained.
  - job
  - if abort_on_refresh was 0, call to session->startWaitingGlobalReadLock() to
  allow other threads to get the global read lock. I.e. removal of the
  protection.
  (Note: it's a bit like an implementation of rwlock).

  [ I am sorry to mention some SQL syntaxes below I know I shouldn't but found
  no better descriptive way ]

  Why does FLUSH TABLES WITH READ LOCK need to block COMMIT: because it's used
  to read a non-moving SHOW MASTER STATUS, and a COMMIT writes to the binary
  log.

  Why getting the global read lock is two steps and not one. Because FLUSH
  TABLES WITH READ LOCK needs to insert one other step between the two:
  flushing tables. So the order is
  1) lock_global_read_lock() (prevents any new table write locks, i.e. stalls
  all new updates)
  2) close_cached_tables() (the FLUSH TABLES), which will wait for tables
  currently opened and being updated to close (so it's possible that there is
  a moment where all new updates of server are stalled *and* FLUSH TABLES WITH
  READ LOCK is, too).
  3) session::makeGlobalReadLockBlockCommit().
  If we have merged 1) and 3) into 1), we would have had this deadlock:
  imagine thread 1 and 2, in non-autocommit mode, thread 3, and an InnoDB
  table t.
  session1: SELECT * FROM t FOR UPDATE;
  session2: UPDATE t SET a=1; # blocked by row-level locks of session1
  session3: FLUSH TABLES WITH READ LOCK; # blocked in close_cached_tables() by the
  table instance of session2
  session1: COMMIT; # blocked by session3.
  session1 blocks session2 which blocks session3 which blocks session1: deadlock.

  Note that we need to support that one thread does
  FLUSH TABLES WITH READ LOCK; and then COMMIT;
  (that's what innobackup does, for some good reason).
  So in this exceptional case the COMMIT should not be blocked by the FLUSH
  TABLES WITH READ LOCK.

****************************************************************************/

volatile uint32_t global_read_lock=0;
volatile uint32_t global_read_lock_blocks_commit=0;
static volatile uint32_t protect_against_global_read_lock=0;
static volatile uint32_t waiting_for_read_lock=0;

bool Session::lockGlobalReadLock()
{
  if (isGlobalReadLock() == Session::NONE)
  {
    const char *old_message;
    LOCK_global_read_lock.lock();
    old_message= enter_cond(COND_global_read_lock, LOCK_global_read_lock,
                            "Waiting to get readlock");

    waiting_for_read_lock++;
    boost::mutex::scoped_lock scopedLock(LOCK_global_read_lock, boost::adopt_lock_t());
    while (protect_against_global_read_lock && not getKilled())
      COND_global_read_lock.wait(scopedLock);
    waiting_for_read_lock--;
    scopedLock.release();
    if (getKilled())
    {
      exit_cond(old_message);
      return true;
    }
    setGlobalReadLock(Session::GOT_GLOBAL_READ_LOCK);
    global_read_lock++;
    exit_cond(old_message); // this unlocks LOCK_global_read_lock
  }

  /*
    We DON'T set global_read_lock_blocks_commit now, it will be set after
    tables are flushed (as the present function serves for FLUSH TABLES WITH
    READ LOCK only). Doing things in this order is necessary to avoid
    deadlocks (we must allow COMMIT until all tables are closed; we should not
    forbid it before, or we can have a 3-thread deadlock if 2 do SELECT FOR
    UPDATE and one does FLUSH TABLES WITH READ LOCK).
  */
  return false;
}


void Session::unlockGlobalReadLock(void)
{
  uint32_t tmp;

  if (not isGlobalReadLock()) // If we have no personal stake in the global lock, just return
    return;

  {
    boost::mutex::scoped_lock scopedLock(LOCK_global_read_lock);
    tmp= --global_read_lock;
    if (isGlobalReadLock() == Session::MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT)
      --global_read_lock_blocks_commit;
  }
  /* Send the signal outside the mutex to avoid a context switch */
  if (not tmp)
  {
    COND_global_read_lock.notify_all();
  }
  setGlobalReadLock(Session::NONE);
}

static inline bool must_wait(bool is_not_commit)
{
  return (global_read_lock &&
          (is_not_commit ||
          global_read_lock_blocks_commit));
}

bool Session::wait_if_global_read_lock(bool abort_on_refresh, bool is_not_commit)
{
  const char *old_message= NULL;
  bool result= 0, need_exit_cond;

  /*
    Assert that we do not own table::Cache::mutex(). If we would own it, other
    threads could not close their tables. This would make a pretty
    deadlock.
  */
  safe_mutex_assert_not_owner(table::Cache::mutex().native_handle());

  LOCK_global_read_lock.lock();
  if ((need_exit_cond= must_wait(is_not_commit)))
  {
    if (isGlobalReadLock())		// This thread had the read locks
    {
      if (is_not_commit)
        my_message(ER_CANT_UPDATE_WITH_READLOCK,
                   ER(ER_CANT_UPDATE_WITH_READLOCK), MYF(0));
      LOCK_global_read_lock.unlock();
      /*
        We allow FLUSHer to COMMIT; we assume FLUSHer knows what it does.
        This allowance is needed to not break existing versions of innobackup
        which do a BEGIN; INSERT; FLUSH TABLES WITH READ LOCK; COMMIT.
      */
      return is_not_commit;
    }
    old_message= enter_cond(COND_global_read_lock, LOCK_global_read_lock,
                            "Waiting for release of readlock");

    while (must_wait(is_not_commit) && not getKilled() &&
	   (!abort_on_refresh || open_tables.version == g_refresh_version))
    {
      boost::mutex::scoped_lock scoped(LOCK_global_read_lock, boost::adopt_lock_t());
      COND_global_read_lock.wait(scoped);
      scoped.release();
    }
    if (getKilled())
      result=1;
  }

  if (not abort_on_refresh && not result)
    protect_against_global_read_lock++;

  /*
    The following is only true in case of a global read locks (which is rare)
    and if old_message is set
  */
  if (unlikely(need_exit_cond))
  {
    exit_cond(old_message); // this unlocks LOCK_global_read_lock
  }
  else
  {
    LOCK_global_read_lock.unlock();
  }

  return result;
}


void Session::startWaitingGlobalReadLock()
{
  if (unlikely(isGlobalReadLock()))
    return;

  LOCK_global_read_lock.lock();
  bool tmp= (!--protect_against_global_read_lock && (waiting_for_read_lock || global_read_lock_blocks_commit));
  LOCK_global_read_lock.unlock();

  if (tmp)
    COND_global_read_lock.notify_all();
}


bool Session::makeGlobalReadLockBlockCommit()
{
  bool error;
  const char *old_message;
  /*
    If we didn't succeed lock_global_read_lock(), or if we already suceeded
    Session::makeGlobalReadLockBlockCommit(), do nothing.
  */
  if (isGlobalReadLock() != Session::GOT_GLOBAL_READ_LOCK)
    return false;
  LOCK_global_read_lock.lock();
  /* increment this BEFORE waiting on cond (otherwise race cond) */
  global_read_lock_blocks_commit++;
  old_message= enter_cond(COND_global_read_lock, LOCK_global_read_lock,
                          "Waiting for all running commits to finish");
  while (protect_against_global_read_lock && not getKilled())
  {
    boost::mutex::scoped_lock scopedLock(LOCK_global_read_lock, boost::adopt_lock_t());
    COND_global_read_lock.wait(scopedLock);
    scopedLock.release();
  }
  if ((error= test(getKilled())))
  {
    global_read_lock_blocks_commit--; // undo what we did
  }
  else
  {
    setGlobalReadLock(Session::MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT);
  }

  exit_cond(old_message); // this unlocks LOCK_global_read_lock

  return error;
}


/**
  Broadcast COND_refresh and COND_global_read_lock.

    Due to a bug in a threading library it could happen that a signal
    did not reach its target. A condition for this was that the same
    condition variable was used with different mutexes in
    pthread_cond_wait(). Some time ago we changed table::Cache::mutex() to
    LOCK_global_read_lock in global read lock handling. So COND_refresh
    was used with table::Cache::mutex() and LOCK_global_read_lock.

    We did now also change from COND_refresh to COND_global_read_lock
    in global read lock handling. But now it is necessary to signal
    both conditions at the same time.

  @note
    When signalling COND_global_read_lock within the global read lock
    handling, it is not necessary to also signal COND_refresh.
*/

void locking::broadcast_refresh()
{
  COND_refresh.notify_all();
  COND_global_read_lock.notify_all();
}

} /* namespace drizzled */
