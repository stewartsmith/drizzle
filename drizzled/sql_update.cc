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


/*
  Single table and multi table updates of tables.
*/

#include <config.h>

#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/probes.h>
#include <drizzled/sql_base.h>
#include <drizzled/field/epoch.h>
#include <drizzled/sql_parse.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/records.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/transaction_services.h>
#include <drizzled/filesort.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/key.h>
#include <drizzled/sql_lex.h>
#include <drizzled/diagnostics_area.h>
#include <drizzled/util/test.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session/transactions.h>

#include <boost/dynamic_bitset.hpp>
#include <list>

using namespace std;

namespace drizzled {

/**
  Re-read record if more columns are needed for error message.

  If we got a duplicate key error, we want to write an error
  message containing the value of the duplicate key. If we do not have
  all fields of the key value in getInsertRecord(), we need to re-read the
  record with a proper read_set.

  @param[in] error   error number
  @param[in] table   table
*/

static void prepare_record_for_error_message(int error, Table *table)
{
  Field **field_p= NULL;
  Field *field= NULL;
  uint32_t keynr= 0;

  /*
    Only duplicate key errors print the key value.
    If storage engine does always read all columns, we have the value alraedy.
  */
  if ((error != HA_ERR_FOUND_DUPP_KEY) ||
      ! (table->cursor->getEngine()->check_flag(HTON_BIT_PARTIAL_COLUMN_READ)))
    return;

  /*
    Get the number of the offended index.
    We will see MAX_KEY if the engine cannot determine the affected index.
  */
  if ((keynr= table->get_dup_key(error)) >= MAX_KEY)
    return;

  /* Create unique_map with all fields used by that index. */
  boost::dynamic_bitset<> unique_map(table->getShare()->sizeFields()); /* Fields in offended unique. */
  table->mark_columns_used_by_index_no_reset(keynr, unique_map);

  /* Subtract read_set and write_set. */
  unique_map-= *table->read_set;
  unique_map-= *table->write_set;

  /*
    If the unique index uses columns that are neither in read_set
    nor in write_set, we must re-read the record.
    Otherwise no need to do anything.
  */
  if (unique_map.none())
    return;

  /* Get identifier of last read record into table->cursor->ref. */
  table->cursor->position(table->getInsertRecord());
  /* Add all fields used by unique index to read_set. */
  *table->read_set|= unique_map;
  /* Read record that is identified by table->cursor->ref. */
  (void) table->cursor->rnd_pos(table->getUpdateRecord(), table->cursor->ref);
  /* Copy the newly read columns into the new record. */
  for (field_p= table->getFields(); (field= *field_p); field_p++)
  {
    if (unique_map.test(field->position()))
    {
      field->copy_from_tmp(table->getShare()->rec_buff_length);
    }
  }

  return;
}


/*
  Process usual UPDATE

  SYNOPSIS
    update_query()
    session			thread handler
    fields		fields for update
    values		values of fields for update
    conds		WHERE clause expression
    order_num		number of elemen in ORDER BY clause
    order		order_st BY clause list
    limit		limit clause
    handle_duplicates	how to handle duplicates

  RETURN
    0  - OK
    1  - error
*/

int update_query(Session *session, TableList *table_list,
                 List<Item> &fields, List<Item> &values, COND *conds,
                 uint32_t order_num, Order *order,
                 ha_rows limit, enum enum_duplicates,
                 bool ignore)
{
  bool		using_limit= limit != HA_POS_ERROR;
  bool		used_key_is_modified;
  bool		transactional_table;
  int		error= 0;
  uint		used_index= MAX_KEY, dup_key_found;
  bool          need_sort= true;
  ha_rows	updated, found;
  key_map	old_covering_keys;
  Table		*table;
  optimizer::SqlSelect *select= NULL;
  ReadRecord	info;
  Select_Lex    *select_lex= &session->lex().select_lex;
  uint64_t     id;
  List<Item> all_fields;
  Session::killed_state_t killed_status= Session::NOT_KILLED;

  DRIZZLE_UPDATE_START(session->getQueryString()->c_str());
  if (session->openTablesLock(table_list))
  {
    DRIZZLE_UPDATE_DONE(1, 0, 0);
    return 1;
  }

  session->set_proc_info("init");
  table= table_list->table;

  /* Calculate "table->covering_keys" based on the WHERE */
  table->covering_keys= table->getShare()->keys_in_use;
  table->quick_keys.reset();

  if (prepare_update(session, table_list, &conds, order_num, order))
  {
    DRIZZLE_UPDATE_DONE(1, 0, 0);
    return 1;
  }

  old_covering_keys= table->covering_keys;		// Keys used in WHERE
  /* Check the fields we are going to modify */
  if (setup_fields_with_no_wrap(session, 0, fields, MARK_COLUMNS_WRITE, 0, 0))
  {
    DRIZZLE_UPDATE_DONE(1, 0, 0);
    return 1;
  }

  if (table->timestamp_field)
  {
    // Don't set timestamp column if this is modified
    if (table->timestamp_field->isWriteSet())
    {
      table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    }
    else
    {
      if (table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_UPDATE ||
          table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH)
      {
        table->setWriteSet(table->timestamp_field->position());
      }
    }
  }

  if (setup_fields(session, 0, values, MARK_COLUMNS_READ, 0, 0))
  {
    free_underlaid_joins(session, select_lex);
    DRIZZLE_UPDATE_DONE(1, 0, 0);

    return 1;
  }

  if (select_lex->inner_refs_list.size() &&
    fix_inner_refs(session, all_fields, select_lex, select_lex->ref_pointer_array))
  {
    DRIZZLE_UPDATE_DONE(1, 0, 0);
    return 1;
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
  if (table->cursor->getEngine()->check_flag(HTON_BIT_PARTIAL_COLUMN_READ) &&
      table->timestamp_field &&
      (table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_UPDATE ||
       table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH))
  {
    *table->read_set|= *table->write_set;
  }
  // Don't count on usage of 'only index' when calculating which key to use
  table->covering_keys.reset();

  /* Update the table->cursor->stats.records number */
  table->cursor->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  select= optimizer::make_select(table, 0, 0, conds, 0, &error);
  if (error || !limit ||
      (select && select->check_quick(session, false, limit)))
  {
    delete select;
    /**
     * Resetting the Diagnostic area to prevent
     * lp bug# 439719
     */
    session->main_da().reset_diagnostics_area();
    free_underlaid_joins(session, select_lex);
    if (error || session->is_error())
    {
      DRIZZLE_UPDATE_DONE(1, 0, 0);
      return 1;
    }
    DRIZZLE_UPDATE_DONE(0, 0, 0);
    session->my_ok();				// No matching records
    return 0;
  }
  if (!select && limit != HA_POS_ERROR)
  {
    if ((used_index= optimizer::get_index_for_order(table, order, limit)) != MAX_KEY)
      need_sort= false;
  }
  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.none())
  {
    session->server_status|=SERVER_QUERY_NO_INDEX_USED;
  }

  table->mark_columns_needed_for_update();

  /* Check if we are modifying a key that we are used to search with */

  if (select && select->quick)
  {
    used_index= select->quick->index;
    used_key_is_modified= (!select->quick->unique_key_range() &&
                          select->quick->is_keys_used(*table->write_set));
  }
  else
  {
    used_key_is_modified= 0;
    if (used_index == MAX_KEY)                  // no index for sort order
      used_index= table->cursor->key_used_on_scan;
    if (used_index != MAX_KEY)
      used_key_is_modified= is_key_used(table, used_index, *table->write_set);
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
      SortField  *sortorder;
      ha_rows examined_rows;
      FileSort filesort(*session);

      table->sort.io_cache= new internal::io_cache_st;
      sortorder=make_unireg_sortorder(order, &length, NULL);

      if ((table->sort.found_records= filesort.run(table, sortorder, length, select, limit, 1, examined_rows)) == HA_POS_ERROR)
        goto err;
      /*
	Filesort has already found and selected the rows we want to update,
	so we don't need the where clause
      */
      safe_delete(select);
    }
    else
    {
      /*
	We are doing a search on a key that is updated. In this case
	we go trough the matching rows, save a pointer to them and
	update these in a separate loop based on the pointer.
      */

      internal::io_cache_st tempfile;
      if (tempfile.open_cached_file(drizzle_tmpdir.c_str(),TEMP_PREFIX, DISK_BUFFER_SIZE, MYF(MY_WME)))
      {
	goto err;
      }

      /* If quick select is used, initialize it before retrieving rows. */
      if (select && select->quick && select->quick->reset())
        goto err;
      table->cursor->try_semi_consistent_read(1);

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
      {
        if ((error= info.init_read_record(session, table, select, 0, true)))
          goto err;
      }
      else
      {
        if ((error= info.init_read_record_idx(session, table, 1, used_index)))
          goto err;
      }

      session->set_proc_info("Searching rows for update");
      ha_rows tmp_limit= limit;

      while (not(error= info.read_record(&info)) && not session->getKilled())
      {
	if (!(select && select->skip_record()))
	{
          if (table->cursor->was_semi_consistent_read())
	    continue;  /* repeat the read of the same row if it still exists */

	  table->cursor->position(table->getInsertRecord());
	  if (my_b_write(&tempfile,table->cursor->ref,
			 table->cursor->ref_length))
	  {
	    error=1;
	    break;
	  }
	  if (!--limit && using_limit)
	  {
	    error= -1;
	    break;
	  }
	}
	else
	  table->cursor->unlock_row();
      }
      if (session->getKilled() && not error)
	error= 1;				// Aborted
      limit= tmp_limit;
      table->cursor->try_semi_consistent_read(0);
      info.end_read_record();

      /* Change select to use tempfile */
      if (select)
      {
	safe_delete(select->quick);
	if (select->free_cond)
	  delete select->cond;
	select->cond=0;
      }
      else
      {
	select= new optimizer::SqlSelect();
	select->head=table;
      }
      if (tempfile.reinit_io_cache(internal::READ_CACHE,0L,0,0))
	error=1;
      // Read row ptrs from this cursor
      memcpy(select->file, &tempfile, sizeof(tempfile));
      if (error >= 0)
	goto err;
    }
    if (table->key_read)
      table->restore_column_maps_after_mark_index();
  }

  if (ignore)
    table->cursor->extra(HA_EXTRA_IGNORE_DUP_KEY);

  if (select && select->quick && select->quick->reset())
    goto err;
  table->cursor->try_semi_consistent_read(1);
  if ((error= info.init_read_record(session, table, select, 0, true)))
  {
    goto err;
  }

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

  transactional_table= table->cursor->has_transactions();
  session->setAbortOnWarning(test(!ignore));

  /*
    Assure that we can use position()
    if we need to create an error message.
  */
  if (table->cursor->getEngine()->check_flag(HTON_BIT_PARTIAL_COLUMN_READ))
    table->prepare_for_position();

  while (not (error=info.read_record(&info)) && not session->getKilled())
  {
    if (not (select && select->skip_record()))
    {
      if (table->cursor->was_semi_consistent_read())
        continue;  /* repeat the read of the same row if it still exists */

      table->storeRecord();
      if (fill_record(session, fields, values))
        break;

      found++;

      if (! table->records_are_comparable() || table->compare_records())
      {
        /* Non-batched update */
        error= table->cursor->updateRecord(table->getUpdateRecord(),
                                            table->getInsertRecord());

        table->auto_increment_field_not_null= false;

        if (!error || error == HA_ERR_RECORD_IS_THE_SAME)
        {
          if (error != HA_ERR_RECORD_IS_THE_SAME)
            updated++;
          else
            error= 0;
        }
        else if (! ignore ||
                 table->cursor->is_fatal_error(error, HA_CHECK_DUP_KEY))
        {
          /*
            If (ignore && error is ignorable) we don't have to
            do anything; otherwise...
          */
          myf flags= 0;

          if (table->cursor->is_fatal_error(error, HA_CHECK_DUP_KEY))
            flags|= ME_FATALERROR; /* Other handler errors are fatal */

          prepare_record_for_error_message(error, table);
          table->print_error(error,MYF(flags));
          error= 1;
          break;
        }
      }

      if (!--limit && using_limit)
      {
        error= -1;				// Simulate end of cursor
        break;
      }
    }
    else
      table->cursor->unlock_row();
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
  killed_status= session->getKilled(); // get the status of the volatile
  // simulated killing after the loop must be ineffective for binlogging
  error= (killed_status == Session::NOT_KILLED)?  error : 1;

  updated-= dup_key_found;
  table->cursor->try_semi_consistent_read(0);

  if (!transactional_table && updated > 0)
    session->transaction.stmt.markModifiedNonTransData();

  info.end_read_record();
  delete select;
  session->set_proc_info("end");
  table->cursor->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  /*
    error < 0 means really no error at all: we processed all rows until the
    last one without error. error > 0 means an error (e.g. unique key
    violation and no IGNORE or REPLACE). error == 0 is also an error (if
    preparing the record or invoking before triggers fails). See
    autocommitOrRollback(error>=0) and return(error>=0) below.
    Sometimes we want to binlog even if we updated no rows, in case user used
    it to be sure master and slave are in same state.
  */
  if ((error < 0) || session->transaction.stmt.hasModifiedNonTransData())
  {
    if (session->transaction.stmt.hasModifiedNonTransData())
      session->transaction.all.markModifiedNonTransData();
  }
  assert(transactional_table || !updated || session->transaction.stmt.hasModifiedNonTransData());
  free_underlaid_joins(session, select_lex);

  /* If LAST_INSERT_ID(X) was used, report X */
  id= session->arg_of_last_insert_id_function ?
    session->first_successful_insert_id_in_prev_stmt : 0;

  if (error < 0)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO), (ulong) found, (ulong) updated,
	    (ulong) session->cuted_fields);
    session->row_count_func= updated;
    /**
     * Resetting the Diagnostic area to prevent
     * lp bug# 439719
     */
    session->main_da().reset_diagnostics_area();
    session->my_ok((ulong) session->rowCount(), found, id, buff);
    session->status_var.updated_row_count+= session->rowCount();
  }
  session->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;		/* calc cuted fields */
  session->setAbortOnWarning(false);
  DRIZZLE_UPDATE_DONE((error >= 0 || session->is_error()), found, updated);
  return ((error >= 0 || session->is_error()) ? 1 : 0);

err:
  if (error != 0)
    table->print_error(error,MYF(0));

  delete select;
  free_underlaid_joins(session, select_lex);
  if (table->key_read)
  {
    table->key_read=0;
    table->cursor->extra(HA_EXTRA_NO_KEYREAD);
  }
  session->setAbortOnWarning(false);

  DRIZZLE_UPDATE_DONE(1, 0, 0);
  return 1;
}

/*
  Prepare items in UPDATE statement

  SYNOPSIS
    prepare_update()
    session			- thread handler
    table_list		- global/local table list
    conds		- conditions
    order_num		- number of ORDER BY list entries
    order		- ORDER BY clause list

  RETURN VALUE
    false OK
    true  error
*/
bool prepare_update(Session *session, TableList *table_list,
			 Item **conds, uint32_t order_num, Order *order)
{
  List<Item> all_fields;
  Select_Lex *select_lex= &session->lex().select_lex;

  session->lex().allow_sum_func= 0;

  if (setup_tables_and_check_access(session, &select_lex->context, &select_lex->top_join_list, table_list, &select_lex->leaf_tables, false) ||
      session->setup_conds(table_list, conds))
      return true;
  select_lex->setup_ref_array(session, order_num);
  if (setup_order(session, select_lex->ref_pointer_array, table_list, all_fields, all_fields, order))
    return true;

  /* Check that we are not using table that we are updating in a sub select */
  {
    TableList *duplicate;
    if ((duplicate= unique_table(table_list, table_list->next_global)))
    {
      my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->getTableName());
      return true;
    }
  }

  return false;
}

} /* namespace drizzled */
