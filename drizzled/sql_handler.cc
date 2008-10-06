/* Copyright (C) 2000-2004 MySQL AB
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* HANDLER ... commands - direct access to ISAM */

/* TODO:
  HANDLER blabla OPEN [ AS foobar ] [ (column-list) ]

  the most natural (easiest, fastest) way to do it is to
  compute List<Item> field_list not in mysql_ha_read
  but in mysql_ha_open, and then store it in Table structure.

  The problem here is that mysql_parse calls free_item to free all the
  items allocated at the end of every query. The workaround would to
  keep two item lists per THD - normal free_list and handler_items.
  The second is to be freeed only on thread end. mysql_ha_open should
  then do { handler_items=concat(handler_items, free_list); free_list=0; }

  But !!! do_command calls free_root at the end of every query and frees up
  all the sql_alloc'ed memory. It's harder to work around...
*/

/*
  There are two containers holding information about open handler tables.
  The first is 'thd->handler_tables'. It is a linked list of Table objects.
  It is used like 'thd->open_tables' in the table cache. The trick is to
  exchange these two lists during open and lock of tables. Thus the normal
  table cache code can be used.
  The second container is a HASH. It holds objects of the type TableList.
  Despite its name, no lists of tables but only single structs are hashed
  (the 'next' pointer is always NULL). The reason for theis second container
  is, that we want handler tables to survive FLUSH Table commands. A table
  affected by FLUSH Table must be closed so that other threads are not
  blocked by handler tables still in use. Since we use the normal table cache
  functions with 'thd->handler_tables', the closed tables are removed from
  this list. Hence we need the original open information for the handler
  table in the case that it is used again. This information is handed over
  to mysql_ha_open() as a TableList. So we store this information in the
  second container, where it is not affected by FLUSH Table. The second
  container is implemented as a hash for performance reasons. Consequently,
  we use it not only for re-opening a handler table, but also for the
  HANDLER ... READ commands. For this purpose, we store a pointer to the
  Table structure (in the first container) in the TBALE_LIST object in the
  second container. When the table is flushed, the pointer is cleared.
*/

#include <drizzled/server_includes.h>
#include <drizzled/sql_select.h>

#define HANDLER_TABLES_HASH_SIZE 120

/**
  Close a HANDLER table.

  @param thd Thread identifier.
  @param tables A list of tables with the first entry to close.
  @param is_locked If LOCK_open is locked.

  @note Though this function takes a list of tables, only the first list entry
  will be closed.
  @note Broadcasts refresh if it closed a table with old version.
*/

static void mysql_ha_close_table(THD *thd, TableList *tables,
                                 bool is_locked)
{
  Table **table_ptr;

  /*
    Though we could take the table pointer from hash_tables->table,
    we must follow the thd->handler_tables chain anyway, as we need the
    address of the 'next' pointer referencing this table
    for close_thread_table().
  */
  for (table_ptr= &(thd->handler_tables);
       *table_ptr && (*table_ptr != tables->table);
         table_ptr= &(*table_ptr)->next)
    ;

  if (*table_ptr)
  {
    (*table_ptr)->file->ha_index_or_rnd_end();
    if (! is_locked)
      pthread_mutex_lock(&LOCK_open);
    if (close_thread_table(thd, table_ptr))
    {
      /* Tell threads waiting for refresh that something has happened */
      broadcast_refresh();
    }
    if (! is_locked)
      pthread_mutex_unlock(&LOCK_open);
  }
  else if (tables->table)
  {
    /* Must be a temporary table */
    Table *table= tables->table;
    table->file->ha_index_or_rnd_end();
    table->query_id= thd->query_id;
    table->open_by_handler= 0;
  }
}


/*
  Close a HANDLER table by alias or table name

  SYNOPSIS
    mysql_ha_close()
    thd                         Thread identifier.
    tables                      A list of tables with the first entry to close.

  DESCRIPTION
    Closes the table that is associated (on the handler tables hash) with the
    name (table->alias) of the specified table.

  RETURN
    false ok
    true  error
*/

bool mysql_ha_close(THD *thd, TableList *tables)
{
  TableList    *hash_tables;

  if ((hash_tables= (TableList*) hash_search(&thd->handler_tables_hash,
                                              (unsigned char*) tables->alias,
                                              strlen(tables->alias) + 1)))
  {
    mysql_ha_close_table(thd, hash_tables, false);
    hash_delete(&thd->handler_tables_hash, (unsigned char*) hash_tables);
  }
  else
  {
    my_error(ER_UNKNOWN_TABLE, MYF(0), tables->alias, "HANDLER");
    return(true);
  }

  my_ok(thd);
  return(false);
}


/**
  Scan the handler tables hash for matching tables.

  @param thd Thread identifier.
  @param tables The list of tables to remove.

  @return Pointer to head of linked list (TableList::next_local) of matching
          TableList elements from handler_tables_hash. Otherwise, NULL if no
          table was matched.
*/

static TableList *mysql_ha_find(THD *thd, TableList *tables)
{
  TableList *hash_tables, *head= NULL, *first= tables;

  /* search for all handlers with matching table names */
  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (TableList*) hash_element(&thd->handler_tables_hash, i);
    for (tables= first; tables; tables= tables->next_local)
    {
      if ((! *tables->db ||
          ! my_strcasecmp(&my_charset_utf8_general_ci, hash_tables->db, tables->db)) &&
          ! my_strcasecmp(&my_charset_utf8_general_ci, hash_tables->table_name,
                          tables->table_name))
        break;
    }
    if (tables)
    {
      hash_tables->next_local= head;
      head= hash_tables;
    }
  }

  return(head);
}


/**
  Remove matching tables from the HANDLER's hash table.

  @param thd Thread identifier.
  @param tables The list of tables to remove.
  @param is_locked If LOCK_open is locked.

  @note Broadcasts refresh if it closed a table with old version.
*/

void mysql_ha_rm_tables(THD *thd, TableList *tables, bool is_locked)
{
  TableList *hash_tables, *next;

  assert(tables);

  hash_tables= mysql_ha_find(thd, tables);

  while (hash_tables)
  {
    next= hash_tables->next_local;
    if (hash_tables->table)
      mysql_ha_close_table(thd, hash_tables, is_locked);
    hash_delete(&thd->handler_tables_hash, (unsigned char*) hash_tables);
    hash_tables= next;
  }

  return;
}


/**
  Flush (close and mark for re-open) all tables that should be should
  be reopen.

  @param thd Thread identifier.

  @note Broadcasts refresh if it closed a table with old version.
*/

void mysql_ha_flush(THD *thd)
{
  TableList *hash_tables;

  safe_mutex_assert_owner(&LOCK_open);

  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (TableList*) hash_element(&thd->handler_tables_hash, i);
    if (hash_tables->table && hash_tables->table->needs_reopen_or_name_lock())
    {
      mysql_ha_close_table(thd, hash_tables, true);
      /* Mark table as closed, ready for re-open. */
      hash_tables->table= NULL;
    }
  }

  return;
}


/**
  Close all HANDLER's tables.

  @param thd Thread identifier.

  @note Broadcasts refresh if it closed a table with old version.
*/

void mysql_ha_cleanup(THD *thd)
{
  TableList *hash_tables;

  for (uint i= 0; i < thd->handler_tables_hash.records; i++)
  {
    hash_tables= (TableList*) hash_element(&thd->handler_tables_hash, i);
    if (hash_tables->table)
      mysql_ha_close_table(thd, hash_tables, false);
   }

  hash_free(&thd->handler_tables_hash);

  return;
}
