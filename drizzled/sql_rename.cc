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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Atomic rename of table;  RENAME TABLE t1 to t2, tmp to t1 [,...]
*/
#include <drizzled/server_includes.h>
#include <drizzled/drizzled_error_messages.h>

static TABLE_LIST *rename_tables(THD *thd, TABLE_LIST *table_list,
				 bool skip_error);

static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list);

/*
  Every second entry in the table_list is the original name and every
  second entry is the new name.
*/

bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list, bool silent)
{
  bool error= 1;
  TABLE_LIST *ren_table= 0;

  /*
    Avoid problems with a rename on a table that we have locked or
    if the user is trying to to do this in a transcation context
  */
  if (thd->locked_tables || thd->active_transaction())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    return(1);
  }

  mysql_ha_rm_tables(thd, table_list, false);

  if (wait_if_global_read_lock(thd,0,1))
    return(1);

  pthread_mutex_lock(&LOCK_open);
  if (lock_table_names_exclusively(thd, table_list))
  {
    pthread_mutex_unlock(&LOCK_open);
    goto err;
  }

  error=0;
  if ((ren_table=rename_tables(thd,table_list,0)))
  {
    /* Rename didn't succeed;  rename back the tables in reverse order */
    TABLE_LIST *table;

    /* Reverse the table list */
    table_list= reverse_table_list(table_list);

    /* Find the last renamed table */
    for (table= table_list;
	 table->next_local != ren_table ;
	 table= table->next_local->next_local) ;
    table= table->next_local->next_local;		// Skip error table
    /* Revert to old names */
    rename_tables(thd, table, 1);

    /* Revert the table list (for prepared statements) */
    table_list= reverse_table_list(table_list);

    error= 1;
  }
  /*
    An exclusive lock on table names is satisfactory to ensure
    no other thread accesses this table.
    We still should unlock LOCK_open as early as possible, to provide
    higher concurrency - query_cache_invalidate can take minutes to
    complete.
  */
  pthread_mutex_unlock(&LOCK_open);

  /* Lets hope this doesn't fail as the result will be messy */ 
  if (!silent && !error)
  {
    write_bin_log(thd, true, thd->query, thd->query_length);
    my_ok(thd);
  }

  pthread_mutex_lock(&LOCK_open);
  unlock_table_names(thd, table_list, (TABLE_LIST*) 0);
  pthread_mutex_unlock(&LOCK_open);

err:
  start_waiting_global_read_lock(thd);
  return(error);
}


/*
  reverse table list

  SYNOPSIS
    reverse_table_list()
    table_list pointer to table _list

  RETURN
    pointer to new (reversed) list
*/
static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list)
{
  TABLE_LIST *prev= 0;

  while (table_list)
  {
    TABLE_LIST *next= table_list->next_local;
    table_list->next_local= prev;
    prev= table_list;
    table_list= next;
  }
  return (prev);
}


/*
  Rename a single table or a view

  SYNPOSIS
    do_rename()
      thd               Thread handle
      ren_table         A table/view to be renamed
      new_db            The database to which the table to be moved to
      new_table_name    The new table/view name
      new_table_alias   The new table/view alias
      skip_error        Whether to skip error

  DESCRIPTION
    Rename a single table or a view.

  RETURN
    false     Ok
    true      rename failed
*/

bool
do_rename(THD *thd, TABLE_LIST *ren_table, char *new_db, char *new_table_name,
          char *new_table_alias, bool skip_error)
{
  int rc= 1;
  char name[FN_REFLEN];
  const char *new_alias, *old_alias;
  /* TODO: What should this really be set to - it doesn't
     get set anywhere before it's used? */
  enum legacy_db_type table_type=DB_TYPE_UNKNOWN;

  if (lower_case_table_names == 2)
  {
    old_alias= ren_table->alias;
    new_alias= new_table_alias;
  }
  else
  {
    old_alias= ren_table->table_name;
    new_alias= new_table_name;
  }
  build_table_filename(name, sizeof(name),
                       new_db, new_alias, reg_ext, 0);
  if (!access(name,F_OK))
  {
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
    return(1);			// This can't be skipped
  }
  build_table_filename(name, sizeof(name),
                       ren_table->db, old_alias, reg_ext, 0);

  rc= mysql_rename_table(ha_resolve_by_legacy_type(thd, table_type),
                               ren_table->db, old_alias,
                               new_db, new_alias, 0);
  if (rc && !skip_error)
    return(1);

  return(0);

}
/*
  Rename all tables in list; Return pointer to wrong entry if something goes
  wrong.  Note that the table_list may be empty!
*/

/*
  Rename tables/views in the list

  SYNPOSIS
    rename_tables()
      thd               Thread handle
      table_list        List of tables to rename
      skip_error        Whether to skip errors

  DESCRIPTION
    Take a table/view name from and odd list element and rename it to a
    the name taken from list element+1. Note that the table_list may be
    empty.

  RETURN
    false     Ok
    true      rename failed
*/

static TABLE_LIST *
rename_tables(THD *thd, TABLE_LIST *table_list, bool skip_error)
{
  TABLE_LIST *ren_table, *new_table;

  for (ren_table= table_list; ren_table; ren_table= new_table->next_local)
  {
    new_table= ren_table->next_local;
    if (do_rename(thd, ren_table, new_table->db, new_table->table_name,
                  new_table->alias, skip_error))
      return(ren_table);
  }
  return(0);
}
