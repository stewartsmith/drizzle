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


/* Basic functions needed by many modules */
#include <config.h>
#include <assert.h>

#include <signal.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <drizzled/internal/my_pthread.h>
#include <drizzled/internal/thread_var.h>

#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/nested_join.h>
#include <drizzled/sql_base.h>
#include <drizzled/show.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/replication_services.h>
#include <drizzled/check_stack_overrun.h>
#include <drizzled/lock.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/cached_directory.h>
#include <drizzled/field/epoch.h>
#include <drizzled/field/null.h>
#include <drizzled/sql_table.h>
#include <drizzled/charset.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/drizzled.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/table/temporary.h>
#include <drizzled/table/placeholder.h>
#include <drizzled/table/unused.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/session.h>
#include <drizzled/item/subselect.h>
#include <drizzled/sql_lex.h>
#include <drizzled/catalog/local.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/table/cache.h>

using namespace std;

namespace drizzled {

extern bool volatile shutdown_in_progress;

void table_cache_free()
{
  g_refresh_version++;				// Force close of open tables

  table::getUnused().clear();
  table::getCache().clear();
}

/*
  Close cursor handle, but leave the table in the table cache

  SYNOPSIS
  close_handle_and_leave_table_as_lock()
  table		Table Cursor

  NOTES
  By leaving the table in the table cache, it disallows any other thread
  to open the table

  session->getKilled() will be set if we run out of memory

  If closing a MERGE child, the calling function has to take care for
  closing the parent too, if necessary.
*/


void close_handle_and_leave_table_as_lock(Table *table)
{
  assert(table->db_stat);
  assert(table->getShare()->getType() == message::Table::STANDARD);

  /*
    Make a local copy of the table share and free the current one.
    This has to be done to ensure that the table share is removed from
    the table defintion cache as soon as the last instance is removed
  */
  identifier::Table identifier(table->getShare()->getSchemaName(), table->getShare()->getTableName(), message::Table::INTERNAL);
  const identifier::Table::Key &key(identifier.getKey());
  TableShare *share= new TableShare(identifier.getType(),
                                    identifier,
                                    const_cast<char *>(key.vector()),  static_cast<uint32_t>(table->getShare()->getCacheKeySize()));

  table->cursor->close();
  table->db_stat= 0;                            // Mark cursor closed
  table::instance::release(table->getMutableShare());
  table->setShare(share);
}


/*****************************************************************************
 *	 Functions to free open table cache
 ****************************************************************************/


void Table::intern_close_table()
{						// Free all structures
  free_io_cache();
  if (cursor)                              // Not true if name lock
  {
    delete_table(true);			// close cursor
  }
}

/* Free resources allocated by filesort() and read_record() */

void Table::free_io_cache()
{
  if (sort.io_cache)
  {
    sort.io_cache->close_cached_file();
    safe_delete(sort.io_cache);
  }
}


/*
  Close all tables which aren't in use by any thread

  @param session Thread context (may be NULL)
  @param tables List of tables to remove from the cache
  @param have_lock If table::Cache::mutex() is locked
  @param wait_for_refresh Wait for a impending flush
  @param wait_for_placeholders Wait for tables being reopened so that the GRL
  won't proceed while write-locked tables are being reopened by other
  threads.

  @remark Session can be NULL, but then wait_for_refresh must be false
  and tables must be NULL.
*/

bool Session::close_cached_tables(TableList *tables, bool wait_for_refresh, bool wait_for_placeholders)
{
  bool result= false;
  Session *session= this;

  {
    boost::mutex::scoped_lock scopedLock(table::Cache::mutex()); /* Optionally lock for remove tables from open_cahe if not in use */
    if (not tables)
    {
      g_refresh_version++;				// Force close of open tables
      table::getUnused().clear();
    }
    else
    {
      bool found= false;
      for (TableList *table= tables; table; table= table->next_local)
      {
        if (table::Cache::removeTable(*session, identifier::Table(table->getSchemaName(), table->getTableName()), RTFC_OWNED_BY_Session_FLAG))
        {
          found= true;
        }
      }
      if (!found)
        wait_for_refresh= false;			// Nothing to wait for
    }

    if (wait_for_refresh)
    {
      /*
        If there is any table that has a lower refresh_version, wait until
        this is closed (or this thread is killed) before returning
      */
      session->mysys_var->current_mutex= &table::Cache::mutex();
      session->mysys_var->current_cond= &COND_refresh;
      session->set_proc_info("Flushing tables");

      session->close_old_data_files();

      bool found= true;
      /* Wait until all threads has closed all the tables we had locked */
      while (found && ! session->getKilled())
      {
        found= false;
        for (table::CacheMap::const_iterator iter= table::getCache().begin();
             iter != table::getCache().end();
             iter++)
        {
          Table *table= iter->second;
          /* Avoid a self-deadlock. */
          if (table->in_use == session)
            continue;
          /*
            Note that we wait here only for tables which are actually open, and
            not for placeholders with Table::open_placeholder set. Waiting for
            latter will cause deadlock in the following scenario, for example:

            conn1-> lock table t1 write;
            conn2-> lock table t2 write;
            conn1-> flush tables;
            conn2-> flush tables;

            It also does not make sense to wait for those of placeholders that
            are employed by CREATE TABLE as in this case table simply does not
            exist yet.
          */
          if (table->needs_reopen_or_name_lock() && (table->db_stat ||
                                                     (table->open_placeholder && wait_for_placeholders)))
          {
            found= true;
            COND_refresh.wait(scopedLock);
            break;
          }
        }
      }
      /*
        No other thread has the locked tables open; reopen them and get the
        old locks. This should always succeed (unless some external process
        has removed the tables)
      */
      result= session->reopen_tables();

      /* Set version for table */
      for (Table *table= session->open_tables.open_tables_; table ; table= table->getNext())
      {
        /*
          Preserve the version (0) of write locked tables so that a impending
          global read lock won't sneak in.
        */
        if (table->reginfo.lock_type < TL_WRITE_ALLOW_WRITE)
          table->getMutableShare()->refreshVersion();
      }
    }
  }

  if (wait_for_refresh)
  {
    boost::mutex::scoped_lock scopedLock(session->mysys_var->mutex);
    session->mysys_var->current_mutex= 0;
    session->mysys_var->current_cond= 0;
    session->set_proc_info(0);
  }

  return result;
}


/**
  move one table to free list
*/

bool Open_tables_state::free_cached_table()
{
  table::Concurrent *table= static_cast<table::Concurrent *>(open_tables_);

  safe_mutex_assert_owner(table::Cache::mutex().native_handle());
  assert(table->key_read == 0);
  assert(not table->cursor || table->cursor->inited == Cursor::NONE);

  open_tables_= table->getNext();

  if (table->needs_reopen_or_name_lock() ||
      version != g_refresh_version || !table->db_stat)
  {
    table::remove_table(table);
    return true;
  }
  /*
    Open placeholders have Table::db_stat set to 0, so they should be
    handled by the first alternative.
  */
  assert(not table->open_placeholder);

  /* Free memory and reset for next loop */
  table->cursor->ha_reset();
  table->in_use= NULL;

  table::getUnused().link(table);
  return false;
}


/**
  Auxiliary function to close all tables in the open_tables list.

  @param session Thread context.

  @remark It should not ordinarily be called directly.
*/

void Open_tables_state::close_open_tables()
{
  bool found_old_table= false;

  safe_mutex_assert_not_owner(table::Cache::mutex().native_handle());

  boost::mutex::scoped_lock scoped_lock(table::Cache::mutex()); /* Close all open tables on Session */

  while (open_tables_)
  {
    found_old_table|= free_cached_table();
  }
  if (found_old_table)
  {
    /* Tell threads waiting for refresh that something has happened */
    locking::broadcast_refresh();
  }
}

/*
  Find table in list.

  SYNOPSIS
  find_table_in_list()
  table		Pointer to table list
  offset		Offset to which list in table structure to use
  db_name		Data base name
  table_name		Table name

NOTES:
This is called by find_table_in_global_list().

RETURN VALUES
NULL	Table not found
#		Pointer to found table.
*/

TableList *find_table_in_list(TableList *table,
                              TableList *TableList::*link,
                              const char *db_name,
                              const char *table_name)
{
  for (; table; table= table->*link )
  {
    if ((table->table == 0 || table->table->getShare()->getType() == message::Table::STANDARD) and
        my_strcasecmp(system_charset_info, table->getSchemaName(), db_name) == 0 and
        my_strcasecmp(system_charset_info, table->getTableName(), table_name) == 0)
    {
      break;
    }
  }
  return table;
}


/*
  Test that table is unique (It's only exists once in the table list)

  SYNOPSIS
  unique_table()
  session                   thread handle
  table                 table which should be checked
  table_list            list of tables
  check_alias           whether to check tables' aliases

NOTE: to exclude derived tables from check we use following mechanism:
a) during derived table processing set Session::derived_tables_processing
b) JOIN::prepare set SELECT::exclude_from_table_unique_test if
Session::derived_tables_processing set. (we can't use JOIN::execute
because for PS we perform only JOIN::prepare, but we can't set this
flag in JOIN::prepare if we are not sure that we are in derived table
processing loop, because multi-update call fix_fields() for some its
items (which mean JOIN::prepare for subqueries) before unique_table
call to detect which tables should be locked for write).
c) unique_table skip all tables which belong to SELECT with
SELECT::exclude_from_table_unique_test set.
Also SELECT::exclude_from_table_unique_test used to exclude from check
tables of main SELECT of multi-delete and multi-update

We also skip tables with TableList::prelocking_placeholder set,
because we want to allow SELECTs from them, and their modification
will rise the error anyway.

TODO: when we will have table/view change detection we can do this check
only once for PS/SP

RETURN
found duplicate
0 if table is unique
*/

TableList* unique_table(TableList *table, TableList *table_list,
                        bool check_alias)
{
  TableList *res;
  const char *d_name, *t_name, *t_alias;

  /*
    If this function called for query which update table (INSERT/UPDATE/...)
    then we have in table->table pointer to Table object which we are
    updating even if it is VIEW so we need TableList of this Table object
    to get right names (even if lower_case_table_names used).

    If this function called for CREATE command that we have not opened table
    (table->table equal to 0) and right names is in current TableList
    object.
  */
  if (table->table)
  {
    /* temporary table is always unique */
    if (table->table && table->table->getShare()->getType() != message::Table::STANDARD)
      return 0;
    table= table->find_underlying_table(table->table);
    /*
      as far as we have table->table we have to find real TableList of
      it in underlying tables
    */
    assert(table);
  }
  d_name= table->getSchemaName();
  t_name= table->getTableName();
  t_alias= table->alias;

  for (;;)
  {
    if ((! (res= find_table_in_global_list(table_list, d_name, t_name))) ||
        ((!res->table || res->table != table->table) &&
         (!check_alias || !(my_strcasecmp(files_charset_info, t_alias, res->alias))) &&
         res->select_lex && !res->select_lex->exclude_from_table_unique_test))
      break;
    /*
      If we found entry of this table or table of SELECT which already
      processed in derived table or top select of multi-update/multi-delete
      (exclude_from_table_unique_test) or prelocking placeholder.
    */
    table_list= res->next_global;
  }
  return res;
}


void Open_tables_state::doGetTableNames(const identifier::Schema &schema_identifier,
                                        std::set<std::string>& set_of_names)
{
  for (Table *table= getTemporaryTables() ; table ; table= table->getNext())
  {
    if (schema_identifier.compare(table->getShare()->getSchemaName()))
    {
      set_of_names.insert(table->getShare()->getTableName());
    }
  }
}

void Open_tables_state::doGetTableNames(CachedDirectory &,
                                        const identifier::Schema &schema_identifier,
                                        std::set<std::string> &set_of_names)
{
  doGetTableNames(schema_identifier, set_of_names);
}

void Open_tables_state::doGetTableIdentifiers(const identifier::Schema &schema_identifier,
                                              identifier::table::vector &set_of_identifiers)
{
  for (Table *table= getTemporaryTables() ; table ; table= table->getNext())
  {
    if (schema_identifier.compare(table->getShare()->getSchemaName()))
    {
      set_of_identifiers.push_back(identifier::Table(table->getShare()->getSchemaName(),
                                                   table->getShare()->getTableName(),
                                                   table->getShare()->getPath()));
    }
  }
}

void Open_tables_state::doGetTableIdentifiers(CachedDirectory &,
                                              const identifier::Schema &schema_identifier,
                                              identifier::table::vector &set_of_identifiers)
{
  doGetTableIdentifiers(schema_identifier, set_of_identifiers);
}

bool Open_tables_state::doDoesTableExist(const identifier::Table &identifier)
{
  for (Table *table= getTemporaryTables() ; table ; table= table->getNext())
  {
    if (table->getShare()->getType() == message::Table::TEMPORARY)
    {
      if (identifier.getKey() == table->getShare()->getCacheKey())
      {
        return true;
      }
    }
  }

  return false;
}

int Open_tables_state::doGetTableDefinition(const identifier::Table &identifier,
                                            message::Table &table_proto)
{
  for (Table *table= getTemporaryTables() ; table ; table= table->getNext())
  {
    if (table->getShare()->getType() == message::Table::TEMPORARY)
    {
      if (identifier.getKey() == table->getShare()->getCacheKey())
      {
        table_proto.CopyFrom(*(table->getShare()->getTableMessage()));

        return EEXIST;
      }
    }
  }

  return ENOENT;
}

Table *Open_tables_state::find_temporary_table(const identifier::Table &identifier)
{
  for (Table *table= temporary_tables ; table ; table= table->getNext())
  {
    if (identifier.getKey() == table->getShare()->getCacheKey())
      return table;
  }

  return NULL;                               // Not a temporary table
}


/**
  Drop a temporary table.

  Try to locate the table in the list of session->temporary_tables.
  If the table is found:
  - if the table is being used by some outer statement, fail.
  - if the table is in session->locked_tables, unlock it and
  remove it from the list of locked tables. Currently only transactional
  temporary tables are present in the locked_tables list.
  - Close the temporary table, remove its .FRM
  - remove the table from the list of temporary tables

  This function is used to drop user temporary tables, as well as
  internal tables created in CREATE TEMPORARY TABLE ... SELECT
  or ALTER Table. Even though part of the work done by this function
  is redundant when the table is internal, as long as we
  link both internal and user temporary tables into the same
  session->temporary_tables list, it's impossible to tell here whether
  we're dealing with an internal or a user temporary table.

  @retval  0  the table was found and dropped successfully.
  @retval  1  the table was not found in the list of temporary tables
  of this thread
  @retval -1  the table is in use by a outer query
*/

int Open_tables_state::drop_temporary_table(const drizzled::identifier::Table &identifier)
{
  Table* table= find_temporary_table(identifier);
  if (not table)
    return 1;

  /* Table might be in use by some outer statement. */
  if (table->query_id && table->query_id != session_.getQueryId())
  {
    my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->getAlias());
    return -1;
  }
  close_temporary_table(table);
  return 0;
}


/**
  Remove all instances of table from thread's open list and
  table cache.

  @param  session     Thread context
  @param  find    Table to remove

  @note because we risk the chance of deleting the share, we can't assume that it will exist past, this should be modified once we can use a TableShare::shared_ptr here.
*/

void Session::unlink_open_table(Table *find)
{
  const identifier::Table::Key find_key(find->getShare()->getCacheKey());
  Table **prev;
  safe_mutex_assert_owner(table::Cache::mutex().native_handle());

  /*
    Note that we need to hold table::Cache::mutex() while changing the
    open_tables list. Another thread may work on it.
    (See: table::Cache::removeTable(), wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  for (prev= &open_tables.open_tables_; *prev; )
  {
    Table *list= *prev;

    if (list->getShare()->getCacheKey() == find_key)
    {
      /* Remove table from open_tables list. */
      *prev= list->getNext();

      /* Close table. */
      table::remove_table(static_cast<table::Concurrent *>(list));
    }
    else
    {
      /* Step to next entry in open_tables list. */
      prev= list->getNextPtr();
    }
  }

  // Notify any 'refresh' threads
  locking::broadcast_refresh();
}


/**
  Auxiliary routine which closes and drops open table.

  @param  session         Thread handle
  @param  table       Table object for table to be dropped
  @param  db_name     Name of database for this table
  @param  table_name  Name of this table

  @note This routine assumes that table to be closed is open only
  by calling thread so we needn't wait until other threads
  will close the table. Also unless called under implicit or
  explicit LOCK TABLES mode it assumes that table to be
  dropped is already unlocked. In the former case it will
  also remove lock on the table. But one should not rely on
  this behaviour as it may change in future.
  Currently, however, this function is never called for a
  table that was locked with LOCK TABLES.
*/

void Session::drop_open_table(Table *table, const identifier::Table &identifier)
{
  if (table->getShare()->getType())
  {
    open_tables.close_temporary_table(table);
  }
  else
  {
    boost::mutex::scoped_lock scoped_lock(table::Cache::mutex()); /* Close and drop a table (AUX routine) */
    /*
      unlink_open_table() also tells threads waiting for refresh or close
      that something has happened.
    */
    unlink_open_table(table);
    (void)plugin::StorageEngine::dropTable(*this, identifier);
  }
}


/*
  Wait for condition but allow the user to send a kill to mysqld

  SYNOPSIS
  wait_for_condition()
  session	Thread Cursor
  mutex	mutex that is currently hold that is associated with condition
  Will be unlocked on return
  cond	Condition to wait for
*/

void Session::wait_for_condition(boost::mutex &mutex, boost::condition_variable_any &cond)
{
  /* Wait until the current table is up to date */
  const char *saved_proc_info;
  mysys_var->current_mutex= &mutex;
  mysys_var->current_cond= &cond;
  saved_proc_info= get_proc_info();
  set_proc_info("Waiting for table");
  {
    /*
      We must unlock mutex first to avoid deadlock becasue conditions are
      sent to this thread by doing locks in the following order:
      lock(mysys_var->mutex)
      lock(mysys_var->current_mutex)

      One by effect of this that one can only use wait_for_condition with
      condition variables that are guranteed to not disapper (freed) even if this
      mutex is unlocked
    */
    boost::mutex::scoped_lock scopedLock(mutex, boost::adopt_lock_t());
    if (not getKilled())
    {
      cond.wait(scopedLock);
    }
  }
  boost::mutex::scoped_lock mysys_scopedLock(mysys_var->mutex);
  mysys_var->current_mutex= 0;
  mysys_var->current_cond= 0;
  set_proc_info(saved_proc_info);
}


/**
  Create and insert into table cache placeholder for table
  which will prevent its opening (or creation) (a.k.a lock
  table name).

  @param session         Thread context
  @param key         Table cache key for name to be locked
  @param key_length  Table cache key length

  @return Pointer to Table object used for name locking or 0 in
  case of failure.
*/

table::Placeholder& Session::table_cache_insert_placeholder(const drizzled::identifier::Table &arg)
{
  safe_mutex_assert_owner(table::Cache::mutex().native_handle());

  /*
    Create a table entry with the right key and with an old refresh version
  */
  identifier::Table identifier(arg.getSchemaName(), arg.getTableName(), message::Table::INTERNAL);
  table::Placeholder* table= new table::Placeholder(this, identifier);
  table::Cache::insert(table);
  return *table;
}


/**
  Obtain an exclusive name lock on the table if it is not cached
  in the table cache.

  @param      session         Thread context
  @param      db          Name of database
  @param      table_name  Name of table
  @param[out] table       Out parameter which is either:
  - set to NULL if table cache contains record for
  the table or
  - set to point to the Table instance used for
  name-locking.

  @note This function takes into account all records for table in table
  cache, even placeholders used for name-locking. This means that
  'table' parameter can be set to NULL for some situations when
  table does not really exist.

  @retval  true   Error occured (OOM)
  @retval  false  Success. 'table' parameter set according to above rules.
*/
Table* Session::lock_table_name_if_not_cached(const identifier::Table &identifier)
{
  const identifier::Table::Key &key(identifier.getKey());
  boost::mutex::scoped_lock scope_lock(table::Cache::mutex()); /* Obtain a name lock even though table is not in cache (like for create table)  */
  if (find_ptr(table::getCache(), key))
    return NULL;
  Table& table= table_cache_insert_placeholder(identifier);
  table.open_placeholder= true;
  table.setNext(open_tables.open_tables_);
  open_tables.open_tables_= &table;
  return &table;
}

/*
  Open a table.

  SYNOPSIS
  open_table()
  session                 Thread context.
  table_list          Open first table in list.
  refresh      INOUT  Pointer to memory that will be set to 1 if
  we need to close all tables and reopen them.
  If this is a NULL pointer, then the table is not
  put in the thread-open-list.
  flags               Bitmap of flags to modify how open works:
  DRIZZLE_LOCK_IGNORE_FLUSH - Open table even if
  someone has done a flush or namelock on it.
  No version number checking is done.
  DRIZZLE_OPEN_TEMPORARY_ONLY - Open only temporary
  table not the base table or view.

  IMPLEMENTATION
  Uses a cache of open tables to find a table not in use.

  If table list element for the table to be opened has "create" flag
  set and table does not exist, this function will automatically insert
  a placeholder for exclusive name lock into the open tables cache and
  will return the Table instance that corresponds to this placeholder.

  RETURN
  NULL  Open failed.  If refresh is set then one should close
  all other tables and retry the open.
#     Success. Pointer to Table object for open table.
*/


Table *Session::openTable(TableList *table_list, bool *refresh, uint32_t flags)
{
  Table *table;
  const char *alias= table_list->alias;

  /* Parsing of partitioning information from .frm needs session->lex set up. */
  assert(lex().is_lex_started);

  /* find a unused table in the open table cache */
  if (refresh)
    *refresh= false;

  /* an open table operation needs a lot of the stack space */
  if (check_stack_overrun(this, STACK_MIN_SIZE_FOR_OPEN, (unsigned char *)&alias))
    return NULL;

  if (getKilled())
    return NULL;

  identifier::Table identifier(table_list->getSchemaName(), table_list->getTableName());
  const identifier::Table::Key &key(identifier.getKey());
  table::CacheRange ppp;

  /*
    Unless requested otherwise, try to resolve this table in the list
    of temporary tables of this thread. In MySQL temporary tables
    are always thread-local and "shadow" possible base tables with the
    same name. This block implements the behaviour.
    TODO -> move this block into a separate function.
  */
  bool reset= false;
  for (table= open_tables.getTemporaryTables(); table ; table=table->getNext())
  {
    if (table->getShare()->getCacheKey() == key)
    {
      /*
        We're trying to use the same temporary table twice in a query.
        Right now we don't support this because a temporary table
        is always represented by only one Table object in Session, and
        it can not be cloned. Emit an error for an unsupported behaviour.
      */
      if (table->query_id)
      {
        my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->getAlias());
        return NULL;
      }
      table->query_id= getQueryId();
      reset= true;
      break;
    }
  }

  if (not reset)
  {
    if (flags & DRIZZLE_OPEN_TEMPORARY_ONLY)
    {
      my_error(ER_TABLE_UNKNOWN, identifier);
      return NULL;
    }

    /*
      If it's the first table from a list of tables used in a query,
      remember refresh_version (the version of open_cache state).
      If the version changes while we're opening the remaining tables,
      we will have to back off, close all the tables opened-so-far,
      and try to reopen them.

      Note-> refresh_version is currently changed only during FLUSH TABLES.
    */
    if (!open_tables.open_tables_)
    {
      open_tables.version= g_refresh_version;
    }
    else if ((open_tables.version != g_refresh_version) &&
             ! (flags & DRIZZLE_LOCK_IGNORE_FLUSH))
    {
      /* Someone did a refresh while thread was opening tables */
      if (refresh)
        *refresh= true;

      return NULL;
    }

    /*
      Non pre-locked/LOCK TABLES mode, and the table is not temporary:
      this is the normal use case.
      Now we should:
      - try to find the table in the table cache.
      - if one of the discovered Table instances is name-locked
      (table->getShare()->version == 0) back off -- we have to wait
      until no one holds a name lock on the table.
      - if there is no such Table in the name cache, read the table definition
      and insert it into the cache.
      We perform all of the above under table::Cache::mutex() which currently protects
      the open cache (also known as table cache) and table definitions stored
      on disk.
    */

    {
      boost::mutex::scoped_lock scopedLock(table::Cache::mutex());

      /*
        Actually try to find the table in the open_cache.
        The cache may contain several "Table" instances for the same
        physical table. The instances that are currently "in use" by
        some thread have their "in_use" member != NULL.
        There is no good reason for having more than one entry in the
        hash for the same physical table, except that we use this as
        an implicit "pending locks queue" - see
        wait_for_locked_table_names for details.
      */
      ppp= table::getCache().equal_range(key);

      table= NULL;
      for (table::CacheMap::const_iterator iter= ppp.first; iter != ppp.second; ++iter, table= NULL)
      {
        table= iter->second;

        if (not table->in_use)
          break;
        /*
          Here we flush tables marked for flush.
          Normally, table->getShare()->version contains the value of
          refresh_version from the moment when this table was
          (re-)opened and added to the cache.
          If since then we did (or just started) FLUSH TABLES
          statement, refresh_version has been increased.
          For "name-locked" Table instances, table->getShare()->version is set
          to 0 (see lock_table_name for details).
          In case there is a pending FLUSH TABLES or a name lock, we
          need to back off and re-start opening tables.
          If we do not back off now, we may dead lock in case of lock
          order mismatch with some other thread:
          c1-> name lock t1; -- sort of exclusive lock
          c2-> open t2;      -- sort of shared lock
          c1-> name lock t2; -- blocks
          c2-> open t1; -- blocks
        */
        if (table->needs_reopen_or_name_lock())
        {
          if (flags & DRIZZLE_LOCK_IGNORE_FLUSH)
          {
            /* Force close at once after usage */
            open_tables.version= table->getShare()->getVersion();
            continue;
          }

          /* Avoid self-deadlocks by detecting self-dependencies. */
          if (table->open_placeholder && table->in_use == this)
          {
            my_error(ER_UPDATE_TABLE_USED, MYF(0), table->getShare()->getTableName());
            return NULL;
          }

          /*
            Back off, part 1: mark the table as "unused" for the
            purpose of name-locking by setting table->db_stat to 0. Do
            that only for the tables in this thread that have an old
            table->getShare()->version (this is an optimization (?)).
            table->db_stat == 0 signals wait_for_locked_table_names
            that the tables in question are not used any more. See
            table_is_used call for details.
          */
          close_old_data_files(false, false);

          /*
            Back-off part 2: try to avoid "busy waiting" on the table:
            if the table is in use by some other thread, we suspend
            and wait till the operation is complete: when any
            operation that juggles with table->getShare()->version completes,
            it broadcasts COND_refresh condition variable.
            If 'old' table we met is in use by current thread we return
            without waiting since in this situation it's this thread
            which is responsible for broadcasting on COND_refresh
            (and this was done already in Session::close_old_data_files()).
            Good example of such situation is when we have statement
            that needs two instances of table and FLUSH TABLES comes
            after we open first instance but before we open second
            instance.
          */
          if (table->in_use != this)
          {
            /* wait_for_conditionwill unlock table::Cache::mutex() for us */
            wait_for_condition(table::Cache::mutex(), COND_refresh);
            scopedLock.release();
          }
          else
          {
            scopedLock.unlock();
          }

          /*
            There is a refresh in progress for this table.
            Signal the caller that it has to try again.
          */
          if (refresh)
            *refresh= true;

          return NULL;
        }
      }

      if (table)
      {
        table::getUnused().unlink(static_cast<table::Concurrent *>(table));
        table->in_use= this;
      }
      else
      {
        /* Insert a new Table instance into the open cache */
        /* Free cache if too big */
        table::getUnused().cull();

        if (table_list->isCreate())
        {
          identifier::Table  lock_table_identifier(table_list->getSchemaName(), table_list->getTableName(), message::Table::STANDARD);

          if (not plugin::StorageEngine::doesTableExist(*this, lock_table_identifier))
          {
            /*
              Table to be created, so we need to create placeholder in table-cache.
            */
            table= &table_cache_insert_placeholder(lock_table_identifier);
            /*
              Link placeholder to the open tables list so it will be automatically
              removed once tables are closed. Also mark it so it won't be ignored
              by other trying to take name-lock.
            */
            table->open_placeholder= true;
            table->setNext(open_tables.open_tables_);
            open_tables.open_tables_= table;

            return table ;
          }
          /* Table exists. Let us try to open it. */
        }

        /* make a new table */
        {
          table::Concurrent *new_table= new table::Concurrent;
          table= new_table;
          if (new_table->open_unireg_entry(this, alias, identifier))
          {
            delete new_table;
            return NULL;
          }
          (void)table::Cache::insert(new_table);
        }
      }
    }

    if (refresh)
    {
      table->setNext(open_tables.open_tables_); /* Link into simple list */
      open_tables.open_tables_= table;
    }
    table->reginfo.lock_type= TL_READ; /* Assume read */

  }
  assert(table->getShare()->getTableCount() > 0 || table->getShare()->getType() != message::Table::STANDARD);

  /* Fix alias if table name changes */
  if (strcmp(table->getAlias(), alias))
  {
    table->setAlias(alias);
  }

  /* These variables are also set in reopen_table() */
  table->tablenr= open_tables.current_tablenr++;
  table->used_fields= 0;
  table->const_table= 0;
  table->null_row= false;
  table->maybe_null= false;
  table->force_index= false;
  table->status=STATUS_NO_RECORD;
  table->insert_values.clear();
  /* Catch wrong handling of the auto_increment_field_not_null. */
  assert(!table->auto_increment_field_not_null);
  table->auto_increment_field_not_null= false;
  if (table->timestamp_field)
  {
    table->timestamp_field_type= table->timestamp_field->get_auto_set_type();
  }
  table->pos_in_table_list= table_list;
  table->clear_column_bitmaps();
  assert(table->key_read == 0);

  return table;
}


/**
  Close all instances of a table open by this thread and replace
  them with exclusive name-locks.

  @param session        Thread context
  @param db         Database name for the table to be closed
  @param table_name Name of the table to be closed

  @note This function assumes that if we are not under LOCK TABLES,
  then there is only one table open and locked. This means that
  the function probably has to be adjusted before it can be used
  anywhere outside ALTER Table.

  @note Must not use TableShare::table_name/db of the table being closed,
  the strings are used in a loop even after the share may be freed.
*/

void Session::close_data_files_and_morph_locks(const identifier::Table &identifier)
{
  safe_mutex_assert_owner(table::Cache::mutex().native_handle()); /* Adjust locks at the end of ALTER TABLEL */

  if (open_tables.lock)
  {
    /*
      If we are not under LOCK TABLES we should have only one table
      open and locked so it makes sense to remove the lock at once.
    */
    unlockTables(open_tables.lock);
    open_tables.lock= 0;
  }

  /*
    Note that open table list may contain a name-lock placeholder
    for target table name if we process ALTER Table ... RENAME.
    So loop below makes sense even if we are not under LOCK TABLES.
  */
  for (Table *table= open_tables.open_tables_; table ; table=table->getNext())
  {
    if (table->getShare()->getCacheKey() == identifier.getKey())
    {
      table->open_placeholder= true;
      close_handle_and_leave_table_as_lock(table);
    }
  }
}


/**
  Reopen all tables with closed data files.

  @param session         Thread context
  @param get_locks   Should we get locks after reopening tables ?
  @param mark_share_as_old  Mark share as old to protect from a impending
  global read lock.

  @note Since this function can't properly handle prelocking and
  create placeholders it should be used in very special
  situations like FLUSH TABLES or ALTER Table. In general
  case one should just repeat open_tables()/lock_tables()
  combination when one needs tables to be reopened (for
  example see openTablesLock()).

  @note One should have lock on table::Cache::mutex() when calling this.

  @return false in case of success, true - otherwise.
*/

bool Session::reopen_tables()
{
  Table *table,*next,**prev;
  Table **tables= 0;			// For locks
  Table **tables_ptr= 0;			// For locks
  bool error= false;
  const uint32_t flags= DRIZZLE_LOCK_NOTIFY_IF_NEED_REOPEN |
    DRIZZLE_LOCK_IGNORE_GLOBAL_READ_LOCK |
    DRIZZLE_LOCK_IGNORE_FLUSH;

  if (open_tables.open_tables_ == NULL)
    return false;

  safe_mutex_assert_owner(table::Cache::mutex().native_handle());
  {
    /*
      The ptr is checked later
      Do not handle locks of MERGE children.
    */
    uint32_t opens= 0;

    for (table= open_tables.open_tables_; table ; table=table->getNext())
    {
      opens++;
    }
    tables= new Table *[opens];
  }

  tables_ptr =tables;

  prev= &open_tables.open_tables_;
  for (table= open_tables.open_tables_; table ; table=next)
  {
    next= table->getNext();

    my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->getAlias());
    table::remove_table(static_cast<table::Concurrent *>(table));
    error= 1;
  }
  *prev=0;

  if (tables != tables_ptr)			// Should we get back old locks
  {
    /*
      We should always get these locks. Anyway, we must not go into
      wait_for_tables() as it tries to acquire table::Cache::mutex(), which is
      already locked.
    */

    if (not lockTables(tables, (uint32_t) (tables_ptr - tables), flags))
    {
      /*
        This case should only happen if there is a bug in the reopen logic.
        Need to issue error message to have a reply for the application.
        Not exactly what happened though, but close enough.
      */
      my_error(ER_LOCK_DEADLOCK, MYF(0));
      error=1;
    }
  }

  delete [] tables;

  locking::broadcast_refresh();

  return error;
}


/**
  Close handlers for tables in list, but leave the Table structure
  intact so that we can re-open these quickly.

  @param session           Thread context
  @param table         Head of the list of Table objects
  @param morph_locks   true  - remove locks which we have on tables being closed
  but ensure that no DML or DDL will sneak in before
  we will re-open the table (i.e. temporarily morph
  our table-level locks into name-locks).
  false - otherwise
  @param send_refresh  Should we awake waiters even if we didn't close any tables?
*/

void Session::close_old_data_files(bool morph_locks, bool send_refresh)
{
  bool found= send_refresh;

  Table *table= open_tables.open_tables_;

  for (; table ; table=table->getNext())
  {
    /*
      Reopen marked for flush.
    */
    if (table->needs_reopen_or_name_lock())
    {
      found= true;
      if (table->db_stat)
      {
        if (morph_locks)
        {
          Table *ulcktbl= table;
          if (ulcktbl->lock_count)
          {
            /*
              Wake up threads waiting for table-level lock on this table
              so they won't sneak in when we will temporarily remove our
              lock on it. This will also give them a chance to close their
              instances of this table.
            */
            abortLock(ulcktbl);
            removeLock(ulcktbl);
            ulcktbl->lock_count= 0;
          }
          if ((ulcktbl != table) && ulcktbl->db_stat)
          {
            /*
              Close the parent too. Note that parent can come later in
              the list of tables. It will then be noticed as closed and
              as a placeholder. When this happens, do not clear the
              placeholder flag. See the branch below ("***").
            */
            ulcktbl->open_placeholder= true;
            close_handle_and_leave_table_as_lock(ulcktbl);
          }
          /*
            We want to protect the table from concurrent DDL operations
            (like RENAME Table) until we will re-open and re-lock it.
          */
          table->open_placeholder= true;
        }
        close_handle_and_leave_table_as_lock(table);
      }
      else if (table->open_placeholder && !morph_locks)
      {
        /*
          We come here only in close-for-back-off scenario. So we have to
          "close" create placeholder here to avoid deadlocks (for example,
          in case of concurrent execution of CREATE TABLE t1 SELECT * FROM t2
          and RENAME Table t2 TO t1). In close-for-re-open scenario we will
          probably want to let it stay.

          Note "***": We must not enter this branch if the placeholder
          flag has been set because of a former close through a child.
          See above the comment that refers to this note.
        */
        table->open_placeholder= false;
      }
    }
  }
  if (found)
    locking::broadcast_refresh();
}


/*
  drop tables from locked list

  SYNOPSIS
  drop_locked_tables()
  session			Thread thandler
  db			Database
  table_name		Table name

  INFORMATION
  This is only called on drop tables

  The Table object for the dropped table is unlocked but still kept around
  as a name lock, which means that the table will be available for other
  thread as soon as we call unlock_table_names().
  If there is multiple copies of the table locked, all copies except
  the first, which acts as a name lock, is removed.

  RETURN
#    If table existed, return table
0	 Table was not locked
*/


Table *drop_locked_tables(Session *session, const drizzled::identifier::Table &identifier)
{
  Table *table,*next,**prev, *found= 0;
  prev= &session->open_tables.open_tables_;

  /*
    Note that we need to hold table::Cache::mutex() while changing the
    open_tables list. Another thread may work on it.
    (See: table::Cache::removeTable(), wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  for (table= session->open_tables.open_tables_; table ; table=next)
  {
    next=table->getNext();
    if (table->getShare()->getCacheKey() == identifier.getKey())
    {
      session->removeLock(table);

      if (!found)
      {
        found= table;
        /* Close engine table, but keep object around as a name lock */
        if (table->db_stat)
        {
          table->db_stat= 0;
          table->cursor->close();
        }
      }
      else
      {
        /* We already have a name lock, remove copy */
        table::remove_table(static_cast<table::Concurrent *>(table));
      }
    }
    else
    {
      *prev=table;
      prev= table->getNextPtr();
    }
  }
  *prev=0;

  if (found)
    locking::broadcast_refresh();

  return found;
}


/*
  If we have the table open, which only happens when a LOCK Table has been
  done on the table, change the lock type to a lock that will abort all
  other threads trying to get the lock.
*/

void abort_locked_tables(Session *session, const drizzled::identifier::Table &identifier)
{
  Table *table;
  for (table= session->open_tables.open_tables_; table ; table= table->getNext())
  {
    if (table->getShare()->getCacheKey() == identifier.getKey())
    {
      /* If MERGE child, forward lock handling to parent. */
      session->abortLock(table);
      assert(0);
      break;
    }
  }
}


/*
  Open all tables in list

  SYNOPSIS
  open_tables()
  session - thread Cursor
  start - list of tables in/out
  counter - number of opened tables will be return using this parameter
  flags   - bitmap of flags to modify how the tables will be open:
  DRIZZLE_LOCK_IGNORE_FLUSH - open table even if someone has
  done a flush or namelock on it.

  NOTE
  Unless we are already in prelocked mode, this function will also precache
  all SP/SFs explicitly or implicitly (via views and triggers) used by the
  query and add tables needed for their execution to table list. If resulting
  tables list will be non empty it will mark query as requiring precaching.
  Prelocked mode will be enabled for such query during lock_tables() call.

  If query for which we are opening tables is already marked as requiring
  prelocking it won't do such precaching and will simply reuse table list
  which is already built.

  RETURN
  0  - OK
  -1 - error
*/

int Session::open_tables_from_list(TableList **start, uint32_t *counter, uint32_t flags)
{
  TableList *tables= NULL;
  bool refresh;
  int result= 0;
  /* Also used for indicating that prelocking is need */
  bool safe_to_ignore_table;

  open_tables.current_tablenr= 0;
restart:
  *counter= 0;
  set_proc_info("Opening tables");

  /*
    For every table in the list of tables to open, try to find or open
    a table.
  */
  for (tables= *start; tables ;tables= tables->next_global)
  {
    safe_to_ignore_table= false;

    /*
      Ignore placeholders for derived tables. After derived tables
      processing, link to created temporary table will be put here.
      If this is derived table for view then we still want to process
      routines used by this view.
    */
    if (tables->derived)
    {
      continue;
    }
    (*counter)++;

    /*
     * Is the user authorized to see this table? Do this before we check
     * to see if it exists so that an unauthorized user cannot phish for
     * table/schema information via error messages
     */
    identifier::Table the_table(tables->getSchemaName(), tables->getTableName());
    if (not plugin::Authorization::isAuthorized(*user(), the_table))
    {
      result= -1;                               // Fatal error
      break;
    }


    /*
      Not a placeholder: must be a base table or a view, and the table is
      not opened yet. Try to open the table.
    */
    if (tables->table == NULL)
      tables->table= openTable(tables, &refresh, flags);

    if (tables->table == NULL)
    {
      if (refresh)				// Refresh in progress
      {
        /*
          We have met name-locked or old version of table. Now we have
          to close all tables which are not up to date. We also have to
          throw away set of prelocked tables (and thus close tables from
          this set that were open by now) since it possible that one of
          tables which determined its content was changed.

          Instead of implementing complex/non-robust logic mentioned
          above we simply close and then reopen all tables.

          In order to prepare for recalculation of set of prelocked tables
          we pretend that we have finished calculation which we were doing
          currently.
        */
        close_tables_for_reopen(start);
        goto restart;
      }

      if (safe_to_ignore_table)
        continue;

      result= -1;				// Fatal error
      break;
    }
    if (tables->lock_type != TL_UNLOCK)
    {
      if (tables->lock_type == TL_WRITE_DEFAULT)
        tables->table->reginfo.lock_type= update_lock_default;
      else if (tables->table->getShare()->getType() == message::Table::STANDARD)
        tables->table->reginfo.lock_type= tables->lock_type;
    }
  }

  set_proc_info(0);

  if (result && tables)
  {
    /*
      Some functions determine success as (tables->table != NULL).
      tables->table is in session->open_tables.
    */
    tables->table= NULL;
  }

  return(result);
}


/*
  Open and lock one table

  SYNOPSIS
  openTableLock()
  session			Thread Cursor
  table_list		Table to open is first table in this list
  lock_type		Lock to use for open
  lock_flags          Flags passed to mysql_lock_table

  NOTE
  This function don't do anything like SP/SF/views/triggers analysis done
  in open_tables(). It is intended for opening of only one concrete table.
  And used only in special contexts.

  RETURN VALUES
  table		Opened table
  0			Error

  If ok, the following are also set:
  table_list->lock_type 	lock_type
  table_list->table		table
*/

Table *Session::openTableLock(TableList *table_list, thr_lock_type lock_type)
{
  Table *table;
  bool refresh;

  set_proc_info("Opening table");
  open_tables.current_tablenr= 0;
  while (!(table= openTable(table_list, &refresh)) && refresh) ;

  if (table)
  {
    table_list->lock_type= lock_type;
    table_list->table=	   table;

    assert(open_tables.lock == 0);	// You must lock everything at once
    if ((table->reginfo.lock_type= lock_type) != TL_UNLOCK)
    {
      if (not (open_tables.lock= lockTables(&table_list->table, 1, 0)))
        table= NULL;
    }
  }

  set_proc_info(0);

  return table;
}

/*
  Lock all tables in list

  SYNOPSIS
  lock_tables()
  session			Thread Cursor
  tables		Tables to lock
  count		Number of opened tables
  need_reopen         Out parameter which if true indicates that some
  tables were dropped or altered during this call
  and therefore invoker should reopen tables and
  try to lock them once again (in this case
  lock_tables() will also return error).

  NOTES
  You can't call lock_tables twice, as this would break the dead-lock-free
  handling thr_lock gives us.  You most always get all needed locks at
  once.

  If query for which we are calling this function marked as requring
  prelocking, this function will do implicit LOCK TABLES and change
  session::prelocked_mode accordingly.

  RETURN VALUES
  0	ok
  -1	Error
*/

int Session::lock_tables(TableList *tables, uint32_t count, bool *need_reopen)
{
  /*
    We can't meet statement requiring prelocking if we already
    in prelocked mode.
  */
  *need_reopen= false;

  if (tables == NULL)
    return 0;

  assert(not open_tables.lock);	// You must lock everything at once

  Table** start;
  Table** ptr=start= new (mem) Table*[count];
  for (TableList* table= tables; table; table= table->next_global)
  {
    if (!table->placeholder())
      *(ptr++)= table->table;
  }
  if (not (open_tables.lock= lockTables(start, (uint32_t) (ptr - start), DRIZZLE_LOCK_NOTIFY_IF_NEED_REOPEN)))
  {
    return -1;
  }
  return 0;
}


/*
  Open a single table without table caching and don't set it in open_list

  SYNPOSIS
  open_temporary_table()
  session		  Thread object
  path	  Path (without .frm)
  db		  database
  table_name	  Table name
  link_in_list  1 if table should be linked into session->temporary_tables

NOTES:
Used by alter_table to open a temporary table and when creating
a temporary table with CREATE TEMPORARY ...

RETURN
0  Error
#  Table object
*/

Table* Session::open_temporary_table(const identifier::Table &identifier, bool link_in_list)
{
  assert(identifier.isTmp());


  table::Temporary *new_tmp_table= new table::Temporary(identifier.getType(),
                                                        identifier,
                                                        const_cast<char *>(const_cast<identifier::Table&>(identifier).getPath().c_str()),
                                                        static_cast<uint32_t>(identifier.getPath().length()));
  if (not new_tmp_table)
    return NULL;

  /*
    First open the share, and then open the table from the share we just opened.
  */
  if (new_tmp_table->getMutableShare()->open_table_def(*static_cast<Session *>(this), identifier) ||
      new_tmp_table->getMutableShare()->open_table_from_share(static_cast<Session *>(this), identifier, identifier.getTableName().c_str(),
                                                              (uint32_t) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                                                                          HA_GET_INDEX),
                                                              ha_open_options,
                                                              *new_tmp_table))
  {
    /* No need to lock share->mutex as this is not needed for tmp tables */
    delete new_tmp_table->getMutableShare();
    delete new_tmp_table;

    return 0;
  }

  new_tmp_table->reginfo.lock_type= TL_WRITE;	 // Simulate locked

  if (link_in_list)
  {
    /* growing temp list at the head */
    new_tmp_table->setNext(open_tables.temporary_tables);
    if (new_tmp_table->getNext())
    {
      new_tmp_table->getNext()->setPrev(new_tmp_table);
    }
    open_tables.temporary_tables= new_tmp_table;
    open_tables.temporary_tables->setPrev(0);
  }
  new_tmp_table->pos_in_table_list= 0;

  return new_tmp_table;
}


/*****************************************************************************
 * The following find_field_in_XXX procedures implement the core of the
 * name resolution functionality. The entry point to resolve a column name in a
 * list of tables is 'find_field_in_tables'. It calls 'find_field_in_table_ref'
 * for each table reference. In turn, depending on the type of table reference,
 * 'find_field_in_table_ref' calls one of the 'find_field_in_XXX' procedures
 * below specific for the type of table reference.
 ******************************************************************************/

/* Special Field pointers as return values of find_field_in_XXX functions. */
Field *not_found_field= (Field*) 0x1;
Field *view_ref_found= (Field*) 0x2;

static void update_field_dependencies(Session *session, Field *field, Table *table)
{
  if (session->mark_used_columns != MARK_COLUMNS_NONE)
  {
    boost::dynamic_bitset<> *current_bitmap= NULL;

    /*
      We always want to register the used keys, as the column bitmap may have
      been set for all fields (for example for view).
    */

    table->covering_keys&= field->part_of_key;
    table->merge_keys|= field->part_of_key;

    if (session->mark_used_columns == MARK_COLUMNS_READ)
    {
      current_bitmap= table->read_set;
    }
    else
    {
      current_bitmap= table->write_set;
    }

    //if (current_bitmap->testAndSet(field->position()))
    if (current_bitmap->test(field->position()))
    {
      if (session->mark_used_columns == MARK_COLUMNS_WRITE)
        session->dup_field= field;
      return;
    }
    table->used_fields++;
  }
}


/*
  Find field by name in a NATURAL/USING join table reference.

  SYNOPSIS
  find_field_in_natural_join()
  session			 [in]  thread Cursor
  table_ref            [in]  table reference to search
  name		 [in]  name of field
  length		 [in]  length of name
  ref                  [in/out] if 'name' is resolved to a view field, ref is
  set to point to the found view field
  register_tree_change [in]  true if ref is not stack variable and we
  need register changes in item tree
  actual_table         [out] the original table reference where the field
  belongs - differs from 'table_list' only for
  NATURAL/USING joins

  DESCRIPTION
  Search for a field among the result fields of a NATURAL/USING join.
  Notice that this procedure is called only for non-qualified field
  names. In the case of qualified fields, we search directly the base
  tables of a natural join.

  RETURN
  NULL        if the field was not found
  PTR         Pointer to the found Field
*/

static Field *
find_field_in_natural_join(Session *session, TableList *table_ref,
                           const char *name, uint32_t , Item **,
                           bool, TableList **actual_table)
{
  List<Natural_join_column>::iterator
    field_it(table_ref->join_columns->begin());
  Natural_join_column *nj_col, *curr_nj_col;
  Field *found_field;

  assert(table_ref->is_natural_join && table_ref->join_columns);
  assert(*actual_table == NULL);

  for (nj_col= NULL, curr_nj_col= field_it++; curr_nj_col;
       curr_nj_col= field_it++)
  {
    if (!my_strcasecmp(system_charset_info, curr_nj_col->name(), name))
    {
      if (nj_col)
      {
        my_error(ER_NON_UNIQ_ERROR, MYF(0), name, session->where());
        return NULL;
      }
      nj_col= curr_nj_col;
    }
  }
  if (!nj_col)
    return NULL;
  {
    /* This is a base table. */
    assert(nj_col->table_ref->table == nj_col->table_field->getTable());
    found_field= nj_col->table_field;
    update_field_dependencies(session, found_field, nj_col->table_ref->table);
  }

  *actual_table= nj_col->table_ref;

  return(found_field);
}


/*
  Find field by name in a base table or a view with temp table algorithm.

  SYNOPSIS
  find_field_in_table()
  session				thread Cursor
  table			table where to search for the field
  name			name of field
  length			length of name
  allow_rowid			do allow finding of "_rowid" field?
  cached_field_index_ptr	cached position in field list (used to speedup
  lookup for fields in prepared tables)

  RETURN
  0	field is not found
#	pointer to field
*/

Field *
find_field_in_table(Session *session, Table *table, const char *name, uint32_t length,
                    bool allow_rowid, uint32_t *cached_field_index_ptr)
{
  Field **field_ptr, *field;
  uint32_t cached_field_index= *cached_field_index_ptr;

  /* We assume here that table->field < NO_CACHED_FIELD_INDEX = UINT_MAX */
  if (cached_field_index < table->getShare()->sizeFields() &&
      !my_strcasecmp(system_charset_info,
                     table->getField(cached_field_index)->field_name, name))
  {
    field_ptr= table->getFields() + cached_field_index;
  }
  else if (table->getShare()->getNamedFieldSize())
  {
    field_ptr= table->getMutableShare()->getNamedField(std::string(name, length));
    if (field_ptr)
    {
      /*
        field_ptr points to field in TableShare. Convert it to the matching
        field in table
      */
      field_ptr= (table->getFields() + table->getShare()->positionFields(field_ptr));
    }
  }
  else
  {
    if (!(field_ptr= table->getFields()))
      return((Field *)0);
    for (; *field_ptr; ++field_ptr)
      if (!my_strcasecmp(system_charset_info, (*field_ptr)->field_name, name))
        break;
  }

  if (field_ptr && *field_ptr)
  {
    *cached_field_index_ptr= field_ptr - table->getFields();
    field= *field_ptr;
  }
  else
  {
    if (!allow_rowid ||
        my_strcasecmp(system_charset_info, name, "_rowid") ||
        table->getShare()->rowid_field_offset == 0)
      return((Field*) 0);
    field= table->getField(table->getShare()->rowid_field_offset-1);
  }

  update_field_dependencies(session, field, table);

  return field;
}


/*
  Find field in a table reference.

  SYNOPSIS
  find_field_in_table_ref()
  session			   [in]  thread Cursor
  table_list		   [in]  table reference to search
  name		   [in]  name of field
  length		   [in]  field length of name
  item_name              [in]  name of item if it will be created (VIEW)
  db_name                [in]  optional database name that qualifies the
  table_name             [in]  optional table name that qualifies the field
  ref		       [in/out] if 'name' is resolved to a view field, ref
  is set to point to the found view field
  allow_rowid		   [in]  do allow finding of "_rowid" field?
  cached_field_index_ptr [in]  cached position in field list (used to
  speedup lookup for fields in prepared tables)
  register_tree_change   [in]  true if ref is not stack variable and we
  need register changes in item tree
  actual_table           [out] the original table reference where the field
  belongs - differs from 'table_list' only for
  NATURAL_USING joins.

  DESCRIPTION
  Find a field in a table reference depending on the type of table
  reference. There are three types of table references with respect
  to the representation of their result columns:
  - an array of Field_translator objects for MERGE views and some
  information_schema tables,
  - an array of Field objects (and possibly a name hash) for stored
  tables,
  - a list of Natural_join_column objects for NATURAL/USING joins.
  This procedure detects the type of the table reference 'table_list'
  and calls the corresponding search routine.

  RETURN
  0			field is not found
  view_ref_found	found value in VIEW (real result is in *ref)
#			pointer to field
*/

Field *
find_field_in_table_ref(Session *session, TableList *table_list,
                        const char *name, uint32_t length,
                        const char *item_name, const char *db_name,
                        const char *table_name, Item **ref,
                        bool allow_rowid,
                        uint32_t *cached_field_index_ptr,
                        bool register_tree_change, TableList **actual_table)
{
  Field *fld= NULL;

  assert(table_list->alias);
  assert(name);
  assert(item_name);

  /*
    Check that the table and database that qualify the current field name
    are the same as the table reference we are going to search for the field.

    Exclude from the test below nested joins because the columns in a
    nested join generally originate from different tables. Nested joins
    also have no table name, except when a nested join is a merge view
    or an information schema table.

    We include explicitly table references with a 'field_translation' table,
    because if there are views over natural joins we don't want to search
    inside the view, but we want to search directly in the view columns
    which are represented as a 'field_translation'.

    TODO-> Ensure that table_name, db_name and tables->db always points to something !
  */
  if (/* Exclude nested joins. */
      (!table_list->getNestedJoin()) &&
      /* Include merge views and information schema tables. */
      /*
        Test if the field qualifiers match the table reference we plan
        to search.
      */
      table_name && table_name[0] &&
      (my_strcasecmp(table_alias_charset, table_list->alias, table_name) ||
       (db_name && db_name[0] && table_list->getSchemaName() && table_list->getSchemaName()[0] &&
        strcmp(db_name, table_list->getSchemaName()))))
    return 0;

  *actual_table= NULL;

  if (!table_list->getNestedJoin())
  {
    /* 'table_list' is a stored table. */
    assert(table_list->table);
    if ((fld= find_field_in_table(session, table_list->table, name, length,
                                  allow_rowid,
                                  cached_field_index_ptr)))
      *actual_table= table_list;
  }
  else
  {
    /*
      'table_list' is a NATURAL/USING join, or an operand of such join that
      is a nested join itself.

      If the field name we search for is qualified, then search for the field
      in the table references used by NATURAL/USING the join.
    */
    if (table_name && table_name[0])
    {
      List<TableList>::iterator it(table_list->getNestedJoin()->join_list.begin());
      TableList *table;
      while ((table= it++))
      {
        if ((fld= find_field_in_table_ref(session, table, name, length, item_name,
                                          db_name, table_name, ref,
                                          allow_rowid,
                                          cached_field_index_ptr,
                                          register_tree_change, actual_table)))
          return fld;
      }
      return NULL;
    }
    /*
      Non-qualified field, search directly in the result columns of the
      natural join. The condition of the outer IF is true for the top-most
      natural join, thus if the field is not qualified, we will search
      directly the top-most NATURAL/USING join.
    */
    fld= find_field_in_natural_join(session, table_list, name, length, ref,
                                    register_tree_change, actual_table);
  }

  if (fld)
  {
    if (session->mark_used_columns != MARK_COLUMNS_NONE)
    {
      /*
        Get rw_set correct for this field so that the Cursor
        knows that this field is involved in the query and gets
        retrieved/updated
      */
      Field *field_to_set= NULL;
      if (fld == view_ref_found)
      {
        Item *it= (*ref)->real_item();
        if (it->type() == Item::FIELD_ITEM)
          field_to_set= ((Item_field*)it)->field;
        else
        {
          if (session->mark_used_columns == MARK_COLUMNS_READ)
            it->walk(&Item::register_field_in_read_map, 1, (unsigned char *) 0);
        }
      }
      else
        field_to_set= fld;
      if (field_to_set)
      {
        Table *table= field_to_set->getTable();
        if (session->mark_used_columns == MARK_COLUMNS_READ)
          table->setReadSet(field_to_set->position());
        else
          table->setWriteSet(field_to_set->position());
      }
    }
  }
  return(fld);
}


/*
  Find field in table list.

  SYNOPSIS
  find_field_in_tables()
  session			  pointer to current thread structure
  item		  field item that should be found
  first_table           list of tables to be searched for item
  last_table            end of the list of tables to search for item. If NULL
  then search to the end of the list 'first_table'.
  ref			  if 'item' is resolved to a view field, ref is set to
  point to the found view field
  report_error	  Degree of error reporting:
  - IGNORE_ERRORS then do not report any error
  - IGNORE_EXCEPT_NON_UNIQUE report only non-unique
  fields, suppress all other errors
  - REPORT_EXCEPT_NON_UNIQUE report all other errors
  except when non-unique fields were found
  - REPORT_ALL_ERRORS
  register_tree_change  true if ref is not a stack variable and we
  to need register changes in item tree

  RETURN VALUES
  0			If error: the found field is not unique, or there are
  no sufficient access priviliges for the found field,
  or the field is qualified with non-existing table.
  not_found_field	The function was called with report_error ==
  (IGNORE_ERRORS || IGNORE_EXCEPT_NON_UNIQUE) and a
  field was not found.
  view_ref_found	View field is found, item passed through ref parameter
  found field         If a item was resolved to some field
*/

Field *
find_field_in_tables(Session *session, Item_ident *item,
                     TableList *first_table, TableList *last_table,
                     Item **ref, find_item_error_report_type report_error,
                     bool register_tree_change)
{
  Field *found=0;
  const char *db= item->db_name;
  const char *table_name= item->table_name;
  const char *name= item->field_name;
  uint32_t length=(uint32_t) strlen(name);
  char name_buff[NAME_LEN+1];
  TableList *cur_table= first_table;
  TableList *actual_table;
  bool allow_rowid;

  if (!table_name || !table_name[0])
  {
    table_name= 0;                              // For easier test
    db= 0;
  }

  allow_rowid= table_name || (cur_table && !cur_table->next_local);

  if (item->cached_table)
  {
    /*
      This shortcut is used by prepared statements. We assume that
      TableList *first_table is not changed during query execution (which
      is true for all queries except RENAME but luckily RENAME doesn't
      use fields...) so we can rely on reusing pointer to its member.
      With this optimization we also miss case when addition of one more
      field makes some prepared query ambiguous and so erroneous, but we
      accept this trade off.
    */
    TableList *table_ref= item->cached_table;
    /*
      The condition (table_ref->view == NULL) ensures that we will call
      find_field_in_table even in the case of information schema tables
      when table_ref->field_translation != NULL.
    */
    if (table_ref->table)
      found= find_field_in_table(session, table_ref->table, name, length,
                                 true, &(item->cached_field_index));
    else
      found= find_field_in_table_ref(session, table_ref, name, length, item->name,
                                     NULL, NULL, ref,
                                     true, &(item->cached_field_index),
                                     register_tree_change,
                                     &actual_table);
    if (found)
    {
      /*
        Only views fields should be marked as dependent, not an underlying
        fields.
      */
      {
        Select_Lex *current_sel= session->lex().current_select;
        Select_Lex *last_select= table_ref->select_lex;
        /*
          If the field was an outer referencee, mark all selects using this
          sub query as dependent on the outer query
        */
        if (current_sel != last_select)
          mark_select_range_as_dependent(session, last_select, current_sel,
                                         found, *ref, item);
      }
      return found;
    }
  }

  if (db)
  {
    /*
      convert database to lower case for comparison.
      We can't do this in Item_field as this would change the
      'name' of the item which may be used in the select list
    */
    strncpy(name_buff, db, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db= name_buff;
  }

  if (last_table)
    last_table= last_table->next_name_resolution_table;

  for (; cur_table != last_table ;
       cur_table= cur_table->next_name_resolution_table)
  {
    Field *cur_field= find_field_in_table_ref(session, cur_table, name, length,
                                              item->name, db, table_name, ref,
                                              allow_rowid,
                                              &(item->cached_field_index),
                                              register_tree_change,
                                              &actual_table);
    if (cur_field)
    {
      /*
        Store the original table of the field, which may be different from
        cur_table in the case of NATURAL/USING join.
      */
      item->cached_table= found ?  0 : actual_table;

      assert(session->where());
      /*
        If we found a fully qualified field we return it directly as it can't
        have duplicates.
      */
      if (db)
        return cur_field;

      if (found)
      {
        if (report_error == REPORT_ALL_ERRORS ||
            report_error == IGNORE_EXCEPT_NON_UNIQUE)
          my_error(ER_NON_UNIQ_ERROR, MYF(0),
                   table_name ? item->full_name() : name, session->where());
        return (Field*) 0;
      }
      found= cur_field;
    }
  }

  if (found)
    return found;

  /*
    If the field was qualified and there were no tables to search, issue
    an error that an unknown table was given. The situation is detected
    as follows: if there were no tables we wouldn't go through the loop
    and cur_table wouldn't be updated by the loop increment part, so it
    will be equal to the first table.
  */
  if (table_name && (cur_table == first_table) &&
      (report_error == REPORT_ALL_ERRORS ||
       report_error == REPORT_EXCEPT_NON_UNIQUE))
  {
    char buff[NAME_LEN*2+1];
    if (db && db[0])
    {
      /* We're in an error condition, two extra strlen's aren't going
       * to kill us */
      assert(strlen(db) <= NAME_LEN);
      assert(strlen(table_name) <= NAME_LEN);
      strcpy(buff, db);
      strcat(buff,".");
      strcat(buff, table_name);
      table_name=buff;
    }
    my_error(ER_UNKNOWN_TABLE, MYF(0), table_name, session->where());
  }
  else
  {
    if (report_error == REPORT_ALL_ERRORS ||
        report_error == REPORT_EXCEPT_NON_UNIQUE)
      my_error(ER_BAD_FIELD_ERROR, MYF(0), item->full_name(), session->where());
    else
      found= not_found_field;
  }
  return found;
}


/*
  Find Item in list of items (find_field_in_tables analog)

  TODO
  is it better return only counter?

  SYNOPSIS
  find_item_in_list()
  find			Item to find
  items			List of items
  counter			To return number of found item
  report_error
  REPORT_ALL_ERRORS		report errors, return 0 if error
  REPORT_EXCEPT_NOT_FOUND	Do not report 'not found' error and
  return not_found_item, report other errors,
  return 0
  IGNORE_ERRORS		Do not report errors, return 0 if error
  resolution                  Set to the resolution type if the item is found
  (it says whether the item is resolved
  against an alias name,
  or as a field name without alias,
  or as a field hidden by alias,
  or ignoring alias)

  RETURN VALUES
  0			Item is not found or item is not unique,
  error message is reported
  not_found_item	Function was called with
  report_error == REPORT_EXCEPT_NOT_FOUND and
  item was not found. No error message was reported
  found field
*/

/* Special Item pointer to serve as a return value from find_item_in_list(). */
Item **not_found_item= (Item**) 0x1;


Item **
find_item_in_list(Session *session,
                  Item *find, List<Item> &items, uint32_t *counter,
                  find_item_error_report_type report_error,
                  enum_resolution_type *resolution)
{
  List<Item>::iterator li(items.begin());
  Item **found=0, **found_unaliased= 0, *item;
  const char *db_name=0;
  const char *field_name=0;
  const char *table_name=0;
  bool found_unaliased_non_uniq= 0;
  /*
    true if the item that we search for is a valid name reference
    (and not an item that happens to have a name).
  */
  bool is_ref_by_name= 0;
  uint32_t unaliased_counter= 0;

  *resolution= NOT_RESOLVED;

  is_ref_by_name= (find->type() == Item::FIELD_ITEM  ||
                   find->type() == Item::REF_ITEM);
  if (is_ref_by_name)
  {
    field_name= ((Item_ident*) find)->field_name;
    table_name= ((Item_ident*) find)->table_name;
    db_name=    ((Item_ident*) find)->db_name;
  }

  for (uint32_t i= 0; (item=li++); i++)
  {
    if (field_name && item->real_item()->type() == Item::FIELD_ITEM)
    {
      Item_ident *item_field= (Item_ident*) item;

      /*
        In case of group_concat() with ORDER BY condition in the QUERY
        item_field can be field of temporary table without item name
        (if this field created from expression argument of group_concat()),
        => we have to check presence of name before compare
      */
      if (!item_field->name)
        continue;

      if (table_name)
      {
        /*
          If table name is specified we should find field 'field_name' in
          table 'table_name'. According to SQL-standard we should ignore
          aliases in this case.

          Since we should NOT prefer fields from the select list over
          other fields from the tables participating in this select in
          case of ambiguity we have to do extra check outside this function.

          We use strcmp for table names and database names as these may be
          case sensitive. In cases where they are not case sensitive, they
          are always in lower case.

          item_field->field_name and item_field->table_name can be 0x0 if
          item is not fix_field()'ed yet.
        */
        if (item_field->field_name && item_field->table_name &&
            !my_strcasecmp(system_charset_info, item_field->field_name,
                           field_name) &&
            !my_strcasecmp(table_alias_charset, item_field->table_name,
                           table_name) &&
            (!db_name || (item_field->db_name &&
                          !strcmp(item_field->db_name, db_name))))
        {
          if (found_unaliased)
          {
            if ((*found_unaliased)->eq(item, 0))
              continue;
            /*
              Two matching fields in select list.
              We already can bail out because we are searching through
              unaliased names only and will have duplicate error anyway.
            */
            if (report_error != IGNORE_ERRORS)
              my_error(ER_NON_UNIQ_ERROR, MYF(0),
                       find->full_name(), session->where());
            return (Item**) 0;
          }
          found_unaliased= li.ref();
          unaliased_counter= i;
          *resolution= RESOLVED_IGNORING_ALIAS;
          if (db_name)
            break;                              // Perfect match
        }
      }
      else
      {
        int fname_cmp= my_strcasecmp(system_charset_info,
                                     item_field->field_name,
                                     field_name);
        if (!my_strcasecmp(system_charset_info,
                           item_field->name,field_name))
        {
          /*
            If table name was not given we should scan through aliases
            and non-aliased fields first. We are also checking unaliased
            name of the field in then next  else-if, to be able to find
            instantly field (hidden by alias) if no suitable alias or
            non-aliased field was found.
          */
          if (found)
          {
            if ((*found)->eq(item, 0))
              continue;                           // Same field twice
            if (report_error != IGNORE_ERRORS)
              my_error(ER_NON_UNIQ_ERROR, MYF(0),
                       find->full_name(), session->where());
            return (Item**) 0;
          }
          found= li.ref();
          *counter= i;
          *resolution= fname_cmp ? RESOLVED_AGAINST_ALIAS:
            RESOLVED_WITH_NO_ALIAS;
        }
        else if (!fname_cmp)
        {
          /*
            We will use non-aliased field or react on such ambiguities only if
            we won't be able to find aliased field.
            Again if we have ambiguity with field outside of select list
            we should prefer fields from select list.
          */
          if (found_unaliased)
          {
            if ((*found_unaliased)->eq(item, 0))
              continue;                           // Same field twice
            found_unaliased_non_uniq= 1;
          }
          found_unaliased= li.ref();
          unaliased_counter= i;
        }
      }
    }
    else if (!table_name)
    {
      if (is_ref_by_name && find->name && item->name &&
          !my_strcasecmp(system_charset_info,item->name,find->name))
      {
        found= li.ref();
        *counter= i;
        *resolution= RESOLVED_AGAINST_ALIAS;
        break;
      }
      else if (find->eq(item,0))
      {
        found= li.ref();
        *counter= i;
        *resolution= RESOLVED_IGNORING_ALIAS;
        break;
      }
    }
  }
  if (!found)
  {
    if (found_unaliased_non_uniq)
    {
      if (report_error != IGNORE_ERRORS)
        my_error(ER_NON_UNIQ_ERROR, MYF(0),
                 find->full_name(), session->where());
      return (Item **) 0;
    }
    if (found_unaliased)
    {
      found= found_unaliased;
      *counter= unaliased_counter;
      *resolution= RESOLVED_BEHIND_ALIAS;
    }
  }
  if (found)
    return found;
  if (report_error != REPORT_EXCEPT_NOT_FOUND)
  {
    if (report_error == REPORT_ALL_ERRORS)
      my_error(ER_BAD_FIELD_ERROR, MYF(0),
               find->full_name(), session->where());
    return (Item **) 0;
  }
  else
    return (Item **) not_found_item;
}


/*
  Test if a string is a member of a list of strings.

  SYNOPSIS
  test_if_string_in_list()
  find      the string to look for
  str_list  a list of strings to be searched

  DESCRIPTION
  Sequentially search a list of strings for a string, and test whether
  the list contains the same string.

  RETURN
  true  if find is in str_list
  false otherwise
*/

static bool
test_if_string_in_list(const char *find, List<String> *str_list)
{
  List<String>::iterator str_list_it(str_list->begin());
  String *curr_str;
  size_t find_length= strlen(find);
  while ((curr_str= str_list_it++))
  {
    if (find_length != curr_str->length())
      continue;
    if (!my_strcasecmp(system_charset_info, find, curr_str->ptr()))
      return true;
  }
  return false;
}


/*
  Create a new name resolution context for an item so that it is
  being resolved in a specific table reference.

  SYNOPSIS
  set_new_item_local_context()
  session        pointer to current thread
  item       item for which new context is created and set
  table_ref  table ref where an item showld be resolved

  DESCRIPTION
  Create a new name resolution context for an item, so that the item
  is resolved only the supplied 'table_ref'.

  RETURN
  false  if all OK
  true   otherwise
*/

static void set_new_item_local_context(Session *session, Item_ident *item, TableList *table_ref)
{
  Name_resolution_context* context= new (session->mem_root) Name_resolution_context;
  context->init();
  context->first_name_resolution_table= context->last_name_resolution_table= table_ref;
  item->context= context;
}


/*
  Find and mark the common columns of two table references.

  SYNOPSIS
  mark_common_columns()
  session                [in] current thread
  table_ref_1        [in] the first (left) join operand
  table_ref_2        [in] the second (right) join operand
  using_fields       [in] if the join is JOIN...USING - the join columns,
  if NATURAL join, then NULL
  found_using_fields [out] number of fields from the USING clause that were
  found among the common fields

  DESCRIPTION
  The procedure finds the common columns of two relations (either
  tables or intermediate join results), and adds an equi-join condition
  to the ON clause of 'table_ref_2' for each pair of matching columns.
  If some of table_ref_XXX represents a base table or view, then we
  create new 'Natural_join_column' instances for each column
  reference and store them in the 'join_columns' of the table
  reference.

  IMPLEMENTATION
  The procedure assumes that store_natural_using_join_columns() was
  called for the previous level of NATURAL/USING joins.

  RETURN
  true   error when some common column is non-unique, or out of memory
  false  OK
*/

static bool
mark_common_columns(Session *session, TableList *table_ref_1, TableList *table_ref_2,
                    List<String> *using_fields, uint32_t *found_using_fields)
{
  Field_iterator_table_ref it_1, it_2;
  Natural_join_column *nj_col_1, *nj_col_2;
  bool first_outer_loop= true;
  /*
    Leaf table references to which new natural join columns are added
    if the leaves are != NULL.
  */
  TableList *leaf_1= (table_ref_1->getNestedJoin() &&
                      ! table_ref_1->is_natural_join) ?
    NULL : table_ref_1;
  TableList *leaf_2= (table_ref_2->getNestedJoin() &&
                      ! table_ref_2->is_natural_join) ?
    NULL : table_ref_2;

  *found_using_fields= 0;

  for (it_1.set(table_ref_1); !it_1.end_of_fields(); it_1.next())
  {
    bool found= false;
    const char *field_name_1;
    /* true if field_name_1 is a member of using_fields */
    bool is_using_column_1;
    if (!(nj_col_1= it_1.get_or_create_column_ref(leaf_1)))
      return true;
    field_name_1= nj_col_1->name();
    is_using_column_1= using_fields &&
      test_if_string_in_list(field_name_1, using_fields);

    /*
      Find a field with the same name in table_ref_2.

      Note that for the second loop, it_2.set() will iterate over
      table_ref_2->join_columns and not generate any new elements or
      lists.
    */
    nj_col_2= NULL;
    for (it_2.set(table_ref_2); !it_2.end_of_fields(); it_2.next())
    {
      Natural_join_column *cur_nj_col_2;
      const char *cur_field_name_2;
      if (!(cur_nj_col_2= it_2.get_or_create_column_ref(leaf_2)))
        return true;
      cur_field_name_2= cur_nj_col_2->name();

      /*
        Compare the two columns and check for duplicate common fields.
        A common field is duplicate either if it was already found in
        table_ref_2 (then found == true), or if a field in table_ref_2
        was already matched by some previous field in table_ref_1
        (then cur_nj_col_2->is_common == true).
        Note that it is too early to check the columns outside of the
        USING list for ambiguity because they are not actually "referenced"
        here. These columns must be checked only on unqualified reference
        by name (e.g. in SELECT list).
      */
      if (!my_strcasecmp(system_charset_info, field_name_1, cur_field_name_2))
      {
        if (cur_nj_col_2->is_common ||
            (found && (!using_fields || is_using_column_1)))
        {
          my_error(ER_NON_UNIQ_ERROR, MYF(0), field_name_1, session->where());
          return true;
        }
        nj_col_2= cur_nj_col_2;
        found= true;
      }
    }
    if (first_outer_loop && leaf_2)
    {
      /*
        Make sure that the next inner loop "knows" that all columns
        are materialized already.
      */
      leaf_2->is_join_columns_complete= true;
      first_outer_loop= false;
    }
    if (!found)
      continue;                                 // No matching field

    /*
      field_1 and field_2 have the same names. Check if they are in the USING
      clause (if present), mark them as common fields, and add a new
      equi-join condition to the ON clause.
    */
    if (nj_col_2 && (!using_fields ||is_using_column_1))
    {
      Item *item_1=   nj_col_1->create_item(session);
      Item *item_2=   nj_col_2->create_item(session);
      Field *field_1= nj_col_1->field();
      Field *field_2= nj_col_2->field();
 
      if (!item_1 || !item_2)
        return true; // out of memory

      /*
        In the case of no_wrap_view_item == 0, the created items must be
        of sub-classes of Item_ident.
      */
      assert(item_1->type() == Item::FIELD_ITEM ||
             item_1->type() == Item::REF_ITEM);
      assert(item_2->type() == Item::FIELD_ITEM ||
             item_2->type() == Item::REF_ITEM);

      /*
        We need to cast item_1,2 to Item_ident, because we need to hook name
        resolution contexts specific to each item.
      */
      Item_ident* item_ident_1= (Item_ident*) item_1;
      Item_ident* item_ident_2= (Item_ident*) item_2;
      /*
        Create and hook special name resolution contexts to each item in the
        new join condition . We need this to both speed-up subsequent name
        resolution of these items, and to enable proper name resolution of
        the items during the execute phase of PS.
      */
      set_new_item_local_context(session, item_ident_1, nj_col_1->table_ref);
      set_new_item_local_context(session, item_ident_2, nj_col_2->table_ref);

      Item_func_eq* eq_cond= new Item_func_eq(item_ident_1, item_ident_2);

      /*
        Add the new equi-join condition to the ON clause. Notice that
        fix_fields() is applied to all ON conditions in setup_conds()
        so we don't do it here.
      */
      add_join_on((table_ref_1->outer_join & JOIN_TYPE_RIGHT ?
                   table_ref_1 : table_ref_2),
                  eq_cond);

      nj_col_1->is_common= nj_col_2->is_common= true;

      if (field_1)
      {
        Table *table_1= nj_col_1->table_ref->table;
        /* Mark field_1 used for table cache. */
        table_1->setReadSet(field_1->position());
        table_1->covering_keys&= field_1->part_of_key;
        table_1->merge_keys|= field_1->part_of_key;
      }
      if (field_2)
      {
        Table *table_2= nj_col_2->table_ref->table;
        /* Mark field_2 used for table cache. */
        table_2->setReadSet(field_2->position());
        table_2->covering_keys&= field_2->part_of_key;
        table_2->merge_keys|= field_2->part_of_key;
      }

      if (using_fields != NULL)
        ++(*found_using_fields);
    }
  }
  if (leaf_1)
    leaf_1->is_join_columns_complete= true;

  /*
    Everything is OK.
    Notice that at this point there may be some column names in the USING
    clause that are not among the common columns. This is an SQL error and
    we check for this error in store_natural_using_join_columns() when
    (found_using_fields < length(join_using_fields)).
  */
  return false;
}



/*
  Materialize and store the row type of NATURAL/USING join.

  SYNOPSIS
  store_natural_using_join_columns()
  session                current thread
  natural_using_join the table reference of the NATURAL/USING join
  table_ref_1        the first (left) operand (of a NATURAL/USING join).
  table_ref_2        the second (right) operand (of a NATURAL/USING join).
  using_fields       if the join is JOIN...USING - the join columns,
  if NATURAL join, then NULL
  found_using_fields number of fields from the USING clause that were
  found among the common fields

  DESCRIPTION
  Iterate over the columns of both join operands and sort and store
  all columns into the 'join_columns' list of natural_using_join
  where the list is formed by three parts:
part1: The coalesced columns of table_ref_1 and table_ref_2,
sorted according to the column order of the first table.
part2: The other columns of the first table, in the order in
which they were defined in CREATE TABLE.
part3: The other columns of the second table, in the order in
which they were defined in CREATE TABLE.
Time complexity - O(N1+N2), where Ni = length(table_ref_i).

IMPLEMENTATION
The procedure assumes that mark_common_columns() has been called
for the join that is being processed.

RETURN
true    error: Some common column is ambiguous
false   OK
*/

static bool
store_natural_using_join_columns(Session *session,
                                 TableList *natural_using_join,
                                 TableList *table_ref_1,
                                 TableList *table_ref_2,
                                 List<String> *using_fields,
                                 uint32_t found_using_fields)
{
  Field_iterator_table_ref it_1, it_2;
  Natural_join_column *nj_col_1, *nj_col_2;

  assert(!natural_using_join->join_columns);

  List<Natural_join_column>* non_join_columns= new List<Natural_join_column>;
  natural_using_join->join_columns= new List<Natural_join_column>;

  /* Append the columns of the first join operand. */
  for (it_1.set(table_ref_1); !it_1.end_of_fields(); it_1.next())
  {
    nj_col_1= it_1.get_natural_column_ref();
    if (nj_col_1->is_common)
    {
      natural_using_join->join_columns->push_back(nj_col_1);
      /* Reset the common columns for the next call to mark_common_columns. */
      nj_col_1->is_common= false;
    }
    else
      non_join_columns->push_back(nj_col_1);
  }

  /*
    Check that all columns in the USING clause are among the common
    columns. If this is not the case, report the first one that was
    not found in an error.
  */
  if (using_fields && found_using_fields < using_fields->size())
  {
    List<String>::iterator using_fields_it(using_fields->begin());
    while (String* using_field_name= using_fields_it++)
    {
      const char *using_field_name_ptr= using_field_name->c_ptr();
      List<Natural_join_column>::iterator  it(natural_using_join->join_columns->begin());
      for (;;)
      {
        /* If reached the end of fields, and none was found, report error. */
        Natural_join_column* common_field= it++;
        if (not common_field)
        {
          my_error(ER_BAD_FIELD_ERROR, MYF(0), using_field_name_ptr, session->where());
          return true;
        }
        if (!my_strcasecmp(system_charset_info, common_field->name(), using_field_name_ptr))
          break;                                // Found match
      }
    }
  }

  /* Append the non-equi-join columns of the second join operand. */
  for (it_2.set(table_ref_2); !it_2.end_of_fields(); it_2.next())
  {
    nj_col_2= it_2.get_natural_column_ref();
    if (!nj_col_2->is_common)
      non_join_columns->push_back(nj_col_2);
    else
    {
      /* Reset the common columns for the next call to mark_common_columns. */
      nj_col_2->is_common= false;
    }
  }

  if (non_join_columns->size() > 0)
    natural_using_join->join_columns->concat(non_join_columns);
  natural_using_join->is_join_columns_complete= true;

  return false;
}


/*
  Precompute and store the row types of the top-most NATURAL/USING joins.

  SYNOPSIS
  store_top_level_join_columns()
  session            current thread
  table_ref      nested join or table in a FROM clause
  left_neighbor  neighbor table reference to the left of table_ref at the
  same level in the join tree
  right_neighbor neighbor table reference to the right of table_ref at the
  same level in the join tree

  DESCRIPTION
  The procedure performs a post-order traversal of a nested join tree
  and materializes the row types of NATURAL/USING joins in a
  bottom-up manner until it reaches the TableList elements that
  represent the top-most NATURAL/USING joins. The procedure should be
  applied to each element of Select_Lex::top_join_list (i.e. to each
  top-level element of the FROM clause).

  IMPLEMENTATION
  Notice that the table references in the list nested_join->join_list
  are in reverse order, thus when we iterate over it, we are moving
  from the right to the left in the FROM clause.

  RETURN
  true   Error
  false  OK
*/

static bool
store_top_level_join_columns(Session *session, TableList *table_ref,
                             TableList *left_neighbor,
                             TableList *right_neighbor)
{
  /* Call the procedure recursively for each nested table reference. */
  if (table_ref->getNestedJoin())
  {
    List<TableList>::iterator nested_it(table_ref->getNestedJoin()->join_list.begin());
    TableList *same_level_left_neighbor= nested_it++;
    TableList *same_level_right_neighbor= NULL;
    /* Left/right-most neighbors, possibly at higher levels in the join tree. */
    TableList *real_left_neighbor, *real_right_neighbor;

    while (same_level_left_neighbor)
    {
      TableList *cur_table_ref= same_level_left_neighbor;
      same_level_left_neighbor= nested_it++;
      /*
        The order of RIGHT JOIN operands is reversed in 'join list' to
        transform it into a LEFT JOIN. However, in this procedure we need
        the join operands in their lexical order, so below we reverse the
        join operands. Notice that this happens only in the first loop,
        and not in the second one, as in the second loop
        same_level_left_neighbor == NULL.
        This is the correct behavior, because the second loop sets
        cur_table_ref reference correctly after the join operands are
        swapped in the first loop.
      */
      if (same_level_left_neighbor &&
          cur_table_ref->outer_join & JOIN_TYPE_RIGHT)
      {
        /* This can happen only for JOIN ... ON. */
        assert(table_ref->getNestedJoin()->join_list.size() == 2);
        std::swap(same_level_left_neighbor, cur_table_ref);
      }

      /*
        Pick the parent's left and right neighbors if there are no immediate
        neighbors at the same level.
      */
      real_left_neighbor=  (same_level_left_neighbor) ?
        same_level_left_neighbor : left_neighbor;
      real_right_neighbor= (same_level_right_neighbor) ?
        same_level_right_neighbor : right_neighbor;

      if (cur_table_ref->getNestedJoin() &&
          store_top_level_join_columns(session, cur_table_ref, real_left_neighbor, real_right_neighbor))
        return true;
      same_level_right_neighbor= cur_table_ref;
    }
  }

  /*
    If this is a NATURAL/USING join, materialize its result columns and
    convert to a JOIN ... ON.
  */
  if (table_ref->is_natural_join)
  {
    assert(table_ref->getNestedJoin() &&
           table_ref->getNestedJoin()->join_list.size() == 2);
    List<TableList>::iterator operand_it(table_ref->getNestedJoin()->join_list.begin());
    /*
      Notice that the order of join operands depends on whether table_ref
      represents a LEFT or a RIGHT join. In a RIGHT join, the operands are
      in inverted order.
    */
    TableList *table_ref_2= operand_it++; /* Second NATURAL join operand.*/
    TableList *table_ref_1= operand_it++; /* First NATURAL join operand. */
    List<String> *using_fields= table_ref->join_using_fields;
    uint32_t found_using_fields;

    /*
      The two join operands were interchanged in the parser, change the order
      back for 'mark_common_columns'.
    */
    if (table_ref_2->outer_join & JOIN_TYPE_RIGHT)
      std::swap(table_ref_1, table_ref_2);
    if (mark_common_columns(session, table_ref_1, table_ref_2,
                            using_fields, &found_using_fields))
      return true;

    /*
      Swap the join operands back, so that we pick the columns of the second
      one as the coalesced columns. In this way the coalesced columns are the
      same as of an equivalent LEFT JOIN.
    */
    if (table_ref_1->outer_join & JOIN_TYPE_RIGHT)
      std::swap(table_ref_1, table_ref_2);
    if (store_natural_using_join_columns(session, table_ref, table_ref_1,
                                         table_ref_2, using_fields,
                                         found_using_fields))
      return true;

    /*
      Change NATURAL JOIN to JOIN ... ON. We do this for both operands
      because either one of them or the other is the one with the
      natural join flag because RIGHT joins are transformed into LEFT,
      and the two tables may be reordered.
    */
    table_ref_1->natural_join= table_ref_2->natural_join= NULL;

    /* Add a true condition to outer joins that have no common columns. */
    if (table_ref_2->outer_join &&
        !table_ref_1->on_expr && !table_ref_2->on_expr)
      table_ref_2->on_expr= new Item_int((int64_t) 1,1);   /* Always true. */

    /* Change this table reference to become a leaf for name resolution. */
    if (left_neighbor)
    {
      TableList *last_leaf_on_the_left;
      last_leaf_on_the_left= left_neighbor->last_leaf_for_name_resolution();
      last_leaf_on_the_left->next_name_resolution_table= table_ref;
    }
    if (right_neighbor)
    {
      TableList *first_leaf_on_the_right;
      first_leaf_on_the_right= right_neighbor->first_leaf_for_name_resolution();
      table_ref->next_name_resolution_table= first_leaf_on_the_right;
    }
    else
      table_ref->next_name_resolution_table= NULL;
  }
  return false;
}


/*
  Compute and store the row types of the top-most NATURAL/USING joins
  in a FROM clause.

  SYNOPSIS
  setup_natural_join_row_types()
  session          current thread
  from_clause  list of top-level table references in a FROM clause

  DESCRIPTION
  Apply the procedure 'store_top_level_join_columns' to each of the
  top-level table referencs of the FROM clause. Adjust the list of tables
  for name resolution - context->first_name_resolution_table to the
  top-most, lef-most NATURAL/USING join.

  IMPLEMENTATION
  Notice that the table references in 'from_clause' are in reverse
  order, thus when we iterate over it, we are moving from the right
  to the left in the FROM clause.

  RETURN
  true   Error
  false  OK
*/
static bool setup_natural_join_row_types(Session *session,
                                         List<TableList> *from_clause,
                                         Name_resolution_context *context)
{
  session->setWhere("from clause");
  if (from_clause->size() == 0)
    return false; /* We come here in the case of UNIONs. */

  List<TableList>::iterator table_ref_it(from_clause->begin());
  TableList *table_ref; /* Current table reference. */
  /* Table reference to the left of the current. */
  TableList *left_neighbor;
  /* Table reference to the right of the current. */
  TableList *right_neighbor= NULL;

  /* Note that tables in the list are in reversed order */
  for (left_neighbor= table_ref_it++; left_neighbor ; )
  {
    table_ref= left_neighbor;
    left_neighbor= table_ref_it++;
    if (store_top_level_join_columns(session, table_ref, left_neighbor, right_neighbor))
      return true;
    if (left_neighbor)
    {
      TableList *first_leaf_on_the_right;
      first_leaf_on_the_right= table_ref->first_leaf_for_name_resolution();
      left_neighbor->next_name_resolution_table= first_leaf_on_the_right;
    }
    right_neighbor= table_ref;
  }

  /*
    Store the top-most, left-most NATURAL/USING join, so that we start
    the search from that one instead of context->table_list. At this point
    right_neighbor points to the left-most top-level table reference in the
    FROM clause.
  */
  assert(right_neighbor);
  context->first_name_resolution_table=
    right_neighbor->first_leaf_for_name_resolution();

  return false;
}


/****************************************************************************
 ** Expand all '*' in given fields
 ****************************************************************************/

int setup_wild(Session *session, List<Item> &fields,
               List<Item> *sum_func_list,
               uint32_t wild_num)
{
  if (!wild_num)
    return 0;

  Item *item;
  List<Item>::iterator it(fields.begin());

  session->lex().current_select->cur_pos_in_select_list= 0;
  while (wild_num && (item= it++))
  {
    if (item->type() == Item::FIELD_ITEM &&
        ((Item_field*) item)->field_name &&
        ((Item_field*) item)->field_name[0] == '*' &&
        !((Item_field*) item)->field)
    {
      uint32_t elem= fields.size();
      bool any_privileges= ((Item_field *) item)->any_privileges;
      Item_subselect *subsel= session->lex().current_select->master_unit()->item;
      if (subsel &&
          subsel->substype() == Item_subselect::EXISTS_SUBS)
      {
        /*
          It is EXISTS(SELECT * ...) and we can replace * by any constant.

          Item_int do not need fix_fields() because it is basic constant.
        */
        it.replace(new Item_int("Not_used", (int64_t) 1,
                                MY_INT64_NUM_DECIMAL_DIGITS));
      }
      else if (insert_fields(session, ((Item_field*) item)->context,
                             ((Item_field*) item)->db_name,
                             ((Item_field*) item)->table_name, &it,
                             any_privileges))
      {
        return -1;
      }
      if (sum_func_list)
      {
        /*
          sum_func_list is a list that has the fields list as a tail.
          Because of this we have to update the element count also for this
          list after expanding the '*' entry.
        */
        sum_func_list->set_size(sum_func_list->size() + fields.size() - elem);
      }
      wild_num--;
    }
    else
      session->lex().current_select->cur_pos_in_select_list++;
  }
  session->lex().current_select->cur_pos_in_select_list= UNDEF_POS;

  return 0;
}

/****************************************************************************
 ** Check that all given fields exists and fill struct with current data
 ****************************************************************************/

bool setup_fields(Session *session, Item **ref_pointer_array,
                  List<Item> &fields, enum_mark_columns mark_used_columns,
                  List<Item> *sum_func_list, bool allow_sum_func)
{
  Item *item;
  enum_mark_columns save_mark_used_columns= session->mark_used_columns;
  nesting_map save_allow_sum_func= session->lex().allow_sum_func;
  List<Item>::iterator it(fields.begin());
  bool save_is_item_list_lookup;

  session->mark_used_columns= mark_used_columns;
  if (allow_sum_func)
    session->lex().allow_sum_func|= 1 << session->lex().current_select->nest_level;
  session->setWhere(Session::DEFAULT_WHERE);
  save_is_item_list_lookup= session->lex().current_select->is_item_list_lookup;
  session->lex().current_select->is_item_list_lookup= 0;

  /*
    To prevent fail on forward lookup we fill it with zerows,
    then if we got pointer on zero after find_item_in_list we will know
    that it is forward lookup.

    There is other way to solve problem: fill array with pointers to list,
    but it will be slower.

    TODO-> remove it when (if) we made one list for allfields and ref_pointer_array
  */
  if (ref_pointer_array)
  {
    memset(ref_pointer_array, 0, sizeof(Item *) * fields.size());
  }

  Item **ref= ref_pointer_array;
  session->lex().current_select->cur_pos_in_select_list= 0;
  while ((item= it++))
  {
    if ((!item->fixed && item->fix_fields(session, it.ref())) || (item= *(it.ref()))->check_cols(1))
    {
      session->lex().current_select->is_item_list_lookup= save_is_item_list_lookup;
      session->lex().allow_sum_func= save_allow_sum_func;
      session->mark_used_columns= save_mark_used_columns;
      return true;
    }
    if (ref)
      *(ref++)= item;
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM &&
        sum_func_list)
      item->split_sum_func(session, ref_pointer_array, *sum_func_list);
    session->used_tables|= item->used_tables();
    session->lex().current_select->cur_pos_in_select_list++;
  }
  session->lex().current_select->is_item_list_lookup= save_is_item_list_lookup;
  session->lex().current_select->cur_pos_in_select_list= UNDEF_POS;

  session->lex().allow_sum_func= save_allow_sum_func;
  session->mark_used_columns= save_mark_used_columns;
  return(test(session->is_error()));
}


/*
  make list of leaves of join table tree

  SYNOPSIS
  make_leaves_list()
  list    pointer to pointer on list first element
  tables  table list

  RETURN pointer on pointer to next_leaf of last element
*/

static TableList **make_leaves_list(TableList **list, TableList *tables)
{
  for (TableList *table= tables; table; table= table->next_local)
  {
    {
      *list= table;
      list= &table->next_leaf;
    }
  }
  return list;
}

/*
  prepare tables

  SYNOPSIS
  setup_tables()
  session		  Thread Cursor
  context       name resolution contest to setup table list there
  from_clause   Top-level list of table references in the FROM clause
  tables	  Table list (select_lex->table_list)
  leaves        List of join table leaves list (select_lex->leaf_tables)
  refresh       It is onle refresh for subquery
  select_insert It is SELECT ... INSERT command

  NOTE
  Check also that the 'used keys' and 'ignored keys' exists and set up the
  table structure accordingly.
  Create a list of leaf tables. For queries with NATURAL/USING JOINs,
  compute the row types of the top most natural/using join table references
  and link these into a list of table references for name resolution.

  This has to be called for all tables that are used by items, as otherwise
  table->map is not set and all Item_field will be regarded as const items.

  RETURN
  false ok;  In this case *map will includes the chosen index
  true  error
*/

bool setup_tables(Session *session, Name_resolution_context *context,
                  List<TableList> *from_clause, TableList *tables,
                  TableList **leaves, bool select_insert)
{
  uint32_t tablenr= 0;

  assert ((select_insert && !tables->next_name_resolution_table) || !tables ||
          (context->table_list && context->first_name_resolution_table));
  /*
    this is used for INSERT ... SELECT.
    For select we setup tables except first (and its underlying tables)
  */
  TableList *first_select_table= (select_insert ?  tables->next_local: NULL);

  if (!(*leaves))
    make_leaves_list(leaves, tables);

  TableList *table_list;
  for (table_list= *leaves;
       table_list;
       table_list= table_list->next_leaf, tablenr++)
  {
    Table *table= table_list->table;
    table->pos_in_table_list= table_list;
    if (first_select_table &&
        table_list->top_table() == first_select_table)
    {
      /* new counting for SELECT of INSERT ... SELECT command */
      first_select_table= 0;
      tablenr= 0;
    }
    table->setup_table_map(table_list, tablenr);
    if (table_list->process_index_hints(table))
      return 1;
  }
  if (tablenr > MAX_TABLES)
  {
    my_error(ER_TOO_MANY_TABLES,MYF(0),MAX_TABLES);
    return 1;
  }

  /* Precompute and store the row types of NATURAL/USING joins. */
  if (setup_natural_join_row_types(session, from_clause, context))
    return 1;

  return 0;
}


/*
  prepare tables and check access for the view tables

  SYNOPSIS
  setup_tables_and_check_view_access()
  session		  Thread Cursor
  context       name resolution contest to setup table list there
  from_clause   Top-level list of table references in the FROM clause
  tables	  Table list (select_lex->table_list)
  conds	  Condition of current SELECT (can be changed by VIEW)
  leaves        List of join table leaves list (select_lex->leaf_tables)
  refresh       It is onle refresh for subquery
  select_insert It is SELECT ... INSERT command
  want_access   what access is needed

  NOTE
  a wrapper for check_tables that will also check the resulting
  table leaves list for access to all the tables that belong to a view

  RETURN
  false ok;  In this case *map will include the chosen index
  true  error
*/
bool setup_tables_and_check_access(Session *session,
                                   Name_resolution_context *context,
                                   List<TableList> *from_clause,
                                   TableList *tables,
                                   TableList **leaves,
                                   bool select_insert)
{
  TableList *leaves_tmp= NULL;

  if (setup_tables(session, context, from_clause, tables,
                   &leaves_tmp, select_insert))
    return true;

  if (leaves)
    *leaves= leaves_tmp;

  return false;
}


/*
  Drops in all fields instead of current '*' field

  SYNOPSIS
  insert_fields()
  session			Thread Cursor
  context             Context for name resolution
  db_name		Database name in case of 'database_name.table_name.*'
  table_name		Table name in case of 'table_name.*'
  it			Pointer to '*'
  any_privileges	0 If we should ensure that we have SELECT privileges
  for all columns
  1 If any privilege is ok
  RETURN
  0	ok     'it' is updated to point at last inserted
  1	error.  Error message is generated but not sent to client
*/

bool
insert_fields(Session *session, Name_resolution_context *context, const char *db_name,
              const char *table_name, List<Item>::iterator *it,
              bool )
{
  Field_iterator_table_ref field_iterator;
  bool found;
  char name_buff[NAME_LEN+1];

  if (db_name)
  {
    /*
      convert database to lower case for comparison
      We can't do this in Item_field as this would change the
      'name' of the item which may be used in the select list
    */
    strncpy(name_buff, db_name, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db_name= name_buff;
  }

  found= false;

  /*
    If table names are qualified, then loop over all tables used in the query,
    else treat natural joins as leaves and do not iterate over their underlying
    tables.
  */
  for (TableList *tables= (table_name ? context->table_list :
                           context->first_name_resolution_table);
       tables;
       tables= (table_name ? tables->next_local :
                tables->next_name_resolution_table)
      )
  {
    Field *field;
    Table *table= tables->table;

    assert(tables->is_leaf_for_name_resolution());

    if ((table_name && my_strcasecmp(table_alias_charset, table_name, tables->alias)) ||
        (db_name && my_strcasecmp(system_charset_info, tables->getSchemaName(),db_name)))
      continue;

    /*
      Update the tables used in the query based on the referenced fields. For
      views and natural joins this update is performed inside the loop below.
    */
    if (table)
      session->used_tables|= table->map;

    /*
      Initialize a generic field iterator for the current table reference.
      Notice that it is guaranteed that this iterator will iterate over the
      fields of a single table reference, because 'tables' is a leaf (for
      name resolution purposes).
    */
    field_iterator.set(tables);

    for (; !field_iterator.end_of_fields(); field_iterator.next())
    {
      Item *item;

      if (!(item= field_iterator.create_item(session)))
        return true;

      if (!found)
      {
        found= true;
        it->replace(item); /* Replace '*' with the first found item. */
      }
      else
        it->after(item);   /* Add 'item' to the SELECT list. */

      if ((field= field_iterator.field()))
      {
        /* Mark fields as used to allow storage engine to optimze access */
        field->getTable()->setReadSet(field->position());
        if (table)
        {
          table->covering_keys&= field->part_of_key;
          table->merge_keys|= field->part_of_key;
        }
        if (tables->is_natural_join)
        {
          Table *field_table;
          /*
            In this case we are sure that the column ref will not be created
            because it was already created and stored with the natural join.
          */
          Natural_join_column *nj_col;
          if (!(nj_col= field_iterator.get_natural_column_ref()))
            return true;
          assert(nj_col->table_field);
          field_table= nj_col->table_ref->table;
          if (field_table)
          {
            session->used_tables|= field_table->map;
            field_table->covering_keys&= field->part_of_key;
            field_table->merge_keys|= field->part_of_key;
            field_table->used_fields++;
          }
        }
      }
      else
      {
        session->used_tables|= item->used_tables();
      }

      session->lex().current_select->cur_pos_in_select_list++;
    }
    /*
      In case of stored tables, all fields are considered as used,
      while in the case of views, the fields considered as used are the
      ones marked in setup_tables during fix_fields of view columns.
      For NATURAL joins, used_tables is updated in the IF above.
    */
    if (table)
      table->used_fields= table->getShare()->sizeFields();
  }
  if (found)
    return false;

  /*
    @TODO in the case when we skipped all columns because there was a
    qualified '*', and all columns were coalesced, we have to give a more
    meaningful message than ER_BAD_TABLE_ERROR.
  */
  if (not table_name)
  {
    my_message(ER_NO_TABLES_USED, ER(ER_NO_TABLES_USED), MYF(0));
  }
  else
  {
    my_error(ER_BAD_TABLE_ERROR, MYF(0), table_name);
  }

  return true;
}


/*
  Fix all conditions and outer join expressions.

  SYNOPSIS
  setup_conds()
  session     thread Cursor
  tables  list of tables for name resolving (select_lex->table_list)
  leaves  list of leaves of join table tree (select_lex->leaf_tables)
  conds   WHERE clause

  DESCRIPTION
  TODO

  RETURN
  true  if some error occured (e.g. out of memory)
  false if all is OK
*/

int Session::setup_conds(TableList *leaves, COND **conds)
{
  Session *session= this;
  Select_Lex *select_lex= session->lex().current_select;
  TableList *table= NULL;	// For HP compilers
  void *save_session_marker= session->session_marker;
  /*
    it_is_update set to true when tables of primary Select_Lex (Select_Lex
    which belong to LEX, i.e. most up SELECT) will be updated by
    INSERT/UPDATE/LOAD
    NOTE-> using this condition helps to prevent call of prepare_check_option()
    from subquery of VIEW, because tables of subquery belongs to VIEW
    (see condition before prepare_check_option() call)
  */
  bool save_is_item_list_lookup= select_lex->is_item_list_lookup;
  select_lex->is_item_list_lookup= 0;

  session->mark_used_columns= MARK_COLUMNS_READ;
  select_lex->cond_count= 0;
  select_lex->between_count= 0;
  select_lex->max_equal_elems= 0;

  session->session_marker= (void*)1;
  if (*conds)
  {
    session->setWhere("where clause");
    if ((!(*conds)->fixed && (*conds)->fix_fields(session, conds)) ||
        (*conds)->check_cols(1))
      goto err_no_arena;
  }
  session->session_marker= save_session_marker;

  /*
    Apply fix_fields() to all ON clauses at all levels of nesting,
    including the ones inside view definitions.
  */
  for (table= leaves; table; table= table->next_leaf)
  {
    TableList *embedded; /* The table at the current level of nesting. */
    TableList *embedding= table; /* The parent nested table reference. */
    do
    {
      embedded= embedding;
      if (embedded->on_expr)
      {
        /* Make a join an a expression */
        session->session_marker= (void*)embedded;
        session->setWhere("on clause");
        if ((!embedded->on_expr->fixed && embedded->on_expr->fix_fields(session, &embedded->on_expr)) ||
            embedded->on_expr->check_cols(1))
          goto err_no_arena;
        select_lex->cond_count++;
      }
      embedding= embedded->getEmbedding();
    }
    while (embedding &&
           &embedding->getNestedJoin()->join_list.front() == embedded);

  }
  session->session_marker= save_session_marker;

  session->lex().current_select->is_item_list_lookup= save_is_item_list_lookup;
  return(test(session->is_error()));

err_no_arena:
  select_lex->is_item_list_lookup= save_is_item_list_lookup;

  return 1;
}


/******************************************************************************
 ** Fill a record with data (for INSERT or UPDATE)
 ** Returns : 1 if some field has wrong type
 ******************************************************************************/


/*
  Fill fields with given items.

  SYNOPSIS
  fill_record()
  fields        Item_fields list to be filled
  values        values to fill with
  ignore_errors true if we should ignore errors

  NOTE
  fill_record() may set table->auto_increment_field_not_null and a
  caller should make sure that it is reset after their last call to this
  function.

  RETURN
  false   OK
  true    error occured
*/

bool
fill_record(Session *session, List<Item> &fields, List<Item> &values, bool ignore_errors)
{
  List<Item>::iterator f(fields.begin());
  List<Item>::iterator v(values.begin());
  Item *value;
  Item_field *field;
  Table *table;

  /*
    Reset the table->auto_increment_field_not_null as it is valid for
    only one row.
  */
  if (fields.size())
  {
    /*
      On INSERT or UPDATE fields are checked to be from the same table,
      thus we safely can take table from the first field.
    */
    field= static_cast<Item_field *>(f++);
    table= field->field->getTable();
    table->auto_increment_field_not_null= false;
    f= fields.begin();
  }

  while ((field= static_cast<Item_field *>(f++)))
  {
    value= v++;

    Field *rfield= field->field;
    table= rfield->getTable();

    if (rfield == table->next_number_field)
      table->auto_increment_field_not_null= true;
    if ((value->save_in_field(rfield, 0) < 0) && !ignore_errors)
    {
      my_message(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR), MYF(0));
      if (table)
        table->auto_increment_field_not_null= false;

      return true;
    }
  }

  return session->is_error();
}


/*
  Fill field buffer with values from Field list

  SYNOPSIS
  fill_record()
  ptr           pointer on pointer to record
  values        list of fields
  ignore_errors true if we should ignore errors

  NOTE
  fill_record() may set table->auto_increment_field_not_null and a
  caller should make sure that it is reset after their last call to this
  function.

  RETURN
  false   OK
  true    error occured
*/

bool fill_record(Session *session, Field **ptr, List<Item> &values, bool)
{
  List<Item>::iterator v(values.begin());
  Item *value;
  Table *table= 0;
  Field *field;

  /*
    Reset the table->auto_increment_field_not_null as it is valid for
    only one row.
  */
  if (*ptr)
  {
    /*
      On INSERT or UPDATE fields are checked to be from the same table,
      thus we safely can take table from the first field.
    */
    table= (*ptr)->getTable();
    table->auto_increment_field_not_null= false;
  }

  while ((field = *ptr++) && ! session->is_error())
  {
    value=v++;
    table= field->getTable();

    if (field == table->next_number_field)
      table->auto_increment_field_not_null= true;

    if (value->save_in_field(field, 0) < 0)
    {
      if (table)
        table->auto_increment_field_not_null= false;

      return true;
    }
  }

  return(session->is_error());
}


void drizzle_rm_tmp_tables()
{
  assert(drizzle_tmpdir.size());
  Session::shared_ptr session= Session::make_shared(plugin::Listen::getNullClient(), catalog::local());
  session->thread_stack= (char*) session.get();
  session->storeGlobals();
  plugin::StorageEngine::removeLostTemporaryTables(*session, drizzle_tmpdir.c_str());
}

/**
  @} (end of group Data_Dictionary)
*/

void kill_drizzle(void)
{
  pthread_kill(signal_thread, SIGTERM);
  shutdown_in_progress= 1;			// Safety if kill didn't work
}

} /* namespace drizzled */
