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
  Single table and multi table updates of tables.
  Multi-table updates were introduced by Sinisa & Monty
*/
#include <drizzled/server_includes.h>
#include <drizzled/sql_select.h>
#include <drizzled/drizzled_error_messages.h>

/*
  check that all fields are real fields

  SYNOPSIS
    check_fields()
    thd             thread handler
    items           Items for check

  RETURN
    true  Items can't be used in UPDATE
    false Items are OK
*/

static bool check_fields(THD *thd, List<Item> &items)
{
  List_iterator<Item> it(items);
  Item *item;
  Item_field *field;

  while ((item= it++))
  {
    if (!(field= item->filed_for_view_update()))
    {
      /* item has name, because it comes from VIEW SELECT list */
      my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), item->name);
      return true;
    }
    /*
      we make temporary copy of Item_field, to avoid influence of changing
      result_field on Item_ref which refer on this field
    */
    thd->change_item_tree(it.ref(), new Item_field(thd, field));
  }
  return false;
}


/**
  Re-read record if more columns are needed for error message.

  If we got a duplicate key error, we want to write an error
  message containing the value of the duplicate key. If we do not have
  all fields of the key value in record[0], we need to re-read the
  record with a proper read_set.

  @param[in] error   error number
  @param[in] table   table
*/

static void prepare_record_for_error_message(int error, Table *table)
{
  Field **field_p;
  Field *field;
  uint keynr;
  MY_BITMAP unique_map; /* Fields in offended unique. */
  my_bitmap_map unique_map_buf[bitmap_buffer_size(MAX_FIELDS)];
  
  /*
    Only duplicate key errors print the key value.
    If storage engine does always read all columns, we have the value alraedy.
  */
  if ((error != HA_ERR_FOUND_DUPP_KEY) ||
      !(table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ))
    return;

  /*
    Get the number of the offended index.
    We will see MAX_KEY if the engine cannot determine the affected index.
  */
  if ((keynr= table->file->get_dup_key(error)) >= MAX_KEY)
    return;

  /* Create unique_map with all fields used by that index. */
  bitmap_init(&unique_map, unique_map_buf, table->s->fields, false);
  table->mark_columns_used_by_index_no_reset(keynr, &unique_map);

  /* Subtract read_set and write_set. */
  bitmap_subtract(&unique_map, table->read_set);
  bitmap_subtract(&unique_map, table->write_set);

  /*
    If the unique index uses columns that are neither in read_set
    nor in write_set, we must re-read the record.
    Otherwise no need to do anything.
  */
  if (bitmap_is_clear_all(&unique_map))
    return;

  /* Get identifier of last read record into table->file->ref. */
  table->file->position(table->record[0]);
  /* Add all fields used by unique index to read_set. */
  bitmap_union(table->read_set, &unique_map);
  /* Tell the engine about the new set. */
  table->file->column_bitmaps_signal();
  /* Read record that is identified by table->file->ref. */
  (void) table->file->rnd_pos(table->record[1], table->file->ref);
  /* Copy the newly read columns into the new record. */
  for (field_p= table->field; (field= *field_p); field_p++)
    if (bitmap_is_set(&unique_map, field->field_index))
      field->copy_from_tmp(table->s->rec_buff_length);

  return;
}


/*
  Process usual UPDATE

  SYNOPSIS
    mysql_update()
    thd			thread handler
    fields		fields for update
    values		values of fields for update
    conds		WHERE clause expression
    order_num		number of elemen in order_st BY clause
    order		order_st BY clause list
    limit		limit clause
    handle_duplicates	how to handle duplicates

  RETURN
    0  - OK
    2  - privilege check and openning table passed, but we need to convert to
         multi-update because of view substitution
    1  - error
*/

int mysql_update(THD *thd,
                 TableList *table_list,
                 List<Item> &fields,
                 List<Item> &values,
                 COND *conds,
                 uint order_num, order_st *order,
                 ha_rows limit,
                 enum enum_duplicates handle_duplicates __attribute__((unused)),
                 bool ignore)
{
  bool		using_limit= limit != HA_POS_ERROR;
  bool		safe_update= test(thd->options & OPTION_SAFE_UPDATES);
  bool		used_key_is_modified, transactional_table, will_batch;
  bool		can_compare_record;
  int		error, loc_error;
  uint		used_index= MAX_KEY, dup_key_found;
  bool          need_sort= true;
  uint          table_count= 0;
  ha_rows	updated, found;
  key_map	old_covering_keys;
  Table		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  SELECT_LEX    *select_lex= &thd->lex->select_lex;
  bool          need_reopen;
  uint64_t     id;
  List<Item> all_fields;
  THD::killed_state killed_status= THD::NOT_KILLED;
  
  for ( ; ; )
  {
    if (open_tables(thd, &table_list, &table_count, 0))
      return(1);

    if (!lock_tables(thd, table_list, table_count, &need_reopen))
      break;
    if (!need_reopen)
      return(1);
    close_tables_for_reopen(thd, &table_list);
  }

  if (mysql_handle_derived(thd->lex, &mysql_derived_prepare) ||
      (thd->fill_derived_tables() &&
       mysql_handle_derived(thd->lex, &mysql_derived_filling)))
    return(1);

  DRIZZLE_UPDATE_START();
  thd_proc_info(thd, "init");
  table= table_list->table;

  /* Calculate "table->covering_keys" based on the WHERE */
  table->covering_keys= table->s->keys_in_use;
  table->quick_keys.clear_all();

  if (mysql_prepare_update(thd, table_list, &conds, order_num, order))
    goto abort;

  old_covering_keys= table->covering_keys;		// Keys used in WHERE
  /* Check the fields we are going to modify */
  if (setup_fields_with_no_wrap(thd, 0, fields, MARK_COLUMNS_WRITE, 0, 0))
    goto abort;                               /* purecov: inspected */
  if (table->timestamp_field)
  {
    // Don't set timestamp column if this is modified
    if (bitmap_is_set(table->write_set,
                      table->timestamp_field->field_index))
      table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    else
    {
      if (table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_UPDATE ||
          table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH)
        bitmap_set_bit(table->write_set,
                       table->timestamp_field->field_index);
    }
  }

  if (setup_fields(thd, 0, values, MARK_COLUMNS_READ, 0, 0))
  {
    free_underlaid_joins(thd, select_lex);
    goto abort;                               /* purecov: inspected */
  }

  if (select_lex->inner_refs_list.elements &&
    fix_inner_refs(thd, all_fields, select_lex, select_lex->ref_pointer_array))
  {
    DRIZZLE_UPDATE_END();
    return(-1);
  }

  if (conds)
  {
    Item::cond_result cond_value;
    conds= remove_eq_conds(thd, conds, &cond_value);
    if (cond_value == Item::COND_FALSE)
      limit= 0;                                   // Impossible WHERE
  }

  /*
    If a timestamp field settable on UPDATE is present then to avoid wrong
    update force the table handler to retrieve write-only fields to be able
    to compare records and detect data change.
  */
  if (table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ &&
      table->timestamp_field &&
      (table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_UPDATE ||
       table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH))
    bitmap_union(table->read_set, table->write_set);
  // Don't count on usage of 'only index' when calculating which key to use
  table->covering_keys.clear_all();

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  select= make_select(table, 0, 0, conds, 0, &error);
  if (error || !limit ||
      (select && select->check_quick(thd, safe_update, limit)))
  {
    delete select;
    free_underlaid_joins(thd, select_lex);
    if (error)
      goto abort;				// Error in where
    DRIZZLE_UPDATE_END();
    my_ok(thd);				// No matching records
    return(0);
  }
  if (!select && limit != HA_POS_ERROR)
  {
    if ((used_index= get_index_for_order(table, order, limit)) != MAX_KEY)
      need_sort= false;
  }
  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.is_clear_all())
  {
    thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
		 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      goto err;
    }
  }

  table->mark_columns_needed_for_update();

  /* Check if we are modifying a key that we are used to search with */
  
  if (select && select->quick)
  {
    used_index= select->quick->index;
    used_key_is_modified= (!select->quick->unique_key_range() &&
                          select->quick->is_keys_used(table->write_set));
  }
  else
  {
    used_key_is_modified= 0;
    if (used_index == MAX_KEY)                  // no index for sort order
      used_index= table->file->key_used_on_scan;
    if (used_index != MAX_KEY)
      used_key_is_modified= is_key_used(table, used_index, table->write_set);
  }


  if (used_key_is_modified || order)
  {
    /*
      We can't update table directly;  We must first search after all
      matching rows before updating the table!
    */
    if (used_index < MAX_KEY && old_covering_keys.is_set(used_index))
    {
      table->key_read=1;
      table->mark_columns_used_by_index(used_index);
    }
    else
    {
      table->use_all_columns();
    }

    /* note: We avoid sorting avoid if we sort on the used index */
    if (order && (need_sort || used_key_is_modified))
    {
      /*
	Doing an order_st BY;  Let filesort find and sort the rows we are going
	to update
        NOTE: filesort will call table->prepare_for_position()
      */
      uint         length= 0;
      SORT_FIELD  *sortorder;
      ha_rows examined_rows;

      table->sort.io_cache = (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
						    MYF(MY_FAE | MY_ZEROFILL));
      if (!(sortorder=make_unireg_sortorder(order, &length, NULL)) ||
          (table->sort.found_records= filesort(thd, table, sortorder, length,
                                               select, limit, 1,
                                               &examined_rows))
          == HA_POS_ERROR)
      {
	goto err;
      }
      /*
	Filesort has already found and selected the rows we want to update,
	so we don't need the where clause
      */
      delete select;
      select= 0;
    }
    else
    {
      /*
	We are doing a search on a key that is updated. In this case
	we go trough the matching rows, save a pointer to them and
	update these in a separate loop based on the pointer.
      */

      IO_CACHE tempfile;
      if (open_cached_file(&tempfile, mysql_tmpdir,TEMP_PREFIX,
			   DISK_BUFFER_SIZE, MYF(MY_WME)))
	goto err;

      /* If quick select is used, initialize it before retrieving rows. */
      if (select && select->quick && select->quick->reset())
        goto err;
      table->file->try_semi_consistent_read(1);

      /*
        When we get here, we have one of the following options:
        A. used_index == MAX_KEY
           This means we should use full table scan, and start it with
           init_read_record call
        B. used_index != MAX_KEY
           B.1 quick select is used, start the scan with init_read_record
           B.2 quick select is not used, this is full index scan (with LIMIT)
               Full index scan must be started with init_read_record_idx
      */

      if (used_index == MAX_KEY || (select && select->quick))
        init_read_record(&info,thd,table,select,0,1);
      else
        init_read_record_idx(&info, thd, table, 1, used_index);

      thd_proc_info(thd, "Searching rows for update");
      ha_rows tmp_limit= limit;

      while (!(error=info.read_record(&info)) && !thd->killed)
      {
	if (!(select && select->skip_record()))
	{
          if (table->file->was_semi_consistent_read())
	    continue;  /* repeat the read of the same row if it still exists */

	  table->file->position(table->record[0]);
	  if (my_b_write(&tempfile,table->file->ref,
			 table->file->ref_length))
	  {
	    error=1; /* purecov: inspected */
	    break; /* purecov: inspected */
	  }
	  if (!--limit && using_limit)
	  {
	    error= -1;
	    break;
	  }
	}
	else
	  table->file->unlock_row();
      }
      if (thd->killed && !error)
	error= 1;				// Aborted
      limit= tmp_limit;
      table->file->try_semi_consistent_read(0);
      end_read_record(&info);
     
      /* Change select to use tempfile */
      if (select)
      {
	delete select->quick;
	if (select->free_cond)
	  delete select->cond;
	select->quick=0;
	select->cond=0;
      }
      else
      {
	select= new SQL_SELECT;
	select->head=table;
      }
      if (reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
	error=1; /* purecov: inspected */
      select->file=tempfile;			// Read row ptrs from this file
      if (error >= 0)
	goto err;
    }
    if (table->key_read)
      table->restore_column_maps_after_mark_index();
  }

  if (ignore)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  
  if (select && select->quick && select->quick->reset())
    goto err;
  table->file->try_semi_consistent_read(1);
  init_read_record(&info,thd,table,select,0,1);

  updated= found= 0;
  /* Generate an error when trying to set a NOT NULL field to NULL. */
  thd->count_cuted_fields= ignore ? CHECK_FIELD_WARN
                                  : CHECK_FIELD_ERROR_FOR_NULL;
  thd->cuted_fields=0L;
  thd_proc_info(thd, "Updating");

  transactional_table= table->file->has_transactions();
  thd->abort_on_warning= test(!ignore);
  will_batch= !table->file->start_bulk_update();

  /*
    Assure that we can use position()
    if we need to create an error message.
  */
  if (table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ)
    table->prepare_for_position();

  /*
    We can use compare_record() to optimize away updates if
    the table handler is returning all columns OR if
    if all updated columns are read
  */
  can_compare_record= (!(table->file->ha_table_flags() &
                         HA_PARTIAL_COLUMN_READ) ||
                       bitmap_is_subset(table->write_set, table->read_set));

  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    if (!(select && select->skip_record()))
    {
      if (table->file->was_semi_consistent_read())
        continue;  /* repeat the read of the same row if it still exists */

      store_record(table,record[1]);
      if (fill_record(thd, fields, values, 0))
        break; /* purecov: inspected */

      found++;

      if (!can_compare_record || table->compare_record())
      {
        if (will_batch)
        {
          /*
            Typically a batched handler can execute the batched jobs when:
            1) When specifically told to do so
            2) When it is not a good idea to batch anymore
            3) When it is necessary to send batch for other reasons
               (One such reason is when READ's must be performed)

            1) is covered by exec_bulk_update calls.
            2) and 3) is handled by the bulk_update_row method.
            
            bulk_update_row can execute the updates including the one
            defined in the bulk_update_row or not including the row
            in the call. This is up to the handler implementation and can
            vary from call to call.

            The dup_key_found reports the number of duplicate keys found
            in those updates actually executed. It only reports those if
            the extra call with HA_EXTRA_IGNORE_DUP_KEY have been issued.
            If this hasn't been issued it returns an error code and can
            ignore this number. Thus any handler that implements batching
            for UPDATE IGNORE must also handle this extra call properly.

            If a duplicate key is found on the record included in this
            call then it should be included in the count of dup_key_found
            and error should be set to 0 (only if these errors are ignored).
          */
          error= table->file->ha_bulk_update_row(table->record[1],
                                                 table->record[0],
                                                 &dup_key_found);
          limit+= dup_key_found;
          updated-= dup_key_found;
        }
        else
        {
          /* Non-batched update */
	  error= table->file->ha_update_row(table->record[1],
                                            table->record[0]);
        }
        if (!error || error == HA_ERR_RECORD_IS_THE_SAME)
	{
          if (error != HA_ERR_RECORD_IS_THE_SAME)
            updated++;
          else
            error= 0;
	}
 	else if (!ignore ||
                 table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
	{
          /*
            If (ignore && error is ignorable) we don't have to
            do anything; otherwise...
          */
          myf flags= 0;

          if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
            flags|= ME_FATALERROR; /* Other handler errors are fatal */

          prepare_record_for_error_message(error, table);
	  table->file->print_error(error,MYF(flags));
	  error= 1;
	  break;
	}
      }

      if (!--limit && using_limit)
      {
        /*
          We have reached end-of-file in most common situations where no
          batching has occurred and if batching was supposed to occur but
          no updates were made and finally when the batch execution was
          performed without error and without finding any duplicate keys.
          If the batched updates were performed with errors we need to
          check and if no error but duplicate key's found we need to
          continue since those are not counted for in limit.
        */
        if (will_batch &&
            ((error= table->file->exec_bulk_update(&dup_key_found)) ||
             dup_key_found))
        {
 	  if (error)
          {
            /* purecov: begin inspected */
            /*
              The handler should not report error of duplicate keys if they
              are ignored. This is a requirement on batching handlers.
            */
            prepare_record_for_error_message(error, table);
            table->file->print_error(error,MYF(0));
            error= 1;
            break;
            /* purecov: end */
          }
          /*
            Either an error was found and we are ignoring errors or there
            were duplicate keys found. In both cases we need to correct
            the counters and continue the loop.
          */
          limit= dup_key_found; //limit is 0 when we get here so need to +
          updated-= dup_key_found;
        }
        else
        {
	  error= -1;				// Simulate end of file
	  break;
        }
      }
    }
    else
      table->file->unlock_row();
    thd->row_count++;
  }
  dup_key_found= 0;
  /*
    Caching the killed status to pass as the arg to query event constuctor;
    The cached value can not change whereas the killed status can
    (externally) since this point and change of the latter won't affect
    binlogging.
    It's assumed that if an error was set in combination with an effective
    killed status then the error is due to killing.
  */
  killed_status= thd->killed; // get the status of the volatile
  // simulated killing after the loop must be ineffective for binlogging
  error= (killed_status == THD::NOT_KILLED)?  error : 1;

  if (error &&
      will_batch &&
      (loc_error= table->file->exec_bulk_update(&dup_key_found)))
    /*
      An error has occurred when a batched update was performed and returned
      an error indication. It cannot be an allowed duplicate key error since
      we require the batching handler to treat this as a normal behavior.

      Otherwise we simply remove the number of duplicate keys records found
      in the batched update.
    */
  {
    /* purecov: begin inspected */
    prepare_record_for_error_message(loc_error, table);
    table->file->print_error(loc_error,MYF(ME_FATALERROR));
    error= 1;
    /* purecov: end */
  }
  else
    updated-= dup_key_found;
  if (will_batch)
    table->file->end_bulk_update();
  table->file->try_semi_consistent_read(0);

  if (!transactional_table && updated > 0)
    thd->transaction.stmt.modified_non_trans_table= true;

  end_read_record(&info);
  delete select;
  thd_proc_info(thd, "end");
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  /*
    error < 0 means really no error at all: we processed all rows until the
    last one without error. error > 0 means an error (e.g. unique key
    violation and no IGNORE or REPLACE). error == 0 is also an error (if
    preparing the record or invoking before triggers fails). See
    ha_autocommit_or_rollback(error>=0) and return(error>=0) below.
    Sometimes we want to binlog even if we updated no rows, in case user used
    it to be sure master and slave are in same state.
  */
  if ((error < 0) || thd->transaction.stmt.modified_non_trans_table)
  {
    if (mysql_bin_log.is_open())
    {
      if (error < 0)
        thd->clear_error();
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query, thd->query_length,
                            transactional_table, false, killed_status) &&
          transactional_table)
      {
        error=1;				// Rollback update
      }
    }
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= true;
  }
  assert(transactional_table || !updated || thd->transaction.stmt.modified_non_trans_table);
  free_underlaid_joins(thd, select_lex);

  /* If LAST_INSERT_ID(X) was used, report X */
  id= thd->arg_of_last_insert_id_function ?
    thd->first_successful_insert_id_in_prev_stmt : 0;

  DRIZZLE_UPDATE_END();
  if (error < 0)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    sprintf(buff, ER(ER_UPDATE_INFO), (ulong) found, (ulong) updated,
	    (ulong) thd->cuted_fields);
    thd->row_count_func=
      (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated;
    my_ok(thd, (ulong) thd->row_count_func, id, buff);
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;		/* calc cuted fields */
  thd->abort_on_warning= 0;
  return((error >= 0 || thd->is_error()) ? 1 : 0);

err:
  delete select;
  free_underlaid_joins(thd, select_lex);
  if (table->key_read)
  {
    table->key_read=0;
    table->file->extra(HA_EXTRA_NO_KEYREAD);
  }
  thd->abort_on_warning= 0;

abort:
  DRIZZLE_UPDATE_END();
  return(1);
}

/*
  Prepare items in UPDATE statement

  SYNOPSIS
    mysql_prepare_update()
    thd			- thread handler
    table_list		- global/local table list
    conds		- conditions
    order_num		- number of order_st BY list entries
    order		- order_st BY clause list

  RETURN VALUE
    false OK
    true  error
*/
bool mysql_prepare_update(THD *thd, TableList *table_list,
			 Item **conds, uint order_num, order_st *order)
{
  List<Item> all_fields;
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  
  /*
    Statement-based replication of UPDATE ... LIMIT is not safe as order of
    rows is not defined, so in mixed mode we go to row-based.

    Note that we may consider a statement as safe if order_st BY primary_key
    is present. However it may confuse users to see very similiar statements
    replicated differently.
  */
  if (thd->lex->current_select->select_limit)
  {
    thd->lex->set_stmt_unsafe();
    thd->set_current_stmt_binlog_row_based_if_mixed();
  }

  thd->lex->allow_sum_func= 0;

  if (setup_tables_and_check_access(thd, &select_lex->context, 
                                    &select_lex->top_join_list,
                                    table_list,
                                    &select_lex->leaf_tables,
                                    false) ||
      setup_conds(thd, table_list, select_lex->leaf_tables, conds) ||
      select_lex->setup_ref_array(thd, order_num) ||
      setup_order(thd, select_lex->ref_pointer_array,
		  table_list, all_fields, all_fields, order))
    return(true);

  /* Check that we are not using table that we are updating in a sub select */
  {
    TableList *duplicate;
    if ((duplicate= unique_table(thd, table_list, table_list->next_global, 0)))
    {
      update_non_unique_table_error(table_list, "UPDATE", duplicate);
      my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->table_name);
      return(true);
    }
  }

  return(false);
}


/***************************************************************************
  Update multiple tables from join 
***************************************************************************/

/*
  Get table map for list of Item_field
*/

static table_map get_table_map(List<Item> *items)
{
  List_iterator_fast<Item> item_it(*items);
  Item_field *item;
  table_map map= 0;

  while ((item= (Item_field *) item_it++)) 
    map|= item->used_tables();
  return map;
}


/*
  make update specific preparation and checks after opening tables

  SYNOPSIS
    mysql_multi_update_prepare()
    thd         thread handler

  RETURN
    false OK
    true  Error
*/

int mysql_multi_update_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  TableList *table_list= lex->query_tables;
  TableList *tl, *leaves;
  List<Item> *fields= &lex->select_lex.item_list;
  table_map tables_for_update;
  bool update_view= 0;
  /*
    if this multi-update was converted from usual update, here is table
    counter else junk will be assigned here, but then replaced with real
    count in open_tables()
  */
  uint  table_count= lex->table_count;
  const bool using_lock_tables= thd->locked_tables != 0;
  bool original_multiupdate= (thd->lex->sql_command == SQLCOM_UPDATE_MULTI);
  bool need_reopen= false;
  

  /* following need for prepared statements, to run next time multi-update */
  thd->lex->sql_command= SQLCOM_UPDATE_MULTI;

reopen_tables:

  /* open tables and create derived ones, but do not lock and fill them */
  if (((original_multiupdate || need_reopen) &&
       open_tables(thd, &table_list, &table_count, 0)) ||
      mysql_handle_derived(lex, &mysql_derived_prepare))
    return(true);
  /*
    setup_tables() need for VIEWs. JOIN::prepare() will call setup_tables()
    second time, but this call will do nothing (there are check for second
    call in setup_tables()).
  */

  if (setup_tables_and_check_access(thd, &lex->select_lex.context,
                                    &lex->select_lex.top_join_list,
                                    table_list,
                                    &lex->select_lex.leaf_tables, false))
    return(true);

  if (setup_fields_with_no_wrap(thd, 0, *fields, MARK_COLUMNS_WRITE, 0, 0))
    return(true);

  if (update_view && check_fields(thd, *fields))
  {
    return(true);
  }

  tables_for_update= get_table_map(fields);

  /*
    Setup timestamp handling and locking mode
  */
  leaves= lex->select_lex.leaf_tables;
  for (tl= leaves; tl; tl= tl->next_leaf)
  {
    Table *table= tl->table;
    /* Only set timestamp column if this is not modified */
    if (table->timestamp_field &&
        bitmap_is_set(table->write_set,
                      table->timestamp_field->field_index))
      table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

    /* if table will be updated then check that it is unique */
    if (table->map & tables_for_update)
    {
      table->mark_columns_needed_for_update();
      /*
        If table will be updated we should not downgrade lock for it and
        leave it as is.
      */
    }
    else
    {
      /*
        If we are using the binary log, we need TL_READ_NO_INSERT to get
        correct order of statements. Otherwise, we use a TL_READ lock to
        improve performance.
      */
      tl->lock_type= using_update_log ? TL_READ_NO_INSERT : TL_READ;
      tl->updating= 0;
      /* Update Table::lock_type accordingly. */
      if (!tl->placeholder() && !using_lock_tables)
        tl->table->reginfo.lock_type= tl->lock_type;
    }
  }

  /* now lock and fill tables */
  if (lock_tables(thd, table_list, table_count, &need_reopen))
  {
    if (!need_reopen)
      return(true);

    /*
      We have to reopen tables since some of them were altered or dropped
      during lock_tables() or something was done with their triggers.
      Let us do some cleanups to be able do setup_table() and setup_fields()
      once again.
    */
    List_iterator_fast<Item> it(*fields);
    Item *item;
    while ((item= it++))
      item->cleanup();

    /* We have to cleanup translation tables of views. */
    for (TableList *tbl= table_list; tbl; tbl= tbl->next_global)
      tbl->cleanup_items();

    close_tables_for_reopen(thd, &table_list);
    goto reopen_tables;
  }

  /*
    Check that we are not using table that we are updating, but we should
    skip all tables of UPDATE SELECT itself
  */
  lex->select_lex.exclude_from_table_unique_test= true;
  /* We only need SELECT privilege for columns in the values list */
  for (tl= leaves; tl; tl= tl->next_leaf)
  {
    if (tl->lock_type != TL_READ &&
        tl->lock_type != TL_READ_NO_INSERT)
    {
      TableList *duplicate;
      if ((duplicate= unique_table(thd, tl, table_list, 0)))
      {
        update_non_unique_table_error(table_list, "UPDATE", duplicate);
        return(true);
      }
    }
  }
  /*
    Set exclude_from_table_unique_test value back to false. It is needed for
    further check in multi_update::prepare whether to use record cache.
  */
  lex->select_lex.exclude_from_table_unique_test= false;
 
  if (thd->fill_derived_tables() &&
      mysql_handle_derived(lex, &mysql_derived_filling))
    return(true);

  return (false);
}


/*
  Setup multi-update handling and call SELECT to do the join
*/

bool mysql_multi_update(THD *thd,
                        TableList *table_list,
                        List<Item> *fields,
                        List<Item> *values,
                        COND *conds,
                        uint64_t options,
                        enum enum_duplicates handle_duplicates, bool ignore,
                        SELECT_LEX_UNIT *unit, SELECT_LEX *select_lex)
{
  multi_update *result;
  bool res;
  
  if (!(result= new multi_update(table_list,
				 thd->lex->select_lex.leaf_tables,
				 fields, values,
				 handle_duplicates, ignore)))
    return(true);

  thd->abort_on_warning= true;

  List<Item> total_list;
  res= mysql_select(thd, &select_lex->ref_pointer_array,
                      table_list, select_lex->with_wild,
                      total_list,
                      conds, 0, (order_st *) NULL, (order_st *)NULL, (Item *) NULL,
                      (order_st *)NULL,
                      options | SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK |
                      OPTION_SETUP_TABLES_DONE,
                      result, unit, select_lex);
  res|= thd->is_error();
  if (unlikely(res))
  {
    /* If we had a another error reported earlier then this will be ignored */
    result->send_error(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR));
    result->abort();
  }
  delete result;
  thd->abort_on_warning= 0;
  return(false);
}


multi_update::multi_update(TableList *table_list,
			   TableList *leaves_list,
			   List<Item> *field_list, List<Item> *value_list,
			   enum enum_duplicates handle_duplicates_arg,
                           bool ignore_arg)
  :all_tables(table_list), leaves(leaves_list), update_tables(0),
   tmp_tables(0), updated(0), found(0), fields(field_list),
   values(value_list), table_count(0), copy_field(0),
   handle_duplicates(handle_duplicates_arg), do_update(1), trans_safe(1),
   transactional_tables(0), ignore(ignore_arg), error_handled(0)
{}


/*
  Connect fields with tables and create list of tables that are updated
*/

int multi_update::prepare(List<Item> &not_used_values __attribute__((unused)),
                          SELECT_LEX_UNIT *lex_unit __attribute__((unused)))
{
  TableList *table_ref;
  SQL_LIST update;
  table_map tables_to_update;
  Item_field *item;
  List_iterator_fast<Item> field_it(*fields);
  List_iterator_fast<Item> value_it(*values);
  uint i, max_fields;
  uint leaf_table_count= 0;
  
  thd->count_cuted_fields= CHECK_FIELD_WARN;
  thd->cuted_fields=0L;
  thd_proc_info(thd, "updating main table");

  tables_to_update= get_table_map(fields);

  if (!tables_to_update)
  {
    my_message(ER_NO_TABLES_USED, ER(ER_NO_TABLES_USED), MYF(0));
    return(1);
  }

  /*
    We have to check values after setup_tables to get covering_keys right in
    reference tables
  */

  if (setup_fields(thd, 0, *values, MARK_COLUMNS_READ, 0, 0))
    return(1);

  /*
    Save tables beeing updated in update_tables
    update_table->shared is position for table
    Don't use key read on tables that are updated
  */

  update.empty();
  for (table_ref= leaves; table_ref; table_ref= table_ref->next_leaf)
  {
    /* TODO: add support of view of join support */
    Table *table=table_ref->table;
    leaf_table_count++;
    if (tables_to_update & table->map)
    {
      TableList *tl= (TableList*) thd->memdup((char*) table_ref,
						sizeof(*tl));
      if (!tl)
	return(1);
      update.link_in_list((uchar*) tl, (uchar**) &tl->next_local);
      tl->shared= table_count++;
      table->no_keyread=1;
      table->covering_keys.clear_all();
      table->pos_in_table_list= tl;
    }
  }


  table_count=  update.elements;
  update_tables= (TableList*) update.first;

  tmp_tables = (Table**) thd->calloc(sizeof(Table *) * table_count);
  tmp_table_param = (TMP_TABLE_PARAM*) thd->calloc(sizeof(TMP_TABLE_PARAM) *
						   table_count);
  fields_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  values_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  if (thd->is_fatal_error)
    return(1);
  for (i=0 ; i < table_count ; i++)
  {
    fields_for_table[i]= new List_item;
    values_for_table[i]= new List_item;
  }
  if (thd->is_fatal_error)
    return(1);

  /* Split fields into fields_for_table[] and values_by_table[] */

  while ((item= (Item_field *) field_it++))
  {
    Item *value= value_it++;
    uint offset= item->field->table->pos_in_table_list->shared;
    fields_for_table[offset]->push_back(item);
    values_for_table[offset]->push_back(value);
  }
  if (thd->is_fatal_error)
    return(1);

  /* Allocate copy fields */
  max_fields=0;
  for (i=0 ; i < table_count ; i++)
    set_if_bigger(max_fields, fields_for_table[i]->elements + leaf_table_count);
  copy_field= new Copy_field[max_fields];
  return(thd->is_fatal_error != 0);
}


/*
  Check if table is safe to update on fly

  SYNOPSIS
    safe_update_on_fly()
    thd                 Thread handler
    join_tab            How table is used in join
    all_tables          List of tables

  NOTES
    We can update the first table in join on the fly if we know that
    a row in this table will never be read twice. This is true under
    the following conditions:

    - We are doing a table scan and the data is in a separate file (MyISAM) or
      if we don't update a clustered key.

    - We are doing a range scan and we don't update the scan key or
      the primary key for a clustered table handler.

    - Table is not joined to itself.

    This function gets information about fields to be updated from
    the Table::write_set bitmap.

  WARNING
    This code is a bit dependent of how make_join_readinfo() works.

  RETURN
    0		Not safe to update
    1		Safe to update
*/

static bool safe_update_on_fly(THD *thd, JOIN_TAB *join_tab,
                               TableList *table_ref, TableList *all_tables)
{
  Table *table= join_tab->table;
  if (unique_table(thd, table_ref, all_tables, 0))
    return 0;
  switch (join_tab->type) {
  case JT_SYSTEM:
  case JT_CONST:
  case JT_EQ_REF:
    return true;				// At most one matching row
  case JT_REF:
  case JT_REF_OR_NULL:
    return !is_key_used(table, join_tab->ref.key, table->write_set);
  case JT_ALL:
    /* If range search on index */
    if (join_tab->quick)
      return !join_tab->quick->is_keys_used(table->write_set);
    /* If scanning in clustered key */
    if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
	table->s->primary_key < MAX_KEY)
      return !is_key_used(table, table->s->primary_key, table->write_set);
    return true;
  default:
    break;					// Avoid compler warning
  }
  return false;

}


/*
  Initialize table for multi table

  IMPLEMENTATION
    - Update first table in join on the fly, if possible
    - Create temporary tables to store changed values for all other tables
      that are updated (and main_table if the above doesn't hold).
*/

bool
multi_update::initialize_tables(JOIN *join)
{
  TableList *table_ref;
  
  if ((thd->options & OPTION_SAFE_UPDATES) && error_if_full_join(join))
    return(1);
  main_table=join->join_tab->table;
  table_to_update= 0;

  /* Any update has at least one pair (field, value) */
  assert(fields->elements);

  /* Create a temporary table for keys to all tables, except main table */
  for (table_ref= update_tables; table_ref; table_ref= table_ref->next_local)
  {
    Table *table=table_ref->table;
    uint cnt= table_ref->shared;
    List<Item> temp_fields;
    order_st     group;
    TMP_TABLE_PARAM *tmp_param;

    table->mark_columns_needed_for_update();
    if (ignore)
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table == main_table)			// First table in join
    {
      if (safe_update_on_fly(thd, join->join_tab, table_ref, all_tables))
      {
	table_to_update= main_table;		// Update table on the fly
	continue;
      }
    }
    table->prepare_for_position();

    tmp_param= tmp_table_param+cnt;

    /*
      Create a temporary table to store all fields that are changed for this
      table. The first field in the temporary table is a pointer to the
      original row so that we can find and update it. For the updatable
      VIEW a few following fields are rowids of tables used in the CHECK
      OPTION condition.
    */

    List_iterator_fast<Table> tbl_it(unupdated_check_opt_tables);
    Table *tbl= table;
    do
    {
      Field_string *field= new Field_string(tbl->file->ref_length, 0,
                                            tbl->alias, &my_charset_bin);
#ifdef OLD
      Field_varstring *field= new Field_varstring(tbl->file->ref_length, 0,
                                                  tbl->alias, tbl->s, &my_charset_bin);
#endif
      if (!field)
        return(1);
      field->init(tbl);
      /*
        The field will be converted to varstring when creating tmp table if
        table to be updated was created by mysql 4.1. Deny this.
      */
      Item_field *ifield= new Item_field((Field *) field);
      if (!ifield)
         return(1);
      ifield->maybe_null= 0;
      if (temp_fields.push_back(ifield))
        return(1);
    } while ((tbl= tbl_it++));

    temp_fields.concat(fields_for_table[cnt]);

    /* Make an unique key over the first field to avoid duplicated updates */
    memset(&group, 0, sizeof(group));
    group.asc= 1;
    group.item= (Item**) temp_fields.head_ref();

    tmp_param->quick_group=1;
    tmp_param->field_count=temp_fields.elements;
    tmp_param->group_parts=1;
    tmp_param->group_length= table->file->ref_length;
    if (!(tmp_tables[cnt]=create_tmp_table(thd,
					   tmp_param,
					   temp_fields,
					   (order_st*) &group, 0, 0,
					   TMP_TABLE_ALL_COLUMNS,
					   HA_POS_ERROR,
					   (char *) "")))
      return(1);
    tmp_tables[cnt]->file->extra(HA_EXTRA_WRITE_CACHE);
  }
  return(0);
}


multi_update::~multi_update()
{
  TableList *table;
  for (table= update_tables ; table; table= table->next_local)
  {
    table->table->no_keyread= table->table->no_cache= 0;
    if (ignore)
      table->table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  }

  if (tmp_tables)
  {
    for (uint cnt = 0; cnt < table_count; cnt++)
    {
      if (tmp_tables[cnt])
      {
	tmp_tables[cnt]->free_tmp_table(thd);
	tmp_table_param[cnt].cleanup();
      }
    }
  }
  if (copy_field)
    delete [] copy_field;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;		// Restore this setting
  assert(trans_safe || !updated ||
              thd->transaction.all.modified_non_trans_table);
}


bool multi_update::send_data(List<Item> &not_used_values __attribute__((unused)))
{
  TableList *cur_table;
  
  for (cur_table= update_tables; cur_table; cur_table= cur_table->next_local)
  {
    Table *table= cur_table->table;
    uint offset= cur_table->shared;
    /*
      Check if we are using outer join and we didn't find the row
      or if we have already updated this row in the previous call to this
      function.

      The same row may be presented here several times in a join of type
      UPDATE t1 FROM t1,t2 SET t1.a=t2.a

      In this case we will do the update for the first found row combination.
      The join algorithm guarantees that we will not find the a row in
      t1 several times.
    */
    if (table->status & (STATUS_NULL_ROW | STATUS_UPDATED))
      continue;

    /*
      We can use compare_record() to optimize away updates if
      the table handler is returning all columns OR if
      if all updated columns are read
    */
    if (table == table_to_update)
    {
      bool can_compare_record;
      can_compare_record= (!(table->file->ha_table_flags() &
                             HA_PARTIAL_COLUMN_READ) ||
                           bitmap_is_subset(table->write_set,
                                            table->read_set));
      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);
      if (fill_record(thd, *fields_for_table[offset],
                      *values_for_table[offset], 0))
	return(1);

      found++;
      if (!can_compare_record || table->compare_record())
      {
	int error;
        if (!updated++)
        {
          /*
            Inform the main table that we are going to update the table even
            while we may be scanning it.  This will flush the read cache
            if it's used.
          */
          main_table->file->extra(HA_EXTRA_PREPARE_FOR_UPDATE);
        }
        if ((error=table->file->ha_update_row(table->record[1],
                                              table->record[0])) &&
            error != HA_ERR_RECORD_IS_THE_SAME)
        {
          updated--;
          if (!ignore ||
              table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
          {
            /*
              If (ignore && error == is ignorable) we don't have to
              do anything; otherwise...
            */
            myf flags= 0;

            if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
              flags|= ME_FATALERROR; /* Other handler errors are fatal */

            prepare_record_for_error_message(error, table);
            table->file->print_error(error,MYF(flags));
            return(1);
          }
        }
        else
        {
          if (error == HA_ERR_RECORD_IS_THE_SAME)
          {
            error= 0;
            updated--;
          }
          /* non-transactional or transactional table got modified   */
          /* either multi_update class' flag is raised in its branch */
          if (table->file->has_transactions())
            transactional_tables= 1;
          else
          {
            trans_safe= 0;
            thd->transaction.stmt.modified_non_trans_table= true;
          }
        }
      }
    }
    else
    {
      int error;
      Table *tmp_table= tmp_tables[offset];
      /*
       For updatable VIEW store rowid of the updated table and
       rowids of tables used in the CHECK OPTION condition.
      */
      uint field_num= 0;
      List_iterator_fast<Table> tbl_it(unupdated_check_opt_tables);
      Table *tbl= table;
      do
      {
        tbl->file->position(tbl->record[0]);
        memcpy(tmp_table->field[field_num]->ptr,
               tbl->file->ref, tbl->file->ref_length);
        field_num++;
      } while ((tbl= tbl_it++));

      /* Store regular updated fields in the row. */
      fill_record(thd,
                  tmp_table->field + 1 + unupdated_check_opt_tables.elements,
                  *values_for_table[offset], 1);

      /* Write row, ignoring duplicated updates to a row */
      error= tmp_table->file->ha_write_row(tmp_table->record[0]);
      if (error != HA_ERR_FOUND_DUPP_KEY && error != HA_ERR_FOUND_DUPP_UNIQUE)
      {
        if (error &&
            create_myisam_from_heap(thd, tmp_table,
                                         tmp_table_param[offset].start_recinfo,
                                         &tmp_table_param[offset].recinfo,
                                         error, 1))
        {
          do_update=0;
	  return(1);			// Not a table_is_full error
	}
        found++;
      }
    }
  }
  return(0);
}


void multi_update::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  my_error(errcode, MYF(0), err);
}


void multi_update::abort()
{
  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->transaction.stmt.modified_non_trans_table && !updated))
    return;
  /*
    If all tables that has been updated are trans safe then just do rollback.
    If not attempt to do remaining updates.
  */

  if (! trans_safe)
  {
    assert(thd->transaction.stmt.modified_non_trans_table);
    if (do_update && table_count > 1)
    {
      /* Add warning here */
      /* 
         todo/fixme: do_update() is never called with the arg 1.
         should it change the signature to become argless?
      */
      do_updates();
    }
  }
  if (thd->transaction.stmt.modified_non_trans_table)
  {
    /*
      The query has to binlog because there's a modified non-transactional table
      either from the query's list or via a stored routine: bug#13270,23333
    */
    if (mysql_bin_log.is_open())
    {
      /*
        THD::killed status might not have been set ON at time of an error
        got caught and if happens later the killed error is written
        into repl event.
      */
      thd->binlog_query(THD::ROW_QUERY_TYPE,
                        thd->query, thd->query_length,
                        transactional_tables, false);
    }
    thd->transaction.all.modified_non_trans_table= true;
  }
  assert(trans_safe || !updated || thd->transaction.stmt.modified_non_trans_table);
}


int multi_update::do_updates()
{
  TableList *cur_table;
  int local_error= 0;
  ha_rows org_updated;
  Table *table, *tmp_table;
  List_iterator_fast<Table> check_opt_it(unupdated_check_opt_tables);
  
  do_update= 0;					// Don't retry this function
  if (!found)
    return(0);
  for (cur_table= update_tables; cur_table; cur_table= cur_table->next_local)
  {
    bool can_compare_record;
    uint offset= cur_table->shared;

    table = cur_table->table;
    if (table == table_to_update)
      continue;					// Already updated
    org_updated= updated;
    tmp_table= tmp_tables[cur_table->shared];
    tmp_table->file->extra(HA_EXTRA_CACHE);	// Change to read cache
    (void) table->file->ha_rnd_init(0);
    table->file->extra(HA_EXTRA_NO_CACHE);

    check_opt_it.rewind();
    while(Table *tbl= check_opt_it++)
    {
      if (tbl->file->ha_rnd_init(1))
        goto err;
      tbl->file->extra(HA_EXTRA_CACHE);
    }

    /*
      Setup copy functions to copy fields from temporary table
    */
    List_iterator_fast<Item> field_it(*fields_for_table[offset]);
    Field **field= tmp_table->field + 
                   1 + unupdated_check_opt_tables.elements; // Skip row pointers
    Copy_field *copy_field_ptr= copy_field, *copy_field_end;
    for ( ; *field ; field++)
    {
      Item_field *item= (Item_field* ) field_it++;
      (copy_field_ptr++)->set(item->field, *field, 0);
    }
    copy_field_end=copy_field_ptr;

    if ((local_error = tmp_table->file->ha_rnd_init(1)))
      goto err;

    can_compare_record= (!(table->file->ha_table_flags() &
                           HA_PARTIAL_COLUMN_READ) ||
                         bitmap_is_subset(table->write_set,
                                          table->read_set));

    for (;;)
    {
      if (thd->killed && trans_safe)
	goto err;
      if ((local_error=tmp_table->file->rnd_next(tmp_table->record[0])))
      {
	if (local_error == HA_ERR_END_OF_FILE)
	  break;
	if (local_error == HA_ERR_RECORD_DELETED)
	  continue;				// May happen on dup key
	goto err;
      }

      /* call rnd_pos() using rowids from temporary table */
      check_opt_it.rewind();
      Table *tbl= table;
      uint field_num= 0;
      do
      {
        if((local_error=
              tbl->file->rnd_pos(tbl->record[0],
                                (uchar *) tmp_table->field[field_num]->ptr)))
          goto err;
        field_num++;
      } while((tbl= check_opt_it++));

      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);

      /* Copy data from temporary table to current table */
      for (copy_field_ptr=copy_field;
	   copy_field_ptr != copy_field_end;
	   copy_field_ptr++)
	(*copy_field_ptr->do_copy)(copy_field_ptr);

      if (!can_compare_record || table->compare_record())
      {
	if ((local_error=table->file->ha_update_row(table->record[1],
						    table->record[0])) &&
            local_error != HA_ERR_RECORD_IS_THE_SAME)
	{
	  if (!ignore ||
              table->file->is_fatal_error(local_error, HA_CHECK_DUP_KEY))
	    goto err;
	}
        if (local_error != HA_ERR_RECORD_IS_THE_SAME)
          updated++;
        else
          local_error= 0;
      }
    }

    if (updated != org_updated)
    {
      if (table->file->has_transactions())
        transactional_tables= 1;
      else
      {
        trans_safe= 0;				// Can't do safe rollback
        thd->transaction.stmt.modified_non_trans_table= true;
      }
    }
    (void) table->file->ha_rnd_end();
    (void) tmp_table->file->ha_rnd_end();
    check_opt_it.rewind();
    while (Table *tbl= check_opt_it++)
        tbl->file->ha_rnd_end();

  }
  return(0);

err:
  {
    prepare_record_for_error_message(local_error, table);
    table->file->print_error(local_error,MYF(ME_FATALERROR));
  }

  (void) table->file->ha_rnd_end();
  (void) tmp_table->file->ha_rnd_end();
  check_opt_it.rewind();
  while (Table *tbl= check_opt_it++)
      tbl->file->ha_rnd_end();

  if (updated != org_updated)
  {
    if (table->file->has_transactions())
      transactional_tables= 1;
    else
    {
      trans_safe= 0;
      thd->transaction.stmt.modified_non_trans_table= true;
    }
  }
  return(1);
}


/* out: 1 if error, 0 if success */

bool multi_update::send_eof()
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  uint64_t id;
  THD::killed_state killed_status= THD::NOT_KILLED;
  
  thd_proc_info(thd, "updating reference tables");

  /* 
     Does updates for the last n - 1 tables, returns 0 if ok;
     error takes into account killed status gained in do_updates()
  */
  int local_error = (table_count) ? do_updates() : 0;
  /*
    if local_error is not set ON until after do_updates() then
    later carried out killing should not affect binlogging.
  */
  killed_status= (local_error == 0)? THD::NOT_KILLED : thd->killed;
  thd_proc_info(thd, "end");

  /*
    Write the SQL statement to the binlog if we updated
    rows and we succeeded or if we updated some non
    transactional tables.
    
    The query has to binlog because there's a modified non-transactional table
    either from the query's list or via a stored routine: bug#13270,23333
  */

  assert(trans_safe || !updated || 
              thd->transaction.stmt.modified_non_trans_table);
  if (local_error == 0 || thd->transaction.stmt.modified_non_trans_table)
  {
    if (mysql_bin_log.is_open())
    {
      if (local_error == 0)
        thd->clear_error();
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query, thd->query_length,
                            transactional_tables, false, killed_status) &&
          trans_safe)
      {
        local_error= 1;				// Rollback update
      }
    }
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= true;
  }
  if (local_error != 0)
    error_handled= true; // to force early leave from ::send_error()

  if (local_error > 0) // if the above log write did not fail ...
  {
    /* Safety: If we haven't got an error before (can happen in do_updates) */
    my_message(ER_UNKNOWN_ERROR, "An error occured in multi-table update",
	       MYF(0));
    return(true);
  }

  id= thd->arg_of_last_insert_id_function ?
    thd->first_successful_insert_id_in_prev_stmt : 0;
  sprintf(buff, ER(ER_UPDATE_INFO), (ulong) found, (ulong) updated,
	  (ulong) thd->cuted_fields);
  thd->row_count_func=
    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated;
  ::my_ok(thd, (ulong) thd->row_count_func, id, buff);
  return(false);
}
