/* Copyright (C) 2000-2006 MySQL AB

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

  Locking functions for mysql.

  Because of the new concurrent inserts, we must first get external locks
  before getting internal locks.  If we do it in the other order, the status
  information is not up to date when called from the lock handler.

  GENERAL DESCRIPTION OF LOCKING

  When not using LOCK TABLES:

  - For each SQL statement mysql_lock_tables() is called for all involved
    tables.
    - mysql_lock_tables() will call
      table_handler->external_lock(session,locktype) for each table.
      This is followed by a call to thr_multi_lock() for all tables.

  - When statement is done, we call mysql_unlock_tables().
    This will call DrizzleLock::unlock() followed by
    table_handler->external_lock(session, F_UNLCK) for each table.

  - Note that mysql_unlock_tables() may be called several times as
    MySQL in some cases can free some tables earlier than others.

  - The above is true both for normal and temporary tables.

  - Temporary non transactional tables are never passed to thr_multi_lock()
    and we never call external_lock(session, F_UNLOCK) on these.

  When using LOCK TABLES:

  - LOCK Table will call mysql_lock_tables() for all tables.
    mysql_lock_tables() will call
    table_handler->external_lock(session,locktype) for each table.
    This is followed by a call to thr_multi_lock() for all tables.

  - For each statement, we will call table_handler->start_stmt(Session)
    to inform the table handler that we are using the table.

    The tables used can only be tables used in LOCK TABLES or a
    temporary table.

  - When statement is done, we will call ha_commit_stmt(session);

  - When calling UNLOCK TABLES we call mysql_unlock_tables() for all
    tables used in LOCK TABLES

  If table_handler->external_lock(session, locktype) fails, we call
  table_handler->external_lock(session, F_UNLCK) for each table that was locked,
  excluding one that caused failure. That means handler must cleanup itself
  in case external_lock() fails.

  @todo
  Change to use malloc() ONLY when using LOCK TABLES command or when
  we are forced to use mysql_lock_merge.
*/
#include "config.h"
#include <fcntl.h>
#include <drizzled/error.h>
#include <drizzled/my_hash.h>
#include <drizzled/thr_lock.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include "drizzled/pthread_globals.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/pthread_globals.h"

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

static DrizzleLock *get_lock_data(Session *session, Table **table,
                                  uint32_t count,
                                  bool should_lock, Table **write_locked);
static int lock_external(Session *session, Table **table,uint32_t count);
static int unlock_external(Session *session, Table **table,uint32_t count);
static void print_lock_error(int error, const char *);

/*
  Lock tables.

  SYNOPSIS
    mysql_lock_tables()
    session                         The current thread.
    tables                      An array of pointers to the tables to lock.
    count                       The number of tables to lock.
    flags                       Options:
      DRIZZLE_LOCK_IGNORE_GLOBAL_READ_LOCK      Ignore a global read lock
      DRIZZLE_LOCK_IGNORE_FLUSH                 Ignore a flush tables.
      DRIZZLE_LOCK_NOTIFY_IF_NEED_REOPEN        Instead of reopening altered
                                              or dropped tables by itself,
                                              mysql_lock_tables() should
                                              notify upper level and rely
                                              on caller doing this.
    need_reopen                 Out parameter, TRUE if some tables were altered
                                or deleted and should be reopened by caller.

  RETURN
    A lock structure pointer on success.
    NULL on error or if some tables should be reopen.
*/

/* Map the return value of thr_lock to an error from errmsg.txt */
static int thr_lock_errno_to_mysql[]=
{ 0, 1, ER_LOCK_WAIT_TIMEOUT, ER_LOCK_DEADLOCK };


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

static void reset_lock_data_and_free(DrizzleLock **mysql_lock)
{
  DrizzleLock *sql_lock= *mysql_lock;
  sql_lock->reset();
  delete sql_lock;
  *mysql_lock= 0;
}

void DrizzleLock::reset(void)
{
  for (std::vector<THR_LOCK_DATA *>::iterator iter= locks.begin(); iter != locks.end(); iter++)
  {
    (*iter)->type= TL_UNLOCK;
  }
}


DrizzleLock *mysql_lock_tables(Session *session, Table **tables, uint32_t count,
                                uint32_t flags, bool *need_reopen)
{
  DrizzleLock *sql_lock;
  Table *write_lock_used;
  vector<plugin::StorageEngine *> involved_engines;
  int rc;

  *need_reopen= false;

  for (;;)
  {
    if (! (sql_lock= get_lock_data(session, tables, count, true,
                                   &write_lock_used)))
      break;

    if (global_read_lock && write_lock_used &&
        ! (flags & DRIZZLE_LOCK_IGNORE_GLOBAL_READ_LOCK))
    {
      /*
	Someone has issued LOCK ALL TABLES FOR READ and we want a write lock
	Wait until the lock is gone
      */
      if (wait_if_global_read_lock(session, 1, 1))
      {
        /* Clear the lock type of all lock data to avoid reusage. */
        reset_lock_data_and_free(&sql_lock);
	break;
      }
      if (session->version != refresh_version)
      {
        /* Clear the lock type of all lock data to avoid reusage. */
        reset_lock_data_and_free(&sql_lock);
	goto retry;
      }
    }
    
    session->set_proc_info("Notify start statement");
    /*
     * Here, we advise all storage engines involved in the
     * statement that we are starting a new statement
     */
    if (sql_lock->table_count)
    {
      size_t num_tables= sql_lock->table_count;
      plugin::StorageEngine *engine;
      set<size_t> involved_slots;
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
               bind2nd(mem_fun(&plugin::StorageEngine::startStatement), session));
    }

    session->set_proc_info("External lock");
    /*
     * Here, the call to lock_external() informs the
     * all engines for all tables used in this statement
     * of the type of lock that Drizzle intends to take on a 
     * specific table.
     */
    if (sql_lock->table_count && lock_external(session, sql_lock->getTable(),
                                               sql_lock->table_count))
    {
      /* Clear the lock type of all lock data to avoid reusage. */
      reset_lock_data_and_free(&sql_lock);
      break;
    }
    session->set_proc_info("Table lock");
    /* Copy the lock data array. thr_multi_lock() reorders its contens. */
    memcpy(sql_lock->getLocks() + sql_lock->lock_count,
           sql_lock->getLocks(),
           sql_lock->lock_count * sizeof(*sql_lock->getLocks()));
    /* Lock on the copied half of the lock data array. */
    rc= thr_lock_errno_to_mysql[(int) thr_multi_lock(sql_lock->getLocks() +
                                                     sql_lock->lock_count,
                                                     sql_lock->lock_count,
                                                     session->lock_id)];
    if (rc > 1)                                 /* a timeout or a deadlock */
    {
      if (sql_lock->table_count)
        unlock_external(session, sql_lock->getTable(), sql_lock->table_count);
      reset_lock_data_and_free(&sql_lock);
      my_error(rc, MYF(0));
      break;
    }
    else if (rc == 1)                           /* aborted */
    {
      session->some_tables_deleted=1;		// Try again
      sql_lock->lock_count= 0;                  // Locks are already freed
      // Fall through: unlock, reset lock data, free and retry
    }
    else if (!session->some_tables_deleted || (flags & DRIZZLE_LOCK_IGNORE_FLUSH))
    {
      /*
        Thread was killed or lock aborted. Let upper level close all
        used tables and retry or give error.
      */
      break;
    }
    else if (!session->open_tables)
    {
      // Only using temporary tables, no need to unlock
      session->some_tables_deleted= 0;
      break;
    }
    session->set_proc_info(0);

    /* going to retry, unlock all tables */
    if (sql_lock->lock_count)
        sql_lock->unlock(sql_lock->lock_count);

    if (sql_lock->table_count)
      unlock_external(session, sql_lock->getTable(), sql_lock->table_count);

    /*
      If thr_multi_lock fails it resets lock type for tables, which
      were locked before (and including) one that caused error. Lock
      type for other tables preserved.
    */
    reset_lock_data_and_free(&sql_lock);

    /*
     * Notify all involved engines that the
     * SQL statement has ended
     */
    for_each(involved_engines.begin(),
             involved_engines.end(),
             bind2nd(mem_fun(&plugin::StorageEngine::endStatement), session));
retry:
    if (flags & DRIZZLE_LOCK_NOTIFY_IF_NEED_REOPEN)
    {
      *need_reopen= true;
      break;
    }
    if (wait_for_tables(session))
      break;					// Couldn't open tables
  }
  session->set_proc_info(0);
  if (session->killed)
  {
    session->send_kill_message();
    if (sql_lock)
    {
      mysql_unlock_tables(session,sql_lock);
      sql_lock= NULL;
    }
  }
  session->set_time_after_lock();
  return (sql_lock);
}


static int lock_external(Session *session, Table **tables, uint32_t count)
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

    if ((error=(*tables)->cursor->ha_external_lock(session,lock_type)))
    {
      print_lock_error(error, (*tables)->cursor->getEngine()->getName().c_str());
      while (--i)
      {
        tables--;
	(*tables)->cursor->ha_external_lock(session, F_UNLCK);
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


void mysql_unlock_tables(Session *session, DrizzleLock *sql_lock)
{
  if (sql_lock->lock_count)
    sql_lock->unlock(sql_lock->lock_count);
  if (sql_lock->table_count)
    unlock_external(session, sql_lock->getTable(), sql_lock->table_count);
  delete sql_lock;
}

/**
  Unlock some of the tables locked by mysql_lock_tables.

  This will work even if get_lock_data fails (next unlock will free all)
*/

void mysql_unlock_some_tables(Session *session, Table **table, uint32_t count)
{
  DrizzleLock *sql_lock;
  Table *write_lock_used;
  if ((sql_lock= get_lock_data(session, table, count, false,
                               &write_lock_used)))
    mysql_unlock_tables(session, sql_lock);
}


/**
  unlock all tables locked for read.
*/

void mysql_unlock_read_tables(Session *session, DrizzleLock *sql_lock)
{
  uint32_t i,found;

  /* Move all write locks first */
  THR_LOCK_DATA **lock=sql_lock->getLocks();
  for (i=found=0 ; i < sql_lock->lock_count ; i++)
  {
    if (sql_lock->getLocks()[i]->type >= TL_WRITE_ALLOW_READ)
    {
      std::swap(*lock, sql_lock->getLocks()[i]);
      lock++;
      found++;
    }
  }
  /* unlock the read locked tables */
  if (i != found)
  {
    sql_lock->unlock(i - found);
    sql_lock->lock_count= found;
  }

  /* Then do the same for the external locks */
  /* Move all write locked tables first */
  Table **table= sql_lock->getTable();
  for (i=found=0 ; i < sql_lock->table_count ; i++)
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
    unlock_external(session,table,i-found);
    sql_lock->table_count=found;
  }
  /* Fix the lock positions in Table */
  table= sql_lock->getTable();
  found= 0;
  for (i= 0; i < sql_lock->table_count; i++)
  {
    Table *tbl= *table;
    tbl->lock_position= table - sql_lock->getTable();
    tbl->lock_data_start= found;
    found+= tbl->lock_count;
    table++;
  }
  return;
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

void mysql_lock_remove(Session *session, Table *table)
{
  mysql_unlock_some_tables(session, &table, /* table count */ 1);
}


/** Abort all other threads waiting to get lock in table. */

void mysql_lock_abort(Session *session, Table *table)
{
  DrizzleLock *locked;
  Table *write_lock_used;

  if ((locked= get_lock_data(session, &table, 1, false,
                             &write_lock_used)))
  {
    for (uint32_t x= 0; x < locked->lock_count; x++)
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

bool mysql_lock_abort_for_thread(Session *session, Table *table)
{
  DrizzleLock *locked;
  Table *write_lock_used;
  bool result= false;

  if ((locked= get_lock_data(session, &table, 1, false,
                             &write_lock_used)))
  {
    for (uint32_t i= 0; i < locked->lock_count; i++)
    {
      if (locked->getLocks()[i]->lock->abort_locks_for_thread(table->in_use->thread_id))
        result= true;
    }
    delete locked;
  }
  return result;
}

/** Unlock a set of external. */

static int unlock_external(Session *session, Table **table, uint32_t count)
{
  int error,error_code;

  error_code=0;
  do
  {
    if ((*table)->current_lock != F_UNLCK)
    {
      (*table)->current_lock = F_UNLCK;
      if ((error=(*table)->cursor->ha_external_lock(session, F_UNLCK)))
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

static DrizzleLock *get_lock_data(Session *session, Table **table_ptr, uint32_t count,
				 bool should_lock, Table **write_lock_used)
{
  uint32_t lock_count;
  DrizzleLock *sql_lock;
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
  sql_lock= new DrizzleLock(lock_count, lock_count*2);

  if (not sql_lock)
    return NULL;

  locks= locks_buf= sql_lock->getLocks();
  to= table_buf= sql_lock->getTable();

  for (uint32_t i= 0; i < count ; i++)
  {
    Table *table;
    enum thr_lock_type lock_type;

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
        sql_lock->lock_count= locks - sql_lock->getLocks();
        reset_lock_data_and_free(&sql_lock);
	return NULL;
      }
    }
    locks_start= locks;
    locks= table->cursor->store_lock(session, locks,
                                   should_lock == false ? TL_IGNORE : lock_type);
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
  sql_lock->lock_count= locks - locks_buf;

  return sql_lock;
}


/**
  Put a not open table with an old refresh version in the table cache.

  @param session			Thread handler
  @param table_list		Lock first table in this list
  @param check_in_use           Do we need to check if table already in use by us

  @note
    One must have a lock on LOCK_open!

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

static int lock_table_name(Session *session, TableList *table_list, bool check_in_use)
{
  bool  found_locked_table= false;
  TableIdentifier identifier(table_list->db, table_list->table_name);
  const TableIdentifier::Key &key(identifier.getKey());

  if (check_in_use)
  {
    /* Only insert the table if we haven't insert it already */
    TableOpenCacheRange ppp;

    ppp= get_open_cache().equal_range(key);

    for (TableOpenCache::const_iterator iter= ppp.first;
         iter != ppp.second; ++iter)
    {
      Table *table= (*iter).second;
      if (table->reginfo.lock_type < TL_WRITE)
      {
        if (table->in_use == session)
          found_locked_table= true;
        continue;
      }

      if (table->in_use == session)
      {
        table->getMutableShare()->resetVersion();                  // Ensure no one can use this
        table->locked_by_name= true;
        return 0;
      }
    }
  }

  Table *table;
  if (!(table= session->table_cache_insert_placeholder(table_list->db, table_list->table_name)))
  {
    return -1;
  }

  table_list->table= table;

  /* Return 1 if table is in use */
  return(test(remove_table_from_cache(session, identifier,
				      check_in_use ? RTFC_NO_FLAG : RTFC_WAIT_OTHER_THREAD_FLAG)));
}


void unlock_table_name(TableList *table_list)
{
  if (table_list->table)
  {
    remove_table(table_list->table);
    broadcast_refresh();
  }
}


static bool locked_named_table(TableList *table_list)
{
  for (; table_list ; table_list=table_list->next_local)
  {
    Table *table= table_list->table;
    if (table)
    {
      Table *save_next= table->getNext();
      bool result;
      table->setNext(NULL);
      result= table_is_used(table_list->table, 0);
      table->setNext(save_next);
      if (result)
        return 1;
    }
  }
  return 0;					// All tables are locked
}


static bool wait_for_locked_table_names(Session *session, TableList *table_list)
{
  bool result= false;

#if 0
  assert(ownership of LOCK_open);
#endif

  while (locked_named_table(table_list))
  {
    if (session->killed)
    {
      result=1;
      break;
    }
    session->wait_for_condition(LOCK_open, COND_refresh);
    LOCK_open.lock(); /* Wait for a table to unlock and then lock it */
  }
  return result;
}


/**
  Lock all tables in list with a name lock.

  REQUIREMENTS
  - One must have a lock on LOCK_open when calling this

  @param table_list		Names of tables to lock

  @retval
    0	ok
  @retval
    1	Fatal error (end of memory ?)
*/

static bool lock_table_names(Session *session, TableList *table_list)
{
  bool got_all_locks=1;
  TableList *lock_table;

  for (lock_table= table_list; lock_table; lock_table= lock_table->next_local)
  {
    int got_lock;
    if ((got_lock= lock_table_name(session, lock_table, true)) < 0)
      goto end;					// Fatal error
    if (got_lock)
      got_all_locks=0;				// Someone is using table
  }

  /* If some table was in use, wait until we got the lock */
  if (!got_all_locks && wait_for_locked_table_names(session, table_list))
    goto end;
  return false;

end:
  unlock_table_names(table_list, lock_table);

  return true;
}


/**
  Unlock all tables in list with a name lock.

  @param session        Thread handle.
  @param table_list Names of tables to lock.

  @note
    This function needs to be protected by LOCK_open. If we're
    under LOCK TABLES, this function does not work as advertised. Namely,
    it does not exclude other threads from using this table and does not
    put an exclusive name lock on this table into the table cache.

  @see lock_table_names
  @see unlock_table_names

  @retval TRUE An error occured.
  @retval FALSE Name lock successfully acquired.
*/

bool lock_table_names_exclusively(Session *session, TableList *table_list)
{
  if (lock_table_names(session, table_list))
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
    One must have a lock on LOCK_open when calling this.

  @note
    This function will broadcast refresh signals to inform other threads
    that the name locks are removed.

  @retval
    0	ok
  @retval
    1	Fatal error (end of memory ?)
*/

void unlock_table_names(TableList *table_list, TableList *last_table)
{
  for (TableList *table= table_list;
       table != last_table;
       table= table->next_local)
    unlock_table_name(table);
  broadcast_refresh();
}


static void print_lock_error(int error, const char *table)
{
  int textno;

  switch (error) {
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

  (XXX: one should never take LOCK_open if LOCK_global_read_lock is
  taken, otherwise a deadlock may occur. Other mutexes could be a
  problem too - grep the code for global_read_lock if you want to use
  any other mutex here) Also one must not hold LOCK_open when calling
  wait_if_global_read_lock(). When the thread with the global read lock
  tries to close its tables, it needs to take LOCK_open in
  close_thread_table().

  How blocking of threads by global read lock is achieved: that's
  advisory. Any piece of code which should be blocked by global read lock must
  be designed like this:
  - call to wait_if_global_read_lock(). When this returns 0, no global read
  lock is owned; if argument abort_on_refresh was 0, none can be obtained.
  - job
  - if abort_on_refresh was 0, call to start_waiting_global_read_lock() to
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
  3) make_global_read_lock_block_commit().
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

#define GOT_GLOBAL_READ_LOCK               1
#define MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT 2

bool lock_global_read_lock(Session *session)
{
  if (!session->global_read_lock)
  {
    const char *old_message;
    LOCK_global_read_lock.lock();
    old_message=session->enter_cond(COND_global_read_lock, LOCK_global_read_lock,
                                    "Waiting to get readlock");

    waiting_for_read_lock++;
    boost_unique_lock_t scopedLock(LOCK_global_read_lock, boost::adopt_lock_t());
    while (protect_against_global_read_lock && !session->killed)
      COND_global_read_lock.wait(scopedLock);
    waiting_for_read_lock--;
    scopedLock.release();
    if (session->killed)
    {
      session->exit_cond(old_message);
      return true;
    }
    session->global_read_lock= GOT_GLOBAL_READ_LOCK;
    global_read_lock++;
    session->exit_cond(old_message); // this unlocks LOCK_global_read_lock
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


void unlock_global_read_lock(Session *session)
{
  uint32_t tmp;

  {
    boost_unique_lock_t scopedLock(LOCK_global_read_lock);
    tmp= --global_read_lock;
    if (session->global_read_lock == MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT)
      --global_read_lock_blocks_commit;
  }
  /* Send the signal outside the mutex to avoid a context switch */
  if (!tmp)
  {
    COND_global_read_lock.notify_all();
  }
  session->global_read_lock= 0;
}

static inline bool must_wait(bool is_not_commit)
{
  return (global_read_lock &&
          (is_not_commit ||
          global_read_lock_blocks_commit));
}

bool wait_if_global_read_lock(Session *session, bool abort_on_refresh,
                              bool is_not_commit)
{
  const char *old_message= NULL;
  bool result= 0, need_exit_cond;

  /*
    Assert that we do not own LOCK_open. If we would own it, other
    threads could not close their tables. This would make a pretty
    deadlock.
  */
  safe_mutex_assert_not_owner(LOCK_open.native_handle());

  LOCK_global_read_lock.lock();
  if ((need_exit_cond= must_wait(is_not_commit)))
  {
    if (session->global_read_lock)		// This thread had the read locks
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
    old_message=session->enter_cond(COND_global_read_lock, LOCK_global_read_lock,
                                    "Waiting for release of readlock");
    while (must_wait(is_not_commit) && ! session->killed &&
	   (!abort_on_refresh || session->version == refresh_version))
    {
      boost_unique_lock_t scoped(LOCK_global_read_lock, boost::adopt_lock_t());
      COND_global_read_lock.wait(scoped);
      scoped.release();
    }
    if (session->killed)
      result=1;
  }
  if (!abort_on_refresh && !result)
    protect_against_global_read_lock++;
  /*
    The following is only true in case of a global read locks (which is rare)
    and if old_message is set
  */
  if (unlikely(need_exit_cond))
    session->exit_cond(old_message); // this unlocks LOCK_global_read_lock
  else
    LOCK_global_read_lock.unlock();
  return result;
}


void start_waiting_global_read_lock(Session *session)
{
  bool tmp;
  if (unlikely(session->global_read_lock))
    return;
  LOCK_global_read_lock.lock();
  tmp= (!--protect_against_global_read_lock &&
        (waiting_for_read_lock || global_read_lock_blocks_commit));
  LOCK_global_read_lock.unlock();
  if (tmp)
    COND_global_read_lock.notify_all();
  return;
}


bool make_global_read_lock_block_commit(Session *session)
{
  bool error;
  const char *old_message;
  /*
    If we didn't succeed lock_global_read_lock(), or if we already suceeded
    make_global_read_lock_block_commit(), do nothing.
  */
  if (session->global_read_lock != GOT_GLOBAL_READ_LOCK)
    return false;
  LOCK_global_read_lock.lock();
  /* increment this BEFORE waiting on cond (otherwise race cond) */
  global_read_lock_blocks_commit++;
  old_message= session->enter_cond(COND_global_read_lock, LOCK_global_read_lock,
                                   "Waiting for all running commits to finish");
  while (protect_against_global_read_lock && !session->killed)
  {
    boost_unique_lock_t scopedLock(LOCK_global_read_lock, boost::adopt_lock_t());
    COND_global_read_lock.wait(scopedLock);
    scopedLock.release();
  }
  if ((error= test(session->killed)))
    global_read_lock_blocks_commit--; // undo what we did
  else
    session->global_read_lock= MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT;
  session->exit_cond(old_message); // this unlocks LOCK_global_read_lock
  return error;
}


/**
  Broadcast COND_refresh and COND_global_read_lock.

    Due to a bug in a threading library it could happen that a signal
    did not reach its target. A condition for this was that the same
    condition variable was used with different mutexes in
    pthread_cond_wait(). Some time ago we changed LOCK_open to
    LOCK_global_read_lock in global read lock handling. So COND_refresh
    was used with LOCK_open and LOCK_global_read_lock.

    We did now also change from COND_refresh to COND_global_read_lock
    in global read lock handling. But now it is necessary to signal
    both conditions at the same time.

  @note
    When signalling COND_global_read_lock within the global read lock
    handling, it is not necessary to also signal COND_refresh.
*/

void broadcast_refresh(void)
{
  COND_refresh.notify_all();
  COND_global_read_lock.notify_all();
}


/**
  @} (end of group Locking)
*/

} /* namespace drizzled */
