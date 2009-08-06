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
#include <drizzled/error.h>
#include <drizzled/probes.h>
#include <drizzled/sql_base.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/sql_parse.h>

#include <list>

using namespace std;


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
  uint32_t keynr;
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
  bitmap_init(&unique_map, unique_map_buf, table->s->fields);
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
    session			thread handler
    fields		fields for update
    values		values of fields for update
    conds		WHERE clause expression
    order_num		number of elemen in order_st BY clause
    order		order_st BY clause list
    limit		limit clause
    handle_duplicates	how to handle duplicates

  RETURN
    0  - OK
    1  - error
*/

int mysql_update(Session *session, TableList *table_list,
                 List<Item> &fields, List<Item> &values, COND *conds,
                 uint32_t order_num, order_st *order,
                 ha_rows limit, enum enum_duplicates,
                 bool ignore)
{
  bool		using_limit= limit != HA_POS_ERROR;
  bool		safe_update= test(session->options & OPTION_SAFE_UPDATES);
  bool		used_key_is_modified, transactional_table, will_batch;
  bool		can_compare_record;
  int		error, loc_error;
  uint		used_index= MAX_KEY, dup_key_found;
  bool          need_sort= true;
  ha_rows	updated, found;
  key_map	old_covering_keys;
  Table		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  Select_Lex    *select_lex= &session->lex->select_lex;
  uint64_t     id;
  List<Item> all_fields;
  Session::killed_state killed_status= Session::NOT_KILLED;

  DRIZZLE_UPDATE_START();
  if (session->openTablesLock(table_list))
    return 1;

  session->set_proc_info("init");
  table= table_list->table;

  /* Calculate "table->covering_keys" based on the WHERE */
  table->covering_keys= table->s->keys_in_use;
  table->quick_keys.reset();

  if (mysql_prepare_update(session, table_list, &conds, order_num, order))
    goto abort;

  old_covering_keys= table->covering_keys;		// Keys used in WHERE
  /* Check the fields we are going to modify */
  if (setup_fields_with_no_wrap(session, 0, fields, MARK_COLUMNS_WRITE, 0, 0))
    goto abort;                               /* purecov: inspected */
  if (table->timestamp_field)
  {
    // Don't set timestamp column if this is modified
    if (table->timestamp_field->isWriteSet())
      table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    else
    {
      if (table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_UPDATE ||
          table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH)
        table->setWriteSet(table->timestamp_field->field_index);
    }
  }

  if (setup_fields(session, 0, values, MARK_COLUMNS_READ, 0, 0))
  {
    free_underlaid_joins(session, select_lex);
    goto abort;                               /* purecov: inspected */
  }

  if (select_lex->inner_refs_list.elements &&
    fix_inner_refs(session, all_fields, select_lex, select_lex->ref_pointer_array))
  {
    DRIZZLE_UPDATE_END();
    return(-1);
  }

  if (conds)
  {
    Item::cond_result cond_value;
    conds= remove_eq_conds(session, conds, &cond_value);
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
  table->covering_keys.reset();

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  select= make_select(table, 0, 0, conds, 0, &error);
  if (error || !limit ||
      (select && select->check_quick(session, safe_update, limit)))
  {
    delete select;
    free_underlaid_joins(session, select_lex);
    if (error)
      goto abort;				// Error in where
    DRIZZLE_UPDATE_END();
    session->my_ok();				// No matching records
    return(0);
  }
  if (!select && limit != HA_POS_ERROR)
  {
    if ((used_index= get_index_for_order(table, order, limit)) != MAX_KEY)
      need_sort= false;
  }
  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.none())
  {
    session->server_status|=SERVER_QUERY_NO_INDEX_USED;
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
    if (used_index < MAX_KEY && old_covering_keys.test(used_index))
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
      uint32_t         length= 0;
      SORT_FIELD  *sortorder;
      ha_rows examined_rows;

      table->sort.io_cache = new IO_CACHE;
      memset(table->sort.io_cache, 0, sizeof(IO_CACHE));

      if (!(sortorder=make_unireg_sortorder(order, &length, NULL)) ||
          (table->sort.found_records= filesort(session, table, sortorder, length,
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
      if (open_cached_file(&tempfile, drizzle_tmpdir,TEMP_PREFIX,
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
        init_read_record(&info,session,table,select,0,1);
      else
        init_read_record_idx(&info, session, table, 1, used_index);

      session->set_proc_info("Searching rows for update");
      ha_rows tmp_limit= limit;

      while (!(error=info.read_record(&info)) && !session->killed)
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
      if (session->killed && !error)
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
  init_read_record(&info,session,table,select,0,1);

  updated= found= 0;
  /*
   * Per the SQL standard, inserting NULL into a NOT NULL
   * field requires an error to be thrown.
   *
   * @NOTE
   *
   * NULL check and handling occurs in field_conv.cc
   */
  session->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  session->cuted_fields= 0L;
  session->set_proc_info("Updating");

  transactional_table= table->file->has_transactions();
  session->abort_on_warning= test(!ignore);
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

  while (!(error=info.read_record(&info)) && !session->killed)
  {
    if (!(select && select->skip_record()))
    {
      if (table->file->was_semi_consistent_read())
        continue;  /* repeat the read of the same row if it still exists */

      table->storeRecord();
      if (fill_record(session, fields, values, 0))
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
    session->row_count++;
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
  killed_status= session->killed; // get the status of the volatile
  // simulated killing after the loop must be ineffective for binlogging
  error= (killed_status == Session::NOT_KILLED)?  error : 1;

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
    session->transaction.stmt.modified_non_trans_table= true;

  end_read_record(&info);
  delete select;
  session->set_proc_info("end");
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
  if ((error < 0) || session->transaction.stmt.modified_non_trans_table)
  {
    if (session->transaction.stmt.modified_non_trans_table)
      session->transaction.all.modified_non_trans_table= true;
  }
  assert(transactional_table || !updated || session->transaction.stmt.modified_non_trans_table);
  free_underlaid_joins(session, select_lex);

  /* If LAST_INSERT_ID(X) was used, report X */
  id= session->arg_of_last_insert_id_function ?
    session->first_successful_insert_id_in_prev_stmt : 0;

  DRIZZLE_UPDATE_END();
  if (error < 0)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    sprintf(buff, ER(ER_UPDATE_INFO), (ulong) found, (ulong) updated,
	    (ulong) session->cuted_fields);
    session->row_count_func= updated;
    session->my_ok((ulong) session->row_count_func, found, id, buff);
  }
  session->count_cuted_fields= CHECK_FIELD_IGNORE;		/* calc cuted fields */
  session->abort_on_warning= 0;
  return((error >= 0 || session->is_error()) ? 1 : 0);

err:
  delete select;
  free_underlaid_joins(session, select_lex);
  if (table->key_read)
  {
    table->key_read=0;
    table->file->extra(HA_EXTRA_NO_KEYREAD);
  }
  session->abort_on_warning= 0;

abort:
  DRIZZLE_UPDATE_END();
  return(1);
}

/*
  Prepare items in UPDATE statement

  SYNOPSIS
    mysql_prepare_update()
    session			- thread handler
    table_list		- global/local table list
    conds		- conditions
    order_num		- number of order_st BY list entries
    order		- order_st BY clause list

  RETURN VALUE
    false OK
    true  error
*/
bool mysql_prepare_update(Session *session, TableList *table_list,
			 Item **conds, uint32_t order_num, order_st *order)
{
  List<Item> all_fields;
  Select_Lex *select_lex= &session->lex->select_lex;

  session->lex->allow_sum_func= 0;

  if (setup_tables_and_check_access(session, &select_lex->context,
                                    &select_lex->top_join_list,
                                    table_list,
                                    &select_lex->leaf_tables,
                                    false) ||
      session->setup_conds(table_list, conds) ||
      select_lex->setup_ref_array(session, order_num) ||
      setup_order(session, select_lex->ref_pointer_array,
		  table_list, all_fields, all_fields, order))
    return true;

  /* Check that we are not using table that we are updating in a sub select */
  {
    TableList *duplicate;
    if ((duplicate= unique_table(session, table_list, table_list->next_global, 0)))
    {
      my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->table_name);
      return true;
    }
  }

  return false;
}
