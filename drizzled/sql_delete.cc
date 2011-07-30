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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Delete of records and truncate of tables.

  Multi-table deletes were introduced by Monty and Sinisa
*/
#include <config.h>
#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/probes.h>
#include <drizzled/sql_parse.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/probes.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/records.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/transaction_services.h>
#include <drizzled/filesort.h>
#include <drizzled/sql_lex.h>
#include <drizzled/diagnostics_area.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session/transactions.h>

namespace drizzled {

/**
  Implement DELETE SQL word.

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool delete_query(Session *session, TableList *table_list, COND *conds,
                  SQL_LIST *order, ha_rows limit, uint64_t,
                  bool reset_auto_increment)
{
  int		error;
  Table		*table;
  optimizer::SqlSelect *select= NULL;
  ReadRecord	info;
  bool          using_limit=limit != HA_POS_ERROR;
  ha_rows	deleted= 0;
  uint32_t usable_index= MAX_KEY;
  Select_Lex   *select_lex= &session->lex().select_lex;
  Session::killed_state_t killed_status= Session::NOT_KILLED;

  if (session->openTablesLock(table_list))
  {
    DRIZZLE_DELETE_DONE(1, 0);
    return true;
  }

  table= table_list->table;
  assert(table);

  session->set_proc_info("init");
  table->map=1;

  if (prepare_delete(session, table_list, &conds))
  {
    DRIZZLE_DELETE_DONE(1, 0);
    return true;
  }

  /* check ORDER BY even if it can be ignored */
  if (order && order->elements)
  {
    TableList   tables;
    List<Item>   fields;
    List<Item>   all_fields;

    tables.table = table;
    tables.alias = table_list->alias;

    select_lex->setup_ref_array(session, order->elements);
    if (setup_order(session, select_lex->ref_pointer_array, &tables, fields, all_fields, (Order*) order->first))
    {
      delete select;
      free_underlaid_joins(session, &session->lex().select_lex);
      DRIZZLE_DELETE_DONE(1, 0);

      return true;
    }
  }

  bool const_cond= not conds || conds->const_item();

  select_lex->no_error= session->lex().ignore;

  bool const_cond_result= const_cond && (!conds || conds->val_int());
  if (session->is_error())
  {
    /* Error evaluating val_int(). */
    return true;
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
    /* Update the table->cursor->stats.records number */
    table->cursor->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    ha_rows const maybe_deleted= table->cursor->stats.records;
    if (!(error=table->cursor->ha_delete_all_rows()))
    {
      error= -1;				// ok
      deleted= maybe_deleted;
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND)
    {
      table->print_error(error,MYF(0));
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

  /* Update the table->cursor->stats.records number */
  table->cursor->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->covering_keys.reset();
  table->quick_keys.reset();		// Can't use 'only index'
  select= optimizer::make_select(table, 0, 0, conds, 0, &error);
  if (error)
  {
    DRIZZLE_DELETE_DONE(1, 0);
    return true;
  }

  if ((select && select->check_quick(session, false, limit)) || !limit)
  {
    delete select;
    free_underlaid_joins(session, select_lex);
    session->row_count_func= 0;
    if (session->is_error())
      return true;
    DRIZZLE_DELETE_DONE(0, 0);
    /**
     * Resetting the Diagnostic area to prevent
     * lp bug# 439719
     */
    session->main_da().reset_diagnostics_area();
    session->my_ok((ha_rows) session->rowCount());
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
  }

  if (order && order->elements)
  {
    if ((!select || table->quick_keys.none()) && limit != HA_POS_ERROR)
      usable_index= optimizer::get_index_for_order(table, (Order*)(order->first), limit);

    if (usable_index == MAX_KEY)
    {
      FileSort filesort(*session);
      table->sort.io_cache= new internal::io_cache_st;
      uint32_t length= 0;
      SortField* sortorder= make_unireg_sortorder((Order*) order->first, &length, NULL);
      ha_rows examined_rows;
      if ((table->sort.found_records= filesort.run(table, sortorder, length, select, HA_POS_ERROR, 1, examined_rows)) == HA_POS_ERROR)
      {
        delete select;
        free_underlaid_joins(session, &session->lex().select_lex);

        DRIZZLE_DELETE_DONE(1, 0);
        return true;
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
    DRIZZLE_DELETE_DONE(1, 0);
    return true;
  }

  if (usable_index==MAX_KEY)
  {
    if ((error= info.init_read_record(session,table,select,1,1)))
    {
      table->print_error(error, MYF(0));
      delete select;
      free_underlaid_joins(session, select_lex);
      return true;
    }
  }
  else
  {
    if ((error= info.init_read_record_idx(session, table, 1, usable_index)))
    {
      table->print_error(error, MYF(0));
      delete select;
      free_underlaid_joins(session, select_lex);
      return true;
    }
  }

  session->set_proc_info("updating");

  table->mark_columns_needed_for_delete();

  while (!(error=info.read_record(&info)) && !session->getKilled() &&
	 ! session->is_error())
  {
    // session->is_error() is tested to disallow delete row on error
    if (!(select && select->skip_record())&& ! session->is_error() )
    {
      if (!(error= table->cursor->deleteRecord(table->getInsertRecord())))
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
	table->print_error(error,MYF(0));
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
      table->cursor->unlock_row();  // Row failed selection, release lock on it
  }
  killed_status= session->getKilled();
  if (killed_status != Session::NOT_KILLED || session->is_error())
    error= 1;					// Aborted

  session->set_proc_info("end");
  info.end_read_record();

cleanup:

  if (reset_auto_increment && (error < 0))
  {
    /*
      We're really doing a truncate and need to reset the table's
      auto-increment counter.
    */
    int error2= table->cursor->ha_reset_auto_increment(0);

    if (error2 && (error2 != HA_ERR_WRONG_COMMAND))
    {
      table->print_error(error2, MYF(0));
      error= 1;
    }
  }

  delete select;
  bool transactional_table= table->cursor->has_transactions();

  if (!transactional_table && deleted > 0)
    session->transaction.stmt.markModifiedNonTransData();

  /* See similar binlogging code in sql_update.cc, for comments */
  if ((error < 0) || session->transaction.stmt.hasModifiedNonTransData())
  {
    if (session->transaction.stmt.hasModifiedNonTransData())
      session->transaction.all.markModifiedNonTransData();
  }
  assert(transactional_table || !deleted || session->transaction.stmt.hasModifiedNonTransData());
  free_underlaid_joins(session, select_lex);

  DRIZZLE_DELETE_DONE((error >= 0 || session->is_error()), deleted);
  if (error < 0 || (session->lex().ignore && !session->is_fatal_error))
  {
    session->row_count_func= deleted;
    /**
     * Resetting the Diagnostic area to prevent
     * lp bug# 439719
     */
    session->main_da().reset_diagnostics_area();
    session->my_ok((ha_rows) session->rowCount());
  }
  session->status_var.deleted_row_count+= deleted;

  return (error >= 0 || session->is_error());
}


/*
  Prepare items in DELETE statement

  SYNOPSIS
    prepare_delete()
    session			- thread handler
    table_list		- global/local table list
    conds		- conditions

  RETURN VALUE
    false OK
    true  error
*/
int prepare_delete(Session *session, TableList *table_list, Item **conds)
{
  Select_Lex *select_lex= &session->lex().select_lex;
  session->lex().allow_sum_func= 0;
  if (setup_tables_and_check_access(session, &session->lex().select_lex.context, &select_lex->top_join_list, 
    table_list, &select_lex->leaf_tables, false) ||
      session->setup_conds(table_list, conds))
    return true;

  if (unique_table(table_list, table_list->next_global))
  {
    my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->alias);
    return true;
  }
  List<Item> all_fields;
  if (select_lex->inner_refs_list.size() && fix_inner_refs(session, all_fields, select_lex, select_lex->ref_pointer_array))
    return true;
  return false;
}


/***************************************************************************
  TRUNCATE Table
****************************************************************************/

/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed
*/

bool truncate(Session& session, TableList *table_list)
{
  uint64_t save_options= session.options;
  table_list->lock_type= TL_WRITE;
  session.options&= ~(OPTION_BEGIN | OPTION_NOT_AUTOCOMMIT);
  init_select(&session.lex());
  int error= delete_query(&session, table_list, (COND*) 0, (SQL_LIST*) 0, HA_POS_ERROR, 0L, true);
  /*
    Safety, in case the engine ignored ha_enable_transaction(false)
    above. Also clears session->transaction.*.
  */
  error= TransactionServices::autocommitOrRollback(session, error);
  session.options= save_options;
  return error;
}

} /* namespace drizzled */
