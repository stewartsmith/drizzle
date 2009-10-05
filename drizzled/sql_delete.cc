/* Copyright (C) 2000 MySQL AB

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
  Delete of records and truncate of tables.

  Multi-table deletes were introduced by Monty and Sinisa
*/
#include <drizzled/server_includes.h>
#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/probes.h>
#include <drizzled/sql_parse.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include "drizzled/probes.h"

using namespace drizzled;

/**
  Implement DELETE SQL word.

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool mysql_delete(Session *session, TableList *table_list, COND *conds,
                  SQL_LIST *order, ha_rows limit, uint64_t options,
                  bool reset_auto_increment)
{
  bool          will_batch;
  int		error, loc_error;
  Table		*table;
  SQL_SELECT	*select=0;
  READ_RECORD	info;
  bool          using_limit=limit != HA_POS_ERROR;
  bool		transactional_table, safe_update, const_cond;
  bool          const_cond_result;
  ha_rows	deleted= 0;
  uint32_t usable_index= MAX_KEY;
  Select_Lex   *select_lex= &session->lex->select_lex;
  Session::killed_state killed_status= Session::NOT_KILLED;

  if (session->openTablesLock(table_list))
  {
    DRIZZLE_DELETE_DONE(1, 0);
    return true;
  }

  table= table_list->table;
  assert(table);

  session->set_proc_info("init");
  table->map=1;

  if (mysql_prepare_delete(session, table_list, &conds))
    goto err;

  /* check ORDER BY even if it can be ignored */
  if (order && order->elements)
  {
    TableList   tables;
    List<Item>   fields;
    List<Item>   all_fields;

    memset(&tables, 0, sizeof(tables));
    tables.table = table;
    tables.alias = table_list->alias;

      if (select_lex->setup_ref_array(session, order->elements) ||
	  setup_order(session, select_lex->ref_pointer_array, &tables,
                    fields, all_fields, (order_st*) order->first))
    {
      delete select;
      free_underlaid_joins(session, &session->lex->select_lex);
      goto err;
    }
  }

  const_cond= (!conds || conds->const_item());
  safe_update=test(session->options & OPTION_SAFE_UPDATES);
  if (safe_update && const_cond)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    goto err;
  }

  select_lex->no_error= session->lex->ignore;

  const_cond_result= const_cond && (!conds || conds->val_int());
  if (session->is_error())
  {
    /* Error evaluating val_int(). */
    return(true);
  }

  /*
    Test if the user wants to delete all rows and deletion doesn't have
    any side-effects (because of triggers), so we can use optimized
    handler::delete_all_rows() method.

    We implement fast TRUNCATE for InnoDB even if triggers are
    present.  TRUNCATE ignores triggers.

    We can use delete_all_rows() if and only if:
    - We allow new functions (not using option --skip-new), and are
      not in safe mode (not using option --safe-mode)
    - There is no limit clause
    - The condition is constant
    - If there is a condition, then it it produces a non-zero value
    - If the current command is DELETE FROM with no where clause
      (i.e., not TRUNCATE) then:
      - We should not be binlogging this statement row-based, and
      - there should be no delete triggers associated with the table.
  */
  if (!using_limit && const_cond_result)
  {
    /* Update the table->file->stats.records number */
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    ha_rows const maybe_deleted= table->file->stats.records;
    if (!(error=table->file->ha_delete_all_rows()))
    {
      error= -1;				// ok
      deleted= maybe_deleted;
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND)
    {
      table->file->print_error(error,MYF(0));
      error=0;
      goto cleanup;
    }
    /* Handler didn't support fast delete; Delete rows one by one */
  }
  if (conds)
  {
    Item::cond_result result;
    conds= remove_eq_conds(session, conds, &result);
    if (result == Item::COND_FALSE)             // Impossible where
      limit= 0;
  }

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->covering_keys.reset();
  table->quick_keys.reset();		// Can't use 'only index'
  select=make_select(table, 0, 0, conds, 0, &error);
  if (error)
    goto err;
  if ((select && select->check_quick(session, safe_update, limit)) || !limit)
  {
    delete select;
    free_underlaid_joins(session, select_lex);
    session->row_count_func= 0;
    DRIZZLE_DELETE_DONE(0, 0);
    /**
     * Resetting the Diagnostic area to prevent
     * lp bug# 439719
     */
    session->main_da.reset_diagnostics_area();
    session->my_ok((ha_rows) session->row_count_func);
    /*
      We don't need to call reset_auto_increment in this case, because
      mysql_truncate always gives a NULL conds argument, hence we never
      get here.
    */
    return 0; // Nothing to delete
  }

  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.none())
  {
    session->server_status|=SERVER_QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      delete select;
      free_underlaid_joins(session, select_lex);
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
                 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      goto err;
    }
  }
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_QUICK);

  if (order && order->elements)
  {
    uint32_t         length= 0;
    SORT_FIELD  *sortorder;
    ha_rows examined_rows;

    if ((!select || table->quick_keys.none()) && limit != HA_POS_ERROR)
      usable_index= get_index_for_order(table, (order_st*)(order->first), limit);

    if (usable_index == MAX_KEY)
    {
      table->sort.io_cache= new IO_CACHE;
      memset(table->sort.io_cache, 0, sizeof(IO_CACHE));


      if (!(sortorder= make_unireg_sortorder((order_st*) order->first,
                                             &length, NULL)) ||
	  (table->sort.found_records = filesort(session, table, sortorder, length,
                                                select, HA_POS_ERROR, 1,
                                                &examined_rows))
	  == HA_POS_ERROR)
      {
        delete select;
        free_underlaid_joins(session, &session->lex->select_lex);
        goto err;
      }
      /*
        Filesort has already found and selected the rows we want to delete,
        so we don't need the where clause
      */
      delete select;
      free_underlaid_joins(session, select_lex);
      select= 0;
    }
  }

  /* If quick select is used, initialize it before retrieving rows. */
  if (select && select->quick && select->quick->reset())
  {
    delete select;
    free_underlaid_joins(session, select_lex);
    goto err;
  }
  if (usable_index==MAX_KEY)
    init_read_record(&info,session,table,select,1,1);
  else
    init_read_record_idx(&info, session, table, 1, usable_index);

  session->set_proc_info("updating");

  will_batch= !table->file->start_bulk_delete();


  table->mark_columns_needed_for_delete();

  while (!(error=info.read_record(&info)) && !session->killed &&
	 ! session->is_error())
  {
    // session->is_error() is tested to disallow delete row on error
    if (!(select && select->skip_record())&& ! session->is_error() )
    {
      if (!(error= table->file->ha_delete_row(table->record[0])))
      {
	deleted++;
	if (!--limit && using_limit)
	{
	  error= -1;
	  break;
	}
      }
      else
      {
	table->file->print_error(error,MYF(0));
	/*
	  In < 4.0.14 we set the error number to 0 here, but that
	  was not sensible, because then MySQL would not roll back the
	  failed DELETE, and also wrote it to the binlog. For MyISAM
	  tables a DELETE probably never should fail (?), but for
	  InnoDB it can fail in a FOREIGN KEY error or an
	  out-of-tablespace error.
	*/
 	error= 1;
	break;
      }
    }
    else
      table->file->unlock_row();  // Row failed selection, release lock on it
  }
  killed_status= session->killed;
  if (killed_status != Session::NOT_KILLED || session->is_error())
    error= 1;					// Aborted
  if (will_batch && (loc_error= table->file->end_bulk_delete()))
  {
    if (error != 1)
      table->file->print_error(loc_error,MYF(0));
    error=1;
  }
  session->set_proc_info("end");
  end_read_record(&info);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);

  if (reset_auto_increment && (error < 0))
  {
    /*
      We're really doing a truncate and need to reset the table's
      auto-increment counter.
    */
    int error2= table->file->ha_reset_auto_increment(0);

    if (error2 && (error2 != HA_ERR_WRONG_COMMAND))
    {
      table->file->print_error(error2, MYF(0));
      error= 1;
    }
  }

cleanup:

  delete select;
  transactional_table= table->file->has_transactions();

  if (!transactional_table && deleted > 0)
    session->transaction.stmt.modified_non_trans_table= true;

  /* See similar binlogging code in sql_update.cc, for comments */
  if ((error < 0) || session->transaction.stmt.modified_non_trans_table)
  {
    if (session->transaction.stmt.modified_non_trans_table)
      session->transaction.all.modified_non_trans_table= true;
  }
  assert(transactional_table || !deleted || session->transaction.stmt.modified_non_trans_table);
  free_underlaid_joins(session, select_lex);

  DRIZZLE_DELETE_DONE((error >= 0 || session->is_error()), deleted);
  if (error < 0 || (session->lex->ignore && !session->is_fatal_error))
  {
    session->row_count_func= deleted;
    /**
     * Resetting the Diagnostic area to prevent
     * lp bug# 439719
     */
    //session->main_da.reset_diagnostics_area();    
    session->my_ok((ha_rows) session->row_count_func);
  }
  return (error >= 0 || session->is_error());

err:
  DRIZZLE_DELETE_DONE(1, 0);
  return true;
}


/*
  Prepare items in DELETE statement

  SYNOPSIS
    mysql_prepare_delete()
    session			- thread handler
    table_list		- global/local table list
    conds		- conditions

  RETURN VALUE
    false OK
    true  error
*/
int mysql_prepare_delete(Session *session, TableList *table_list, Item **conds)
{
  Select_Lex *select_lex= &session->lex->select_lex;

  List<Item> all_fields;

  session->lex->allow_sum_func= 0;
  if (setup_tables_and_check_access(session, &session->lex->select_lex.context,
                                    &session->lex->select_lex.top_join_list,
                                    table_list,
                                    &select_lex->leaf_tables, false) ||
      session->setup_conds(table_list, conds))
    return(true);
  {
    TableList *duplicate;
    if ((duplicate= unique_table(session, table_list, table_list->next_global, 0)))
    {
      my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->alias);
      return(true);
    }
  }

  if (select_lex->inner_refs_list.elements &&
    fix_inner_refs(session, all_fields, select_lex, select_lex->ref_pointer_array))
    return(-1);

  return(false);
}


/***************************************************************************
  TRUNCATE Table
****************************************************************************/

/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed

  dont_send_ok should be set if:
  - We should always wants to generate the table (even if the table type
    normally can't safely do this.
  - We don't want an ok to be sent to the end user.
  - We don't want to log the truncate command
  - If we want to have a name lock on the table on exit without errors.
*/

bool mysql_truncate(Session *session, TableList *table_list, bool dont_send_ok)
{
  HA_CREATE_INFO create_info;
  char path[FN_REFLEN];
  Table *table;
  bool error;
  uint32_t path_length;


  memset(&create_info, 0, sizeof(create_info));
  /* If it is a temporary table, close and regenerate it */
  if (!dont_send_ok && (table= session->find_temporary_table(table_list)))
  {
    plugin::StorageEngine *table_type= table->s->db_type();
    TableShare *share= table->s;

    if (!table_type->check_flag(HTON_BIT_CAN_RECREATE))
      goto trunc_by_del;

    table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);

    session->close_temporary_table(table, false, false);    // Don't free share
    ha_create_table(session, share->normalized_path.str,
                    share->db.str, share->table_name.str, &create_info, 1,
                    NULL);
    // We don't need to call invalidate() because this table is not in cache
    if ((error= (int) !(session->open_temporary_table(share->path.str,
                                                      share->db.str,
                                                      share->table_name.str, 1,
                                                      OTM_OPEN))))
      (void) session->rm_temporary_table(table_type, path);
    share->free_table_share();
    free((char*) table);
    /*
      If we return here we will not have logged the truncation to the bin log
      and we will not my_ok() to the client.
    */
    goto end;
  }

  path_length= build_table_filename(path, sizeof(path), table_list->db,
                                    table_list->table_name, 0);

  if (!dont_send_ok)
    goto trunc_by_del;

  pthread_mutex_lock(&LOCK_open); /* Recreate table for truncate */
  error= ha_create_table(session, path, table_list->db, table_list->table_name,
                         &create_info, 1, NULL);
  pthread_mutex_unlock(&LOCK_open);

end:
  if (!dont_send_ok)
  {
    if (!error)
    {
      /*
        TRUNCATE must always be statement-based binlogged (not row-based) so
        we don't test current_stmt_binlog_row_based.
      */
      write_bin_log(session, true, session->query, session->query_length);
      session->my_ok();		// This should return record count
    }
    pthread_mutex_lock(&LOCK_open); /* For truncate delete from hash when finished */
    unlock_table_name(table_list);
    pthread_mutex_unlock(&LOCK_open);
  }
  else if (error)
  {
    pthread_mutex_lock(&LOCK_open); /* For truncate delete from hash when finished */
    unlock_table_name(table_list);
    pthread_mutex_unlock(&LOCK_open);
  }
  return(error);

trunc_by_del:
  /* Probably InnoDB table */
  uint64_t save_options= session->options;
  table_list->lock_type= TL_WRITE;
  session->options&= ~(OPTION_BEGIN | OPTION_NOT_AUTOCOMMIT);
  ha_enable_transaction(session, false);
  mysql_init_select(session->lex);
  error= mysql_delete(session, table_list, (COND*) 0, (SQL_LIST*) 0,
                      HA_POS_ERROR, 0L, true);
  ha_enable_transaction(session, true);
  /*
    Safety, in case the engine ignored ha_enable_transaction(false)
    above. Also clears session->transaction.*.
  */
  error= ha_autocommit_or_rollback(session, error);
  ha_commit(session);
  session->options= save_options;
  return(error);
}
