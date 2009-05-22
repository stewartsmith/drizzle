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


  if (open_and_lock_tables(session, table_list))
    return(true);

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
    DRIZZLE_DELETE_END();
    session->my_ok((ha_rows) session->row_count_func);
    /*
      We don't need to call reset_auto_increment in this case, because
      mysql_truncate always gives a NULL conds argument, hence we never
      get here.
    */
    return(0);				// Nothing to delete
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

  DRIZZLE_DELETE_END();
  if (error < 0 || (session->lex->ignore && !session->is_fatal_error))
  {
    session->row_count_func= deleted;
    session->my_ok((ha_rows) session->row_count_func);
  }
  return(error >= 0 || session->is_error());

err:
  DRIZZLE_DELETE_END();
  return(true);
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
      setup_conds(session, table_list, select_lex->leaf_tables, conds))
    return(true);
  {
    TableList *duplicate;
    if ((duplicate= unique_table(session, table_list, table_list->next_global, 0)))
    {
      update_non_unique_table_error(table_list, "DELETE", duplicate);
      return(true);
    }
  }

  if (select_lex->inner_refs_list.elements &&
    fix_inner_refs(session, all_fields, select_lex, select_lex->ref_pointer_array))
    return(-1);

  return(false);
}


/***************************************************************************
  Delete multiple tables from join
***************************************************************************/

extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b)
{
  handler *file= (handler*)arg;
  return file->cmp_ref((const unsigned char*)a, (const unsigned char*)b);
}

/*
  make delete specific preparation and checks after opening tables

  SYNOPSIS
    mysql_multi_delete_prepare()
    session         thread handler

  RETURN
    false OK
    true  Error
*/

int mysql_multi_delete_prepare(Session *session)
{
  LEX *lex= session->lex;
  TableList *aux_tables= (TableList *)lex->auxiliary_table_list.first;
  TableList *target_tbl;


  /*
    setup_tables() need for VIEWs. JOIN::prepare() will not do it second
    time.

    lex->query_tables also point on local list of DELETE Select_Lex
  */
  if (setup_tables_and_check_access(session, &session->lex->select_lex.context,
                                    &session->lex->select_lex.top_join_list,
                                    lex->query_tables,
                                    &lex->select_lex.leaf_tables, false))
    return(true);


  /*
    Multi-delete can't be constructed over-union => we always have
    single SELECT on top and have to check underlying SELECTs of it
  */
  lex->select_lex.exclude_from_table_unique_test= true;
  /* Fix tables-to-be-deleted-from list to point at opened tables */
  for (target_tbl= (TableList*) aux_tables;
       target_tbl;
       target_tbl= target_tbl->next_local)
  {
    target_tbl->table= target_tbl->correspondent_table->table;
    assert(target_tbl->table);

    /*
      Check that table from which we delete is not used somewhere
      inside subqueries/view.
    */
    {
      TableList *duplicate;
      if ((duplicate= unique_table(session, target_tbl->correspondent_table,
                                   lex->query_tables, 0)))
      {
        update_non_unique_table_error(target_tbl->correspondent_table,
                                      "DELETE", duplicate);
        return(true);
      }
    }
  }
  return(false);
}


multi_delete::multi_delete(TableList *dt, uint32_t num_of_tables_arg)
  : delete_tables(dt), deleted(0), found(0),
    num_of_tables(num_of_tables_arg), error(0),
    do_delete(0), transactional_tables(0), normal_tables(0), error_handled(0)
{
  tempfiles= (Unique **) sql_calloc(sizeof(Unique *) * num_of_tables);
}


int
multi_delete::prepare(List<Item> &, Select_Lex_Unit *u)
{

  unit= u;
  do_delete= 1;
  session->set_proc_info("deleting from main table");
  return(0);
}


bool
multi_delete::initialize_tables(JOIN *join)
{
  TableList *walk;
  Unique **tempfiles_ptr;


  if ((session->options & OPTION_SAFE_UPDATES) && error_if_full_join(join))
    return(1);

  table_map tables_to_delete_from=0;
  for (walk= delete_tables; walk; walk= walk->next_local)
    tables_to_delete_from|= walk->table->map;

  walk= delete_tables;
  delete_while_scanning= 1;
  for (JOIN_TAB *tab=join->join_tab, *end=join->join_tab+join->tables;
       tab < end;
       tab++)
  {
    if (tab->table->map & tables_to_delete_from)
    {
      /* We are going to delete from this table */
      Table *tbl=walk->table=tab->table;
      walk= walk->next_local;
      /* Don't use KEYREAD optimization on this table */
      tbl->no_keyread=1;
      /* Don't use record cache */
      tbl->no_cache= 1;
      tbl->covering_keys.reset();
      if (tbl->file->has_transactions())
	transactional_tables= 1;
      else
	normal_tables= 1;
      tbl->prepare_for_position();
      tbl->mark_columns_needed_for_delete();
    }
    else if ((tab->type != JT_SYSTEM && tab->type != JT_CONST) &&
             walk == delete_tables)
    {
      /*
        We are not deleting from the table we are scanning. In this
        case send_data() shouldn't delete any rows a we may touch
        the rows in the deleted table many times
      */
      delete_while_scanning= 0;
    }
  }
  walk= delete_tables;
  tempfiles_ptr= tempfiles;
  if (delete_while_scanning)
  {
    table_being_deleted= delete_tables;
    walk= walk->next_local;
  }
  for (;walk ;walk= walk->next_local)
  {
    Table *table=walk->table;
    *tempfiles_ptr++= new Unique (refpos_order_cmp,
				  (void *) table->file,
				  table->file->ref_length,
				  current_session->variables.sortbuff_size);
  }
  return(session->is_fatal_error != 0);
}


multi_delete::~multi_delete()
{
  for (table_being_deleted= delete_tables;
       table_being_deleted;
       table_being_deleted= table_being_deleted->next_local)
  {
    Table *table= table_being_deleted->table;
    table->no_keyread=0;
  }

  for (uint32_t counter= 0; counter < num_of_tables; counter++)
  {
    if (tempfiles[counter])
      delete tempfiles[counter];
  }
}


bool multi_delete::send_data(List<Item> &)
{
  int secure_counter= delete_while_scanning ? -1 : 0;
  TableList *del_table;


  for (del_table= delete_tables;
       del_table;
       del_table= del_table->next_local, secure_counter++)
  {
    Table *table= del_table->table;

    /* Check if we are using outer join and we didn't find the row */
    if (table->status & (STATUS_NULL_ROW | STATUS_DELETED))
      continue;

    table->file->position(table->record[0]);
    found++;

    if (secure_counter < 0)
    {
      /* We are scanning the current table */
      assert(del_table == table_being_deleted);
      table->status|= STATUS_DELETED;
      if (!(error=table->file->ha_delete_row(table->record[0])))
      {
        deleted++;
        if (!table->file->has_transactions())
          session->transaction.stmt.modified_non_trans_table= true;
      }
      else
      {
        table->file->print_error(error,MYF(0));
        return(1);
      }
    }
    else
    {
      error=tempfiles[secure_counter]->unique_add((char*) table->file->ref);
      if (error)
      {
	error= 1;                               // Fatal error
	return(1);
      }
    }
  }
  return(0);
}


void multi_delete::send_error(uint32_t errcode,const char *err)
{


  /* First send error what ever it is ... */
  my_message(errcode, err, MYF(0));

  return;
}


void multi_delete::abort()
{


  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!session->transaction.stmt.modified_non_trans_table && !deleted))
    return;

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if (do_delete && normal_tables &&
      (table_being_deleted != delete_tables ||
       !table_being_deleted->table->file->has_transactions()))
  {
    /*
      We have to execute the recorded do_deletes() and write info into the
      error log
    */
    error= 1;
    send_eof();
    assert(error_handled);
    return;
  }

  if (session->transaction.stmt.modified_non_trans_table)
  {
    session->transaction.all.modified_non_trans_table= true;
  }
  return;
}



/*
  Do delete from other tables.
  Returns values:
	0 ok
	1 error
*/

int multi_delete::do_deletes()
{
  int local_error= 0, counter= 0, tmp_error;
  bool will_batch;

  assert(do_delete);

  do_delete= 0;                                 // Mark called
  if (!found)
    return(0);

  table_being_deleted= (delete_while_scanning ? delete_tables->next_local :
                        delete_tables);

  for (; table_being_deleted;
       table_being_deleted= table_being_deleted->next_local, counter++)
  {
    ha_rows last_deleted= deleted;
    Table *table = table_being_deleted->table;
    if (tempfiles[counter]->get(table))
    {
      local_error=1;
      break;
    }

    READ_RECORD	info;
    init_read_record(&info,session,table,NULL,0,1);
    /*
      Ignore any rows not found in reference tables as they may already have
      been deleted by foreign key handling
    */
    info.ignore_not_found_rows= 1;
    will_batch= !table->file->start_bulk_delete();
    while (!(local_error=info.read_record(&info)) && !session->killed)
    {
      if ((local_error=table->file->ha_delete_row(table->record[0])))
      {
	table->file->print_error(local_error,MYF(0));
	break;
      }
      deleted++;
    }
    if (will_batch && (tmp_error= table->file->end_bulk_delete()))
    {
      if (!local_error)
      {
        local_error= tmp_error;
        table->file->print_error(local_error,MYF(0));
      }
    }
    if (last_deleted != deleted && !table->file->has_transactions())
      session->transaction.stmt.modified_non_trans_table= true;
    end_read_record(&info);
    if (session->killed && !local_error)
      local_error= 1;
    if (local_error == -1)				// End of file
      local_error = 0;
  }
  return(local_error);
}


/*
  Send ok to the client

  return:  0 sucess
	   1 error
*/

bool multi_delete::send_eof()
{
  Session::killed_state killed_status= Session::NOT_KILLED;
  session->set_proc_info("deleting from reference tables");

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  int local_error= do_deletes();		// returns 0 if success

  /* compute a total error to know if something failed */
  local_error= local_error || error;
  killed_status= (local_error == 0)? Session::NOT_KILLED : session->killed;
  /* reset used flags */
  session->set_proc_info("end");

  if ((local_error == 0) || session->transaction.stmt.modified_non_trans_table)
  {
    if (session->transaction.stmt.modified_non_trans_table)
      session->transaction.all.modified_non_trans_table= true;
  }
  if (local_error != 0)
    error_handled= true; // to force early leave from ::send_error()

  if (!local_error)
  {
    session->row_count_func= deleted;
    session->my_ok((ha_rows) session->row_count_func);
  }
  return 0;
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
  if (!dont_send_ok && (table= find_temporary_table(session, table_list)))
  {
    StorageEngine *table_type= table->s->db_type();
    TableShare *share= table->s;

    if (!table_type->check_flag(HTON_BIT_CAN_RECREATE))
      goto trunc_by_del;

    table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);

    close_temporary_table(session, table, 0, 0);    // Don't free share
    ha_create_table(session, share->normalized_path.str,
                    share->db.str, share->table_name.str, &create_info, 1);
    // We don't need to call invalidate() because this table is not in cache
    if ((error= (int) !(open_temporary_table(session, share->path.str,
                                             share->db.str,
					     share->table_name.str, 1,
                                             OTM_OPEN))))
      (void) rm_temporary_table(table_type, path);
    share->free_table_share();
    free((char*) table);
    /*
      If we return here we will not have logged the truncation to the bin log
      and we will not my_ok() to the client.
    */
    goto end;
  }

  path_length= build_table_filename(path, sizeof(path), table_list->db,
                                    table_list->table_name, "", 0);

  if (!dont_send_ok)
    goto trunc_by_del;

  pthread_mutex_lock(&LOCK_open);
  error= ha_create_table(session, path, table_list->db, table_list->table_name,
                         &create_info, 1);
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
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(table_list);
    pthread_mutex_unlock(&LOCK_open);
  }
  else if (error)
  {
    pthread_mutex_lock(&LOCK_open);
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
