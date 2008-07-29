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


/* Insert of records */

/*
  INSERT DELAYED

  Drizzle has a different form of DELAYED then MySQL. DELAYED is just
  a hint to the the sorage engine (which can then do whatever it likes.
*/

#include "mysql_priv.h"
#include "sql_select.h"
#include "sql_show.h"
#include "slave.h"
#include "rpl_mi.h"

/* Define to force use of my_malloc() if the allocated memory block is big */

#ifndef HAVE_ALLOCA
#define my_safe_alloca(size, min_length) my_alloca(size)
#define my_safe_afree(ptr, size, min_length) my_afree(ptr)
#else
#define my_safe_alloca(size, min_length) ((size <= min_length) ? my_alloca(size) : my_malloc(size,MYF(0)))
#define my_safe_afree(ptr, size, min_length) if (size > min_length) my_free(ptr,MYF(0))
#endif



/*
  Check if insert fields are correct.

  SYNOPSIS
    check_insert_fields()
    thd                         The current thread.
    table                       The table for insert.
    fields                      The insert fields.
    values                      The insert values.
    check_unique                If duplicate values should be rejected.

  NOTE
    Clears TIMESTAMP_AUTO_SET_ON_INSERT from table->timestamp_field_type
    or leaves it as is, depending on if timestamp should be updated or
    not.

  RETURN
    0           OK
    -1          Error
*/

static int check_insert_fields(THD *thd, TABLE_LIST *table_list,
                               List<Item> &fields, List<Item> &values,
                               bool check_unique,
                               table_map *map __attribute__((unused)))
{
  TABLE *table= table_list->table;

  if (fields.elements == 0 && values.elements != 0)
  {
    if (values.elements != table->s->fields)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return -1;
    }
    clear_timestamp_auto_bits(table->timestamp_field_type,
                              TIMESTAMP_AUTO_SET_ON_INSERT);
    /*
      No fields are provided so all fields must be provided in the values.
      Thus we set all bits in the write set.
    */
    bitmap_set_all(table->write_set);
  }
  else
  {						// Part field list
    SELECT_LEX *select_lex= &thd->lex->select_lex;
    Name_resolution_context *context= &select_lex->context;
    Name_resolution_context_state ctx_state;
    int res;

    if (fields.elements != values.elements)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return -1;
    }

    thd->dup_field= 0;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
    */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);
    res= setup_fields(thd, 0, fields, MARK_COLUMNS_WRITE, 0, 0);

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);

    if (res)
      return -1;

    if (check_unique && thd->dup_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), thd->dup_field->field_name);
      return -1;
    }
    if (table->timestamp_field)	// Don't automaticly set timestamp if used
    {
      if (bitmap_is_set(table->write_set,
                        table->timestamp_field->field_index))
        clear_timestamp_auto_bits(table->timestamp_field_type,
                                  TIMESTAMP_AUTO_SET_ON_INSERT);
      else
      {
        bitmap_set_bit(table->write_set,
                       table->timestamp_field->field_index);
      }
    }
  }

  return 0;
}


/*
  Check update fields for the timestamp field.

  SYNOPSIS
    check_update_fields()
    thd                         The current thread.
    insert_table_list           The insert table list.
    table                       The table for update.
    update_fields               The update fields.

  NOTE
    If the update fields include the timestamp field,
    remove TIMESTAMP_AUTO_SET_ON_UPDATE from table->timestamp_field_type.

  RETURN
    0           OK
    -1          Error
*/

static int check_update_fields(THD *thd, TABLE_LIST *insert_table_list,
                               List<Item> &update_fields,
                               table_map *map __attribute__((unused)))
{
  TABLE *table= insert_table_list->table;
  bool timestamp_mark= false;

  if (table->timestamp_field)
  {
    /*
      Unmark the timestamp field so that we can check if this is modified
      by update_fields
    */
    timestamp_mark= bitmap_test_and_clear(table->write_set,
                                          table->timestamp_field->field_index);
  }

  /* Check the fields we are going to modify */
  if (setup_fields(thd, 0, update_fields, MARK_COLUMNS_WRITE, 0, 0))
    return -1;

  if (table->timestamp_field)
  {
    /* Don't set timestamp column if this is modified. */
    if (bitmap_is_set(table->write_set,
                      table->timestamp_field->field_index))
      clear_timestamp_auto_bits(table->timestamp_field_type,
                                TIMESTAMP_AUTO_SET_ON_UPDATE);
    if (timestamp_mark)
      bitmap_set_bit(table->write_set,
                     table->timestamp_field->field_index);
  }
  return 0;
}


/**
  Upgrade table-level lock of INSERT statement to TL_WRITE if
  a more concurrent lock is infeasible for some reason. This is
  necessary for engines without internal locking support (MyISAM).
  An engine with internal locking implementation might later
  downgrade the lock in handler::store_lock() method.
*/

static
void upgrade_lock_type(THD *thd __attribute__((unused)),
                       thr_lock_type *lock_type,
                       enum_duplicates duplic,
                       bool is_multi_insert __attribute__((unused)))
{
  if (duplic == DUP_UPDATE ||
      (duplic == DUP_REPLACE && *lock_type == TL_WRITE_CONCURRENT_INSERT))
  {
    *lock_type= TL_WRITE_DEFAULT;
    return;
  }
}


/**
  INSERT statement implementation

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool mysql_insert(THD *thd,TABLE_LIST *table_list,
                  List<Item> &fields,
                  List<List_item> &values_list,
                  List<Item> &update_fields,
                  List<Item> &update_values,
                  enum_duplicates duplic,
		  bool ignore)
{
  int error;
  bool transactional_table, joins_freed= false;
  bool changed;
  bool was_insert_delayed= (table_list->lock_type ==  TL_WRITE_DELAYED);
  uint value_count;
  ulong counter = 1;
  uint64_t id;
  COPY_INFO info;
  TABLE *table= 0;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  Name_resolution_context *context;
  Name_resolution_context_state ctx_state;
  thr_lock_type lock_type;
  Item *unused_conds= 0;
  

  /*
    Upgrade lock type if the requested lock is incompatible with
    the current connection mode or table operation.
  */
  upgrade_lock_type(thd, &table_list->lock_type, duplic,
                    values_list.elements > 1);

  /*
    We can't write-delayed into a table locked with LOCK TABLES:
    this will lead to a deadlock, since the delayed thread will
    never be able to get a lock on the table. QQQ: why not
    upgrade the lock here instead?
  */
  if (table_list->lock_type == TL_WRITE_DELAYED && thd->locked_tables &&
      find_locked_table(thd, table_list->db, table_list->table_name))
  {
    my_error(ER_DELAYED_INSERT_TABLE_LOCKED, MYF(0),
             table_list->table_name);
    return(true);
  }

  {
    if (open_and_lock_tables(thd, table_list))
      return(true);
  }
  lock_type= table_list->lock_type;

  thd_proc_info(thd, "init");
  thd->used_tables=0;
  values= its++;
  value_count= values->elements;

  if (mysql_prepare_insert(thd, table_list, table, fields, values,
			   update_fields, update_values, duplic, &unused_conds,
                           false,
                           (fields.elements || !value_count ||
                            (0) != 0), !ignore))
    goto abort;

  /* mysql_prepare_insert set table_list->table if it was not set */
  table= table_list->table;

  context= &thd->lex->select_lex.context;
  /*
    These three asserts test the hypothesis that the resetting of the name
    resolution context below is not necessary at all since the list of local
    tables for INSERT always consists of one table.
  */
  assert(!table_list->next_local);
  assert(!context->table_list->next_local);
  assert(!context->first_name_resolution_table->next_name_resolution_table);

  /* Save the state of the current name resolution context. */
  ctx_state.save_state(context, table_list);

  /*
    Perform name resolution only in the first table - 'table_list',
    which is the table that is inserted into.
  */
  table_list->next_local= 0;
  context->resolve_in_table_list_only(table_list);

  while ((values= its++))
  {
    counter++;
    if (values->elements != value_count)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), counter);
      goto abort;
    }
    if (setup_fields(thd, 0, *values, MARK_COLUMNS_READ, 0, 0))
      goto abort;
  }
  its.rewind ();
 
  /* Restore the current context. */
  ctx_state.restore_state(context, table_list);

  /*
    Fill in the given fields and dump it to the table file
  */
  memset((char*) &info, 0, sizeof(info));
  info.ignore= ignore;
  info.handle_duplicates=duplic;
  info.update_fields= &update_fields;
  info.update_values= &update_values;

  /*
    Count warnings for all inserts.
    For single line insert, generate an error if try to set a NOT NULL field
    to NULL.
  */
  thd->count_cuted_fields= ((values_list.elements == 1 &&
                             !ignore) ?
			    CHECK_FIELD_ERROR_FOR_NULL :
			    CHECK_FIELD_WARN);
  thd->cuted_fields = 0L;
  table->next_number_field=table->found_next_number_field;

#ifdef HAVE_REPLICATION
  if (thd->slave_thread &&
      (info.handle_duplicates == DUP_UPDATE) &&
      (table->next_number_field != NULL) &&
      rpl_master_has_bug(&active_mi->rli, 24432))
    goto abort;
#endif

  error=0;
  thd_proc_info(thd, "update");
  if (duplic == DUP_REPLACE)
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplic == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  {
    if (duplic != DUP_ERROR || ignore)
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    table->file->ha_start_bulk_insert(values_list.elements);
  }


  thd->abort_on_warning= !ignore;

  table->mark_columns_needed_for_insert();

  while ((values= its++))
  {
    if (fields.elements || !value_count)
    {
      restore_record(table,s->default_values);	// Get empty record
      if (fill_record(thd, fields, *values, 0))
      {
	if (values_list.elements != 1 && ! thd->is_error())
	{
	  info.records++;
	  continue;
	}
	/*
	  TODO: set thd->abort_on_warning if values_list.elements == 1
	  and check that all items return warning in case of problem with
	  storing field.
        */
	error=1;
	break;
      }
    }
    else
    {
      if (thd->used_tables)			// Column used in values()
	restore_record(table,s->default_values);	// Get empty record
      else
      {
        /*
          Fix delete marker. No need to restore rest of record since it will
          be overwritten by fill_record() anyway (and fill_record() does not
          use default values in this case).
        */
	table->record[0][0]= table->s->default_values[0];
      }
      if (fill_record(thd, table->field, *values, 0))
      {
	if (values_list.elements != 1 && ! thd->is_error())
	{
	  info.records++;
	  continue;
	}
	error=1;
	break;
      }
    }

    error=write_record(thd, table ,&info);
    if (error)
      break;
    thd->row_count++;
  }

  free_underlaid_joins(thd, &thd->lex->select_lex);
  joins_freed= true;

  /*
    Now all rows are inserted.  Time to update logs and sends response to
    user
  */
  {
    /*
      Do not do this release if this is a delayed insert, it would steal
      auto_inc values from the delayed_insert thread as they share TABLE.
    */
    table->file->ha_release_auto_increment();
    if (table->file->ha_end_bulk_insert() && !error)
    {
      table->file->print_error(my_errno,MYF(0));
      error=1;
    }
    if (duplic != DUP_ERROR || ignore)
      table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

    transactional_table= table->file->has_transactions();

    if ((changed= (info.copied || info.deleted || info.updated)))
    {
      /*
        Invalidate the table in the query cache if something changed.
        For the transactional algorithm to work the invalidation must be
        before binlog writing and ha_autocommit_or_rollback
      */
    }
    if ((changed && error <= 0) || thd->transaction.stmt.modified_non_trans_table || was_insert_delayed)
    {
      if (mysql_bin_log.is_open())
      {
	if (error <= 0)
        {
	  /*
	    [Guilhem wrote] Temporary errors may have filled
	    thd->net.last_error/errno.  For example if there has
	    been a disk full error when writing the row, and it was
	    MyISAM, then thd->net.last_error/errno will be set to
	    "disk full"... and the my_pwrite() will wait until free
	    space appears, and so when it finishes then the
	    write_row() was entirely successful
	  */
	  /* todo: consider removing */
	  thd->clear_error();
	}
	/* bug#22725:

	A query which per-row-loop can not be interrupted with
	KILLED, like INSERT, and that does not invoke stored
	routines can be binlogged with neglecting the KILLED error.
        
	If there was no error (error == zero) until after the end of
	inserting loop the KILLED flag that appeared later can be
	disregarded since previously possible invocation of stored
	routines did not result in any error due to the KILLED.  In
	such case the flag is ignored for constructing binlog event.
	*/
	assert(thd->killed != THD::KILL_BAD_DATA || error > 0);
	if (thd->binlog_query(THD::ROW_QUERY_TYPE,
			      thd->query, thd->query_length,
			      transactional_table, false,
			      (error>0) ? thd->killed : THD::NOT_KILLED) &&
	    transactional_table)
        {
	  error=1;
	}
      }
      if (thd->transaction.stmt.modified_non_trans_table)
	thd->transaction.all.modified_non_trans_table= true;
    }
    assert(transactional_table || !changed || 
                thd->transaction.stmt.modified_non_trans_table);

  }
  thd_proc_info(thd, "end");
  /*
    We'll report to the client this id:
    - if the table contains an autoincrement column and we successfully
    inserted an autogenerated value, the autogenerated value.
    - if the table contains no autoincrement column and LAST_INSERT_ID(X) was
    called, X.
    - if the table contains an autoincrement column, and some rows were
    inserted, the id of the last "inserted" row (if IGNORE, that value may not
    have been really inserted but ignored).
  */
  id= (thd->first_successful_insert_id_in_cur_stmt > 0) ?
    thd->first_successful_insert_id_in_cur_stmt :
    (thd->arg_of_last_insert_id_function ?
     thd->first_successful_insert_id_in_prev_stmt :
     ((table->next_number_field && info.copied) ?
     table->next_number_field->val_int() : 0));
  table->next_number_field=0;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  table->auto_increment_field_not_null= false;
  if (duplic == DUP_REPLACE)
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  if (error)
    goto abort;
  if (values_list.elements == 1 && (!(thd->options & OPTION_WARNINGS) ||
				    !thd->cuted_fields))
  {
    thd->row_count_func= info.copied + info.deleted +
                         ((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                          info.touched : info.updated);
    my_ok(thd, (ulong) thd->row_count_func, id);
  }
  else
  {
    char buff[160];
    ha_rows updated=((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                     info.touched : info.updated);
    if (ignore)
      sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
              (ulong) (info.records - info.copied), (ulong) thd->cuted_fields);
    else
      sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	      (ulong) (info.deleted + updated), (ulong) thd->cuted_fields);
    thd->row_count_func= info.copied + info.deleted + updated;
    ::my_ok(thd, (ulong) thd->row_count_func, id, buff);
  }
  thd->abort_on_warning= 0;
  MYSQL_INSERT_END();
  return(false);

abort:
  if (table != NULL)
    table->file->ha_release_auto_increment();
  if (!joins_freed)
    free_underlaid_joins(thd, &thd->lex->select_lex);
  thd->abort_on_warning= 0;
  MYSQL_INSERT_END();
  return(true);
}


/*
  Check if table can be updated

  SYNOPSIS
     mysql_prepare_insert_check_table()
     thd		Thread handle
     table_list		Table list
     fields		List of fields to be updated
     where		Pointer to where clause
     select_insert      Check is making for SELECT ... INSERT

   RETURN
     false ok
     true  ERROR
*/

static bool mysql_prepare_insert_check_table(THD *thd, TABLE_LIST *table_list,
                                             List<Item> &fields __attribute__((unused)),
                                             bool select_insert)
{
  

  /*
     first table in list is the one we'll INSERT into, requires INSERT_ACL.
     all others require SELECT_ACL only. the ACL requirement below is for
     new leaves only anyway (view-constituents), so check for SELECT rather
     than INSERT.
  */

  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    table_list,
                                    &thd->lex->select_lex.leaf_tables,
                                    select_insert))
    return(true);

  return(false);
}


/*
  Prepare items in INSERT statement

  SYNOPSIS
    mysql_prepare_insert()
    thd			Thread handler
    table_list	        Global/local table list
    table		Table to insert into (can be NULL if table should
			be taken from table_list->table)    
    where		Where clause (for insert ... select)
    select_insert	true if INSERT ... SELECT statement
    check_fields        true if need to check that all INSERT fields are 
                        given values.
    abort_on_warning    whether to report if some INSERT field is not 
                        assigned as an error (true) or as a warning (false).

  TODO (in far future)
    In cases of:
    INSERT INTO t1 SELECT a, sum(a) as sum1 from t2 GROUP BY a
    ON DUPLICATE KEY ...
    we should be able to refer to sum1 in the ON DUPLICATE KEY part

  WARNING
    You MUST set table->insert_values to 0 after calling this function
    before releasing the table object.
  
  RETURN VALUE
    false OK
    true  error
*/

bool mysql_prepare_insert(THD *thd, TABLE_LIST *table_list,
                          TABLE *table, List<Item> &fields, List_item *values,
                          List<Item> &update_fields, List<Item> &update_values,
                          enum_duplicates duplic,
                          COND **where __attribute__((unused)),
                          bool select_insert,
                          bool check_fields, bool abort_on_warning)
{
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  Name_resolution_context *context= &select_lex->context;
  Name_resolution_context_state ctx_state;
  bool insert_into_view= (0 != 0);
  bool res= 0;
  table_map map= 0;
  
  /* INSERT should have a SELECT or VALUES clause */
  assert (!select_insert || !values);

  /*
    For subqueries in VALUES() we should not see the table in which we are
    inserting (for INSERT ... SELECT this is done by changing table_list,
    because INSERT ... SELECT share SELECT_LEX it with SELECT.
  */
  if (!select_insert)
  {
    for (SELECT_LEX_UNIT *un= select_lex->first_inner_unit();
         un;
         un= un->next_unit())
    {
      for (SELECT_LEX *sl= un->first_select();
           sl;
           sl= sl->next_select())
      {
        sl->context.outer_context= 0;
      }
    }
  }

  if (duplic == DUP_UPDATE)
  {
    /* it should be allocated before Item::fix_fields() */
    if (table_list->set_insert_values(thd->mem_root))
      return(true);
  }

  if (mysql_prepare_insert_check_table(thd, table_list, fields, select_insert))
    return(true);


  /* Prepare the fields in the statement. */
  if (values)
  {
    /* if we have INSERT ... VALUES () we cannot have a GROUP BY clause */
    assert (!select_lex->group_list.elements);

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
     */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);

    res= check_insert_fields(thd, context->table_list, fields, *values,
                             !insert_into_view, &map) ||
      setup_fields(thd, 0, *values, MARK_COLUMNS_READ, 0, 0);

    if (!res && check_fields)
    {
      bool saved_abort_on_warning= thd->abort_on_warning;
      thd->abort_on_warning= abort_on_warning;
      res= check_that_all_fields_are_given_values(thd, 
                                                  table ? table : 
                                                  context->table_list->table,
                                                  context->table_list);
      thd->abort_on_warning= saved_abort_on_warning;
    }

    if (!res && duplic == DUP_UPDATE)
    {
      res= check_update_fields(thd, context->table_list, update_fields, &map);
    }

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);

    if (!res)
      res= setup_fields(thd, 0, update_values, MARK_COLUMNS_READ, 0, 0);
  }

  if (res)
    return(res);

  if (!table)
    table= table_list->table;

  if (!select_insert)
  {
    TABLE_LIST *duplicate;
    if ((duplicate= unique_table(thd, table_list, table_list->next_global, 1)))
    {
      update_non_unique_table_error(table_list, "INSERT", duplicate);
      return(true);
    }
    select_lex->first_execution= 0;
  }
  if (duplic == DUP_UPDATE || duplic == DUP_REPLACE)
    table->prepare_for_position();
  return(false);
}


	/* Check if there is more uniq keys after field */

static int last_uniq_key(TABLE *table,uint keynr)
{
  while (++keynr < table->s->keys)
    if (table->key_info[keynr].flags & HA_NOSAME)
      return 0;
  return 1;
}


/*
  Write a record to table with optional deleting of conflicting records,
  invoke proper triggers if needed.

  SYNOPSIS
     write_record()
      thd   - thread context
      table - table to which record should be written
      info  - COPY_INFO structure describing handling of duplicates
              and which is used for counting number of records inserted
              and deleted.

  NOTE
    Once this record will be written to table after insert trigger will
    be invoked. If instead of inserting new record we will update old one
    then both on update triggers will work instead. Similarly both on
    delete triggers will be invoked if we will delete conflicting records.

    Sets thd->transaction.stmt.modified_non_trans_table to true if table which is updated didn't have
    transactions.

  RETURN VALUE
    0     - success
    non-0 - error
*/


int write_record(THD *thd, TABLE *table,COPY_INFO *info)
{
  int error;
  char *key=0;
  MY_BITMAP *save_read_set, *save_write_set;
  uint64_t prev_insert_id= table->file->next_insert_id;
  uint64_t insert_id_for_cur_row= 0;
  

  info->records++;
  save_read_set=  table->read_set;
  save_write_set= table->write_set;

  if (info->handle_duplicates == DUP_REPLACE ||
      info->handle_duplicates == DUP_UPDATE)
  {
    while ((error=table->file->ha_write_row(table->record[0])))
    {
      uint key_nr;
      /*
        If we do more than one iteration of this loop, from the second one the
        row will have an explicit value in the autoinc field, which was set at
        the first call of handler::update_auto_increment(). So we must save
        the autogenerated value to avoid thd->insert_id_for_cur_row to become
        0.
      */
      if (table->file->insert_id_for_cur_row > 0)
        insert_id_for_cur_row= table->file->insert_id_for_cur_row;
      else
        table->file->insert_id_for_cur_row= insert_id_for_cur_row;
      bool is_duplicate_key_error;
      if (table->file->is_fatal_error(error, HA_CHECK_DUP))
	goto err;
      is_duplicate_key_error= table->file->is_fatal_error(error, 0);
      if (!is_duplicate_key_error)
      {
        /*
          We come here when we had an ignorable error which is not a duplicate
          key error. In this we ignore error if ignore flag is set, otherwise
          report error as usual. We will not do any duplicate key processing.
        */
        if (info->ignore)
          goto gok_or_after_err; /* Ignoring a not fatal error, return 0 */
        goto err;
      }
      if ((int) (key_nr = table->file->get_dup_key(error)) < 0)
      {
	error= HA_ERR_FOUND_DUPP_KEY;         /* Database can't find key */
	goto err;
      }
      /* Read all columns for the row we are going to replace */
      table->use_all_columns();
      /*
	Don't allow REPLACE to replace a row when a auto_increment column
	was used.  This ensures that we don't get a problem when the
	whole range of the key has been used.
      */
      if (info->handle_duplicates == DUP_REPLACE &&
          table->next_number_field &&
          key_nr == table->s->next_number_index &&
	  (insert_id_for_cur_row > 0))
	goto err;
      if (table->file->ha_table_flags() & HA_DUPLICATE_POS)
      {
	if (table->file->rnd_pos(table->record[1],table->file->dup_ref))
	  goto err;
      }
      else
      {
	if (table->file->extra(HA_EXTRA_FLUSH_CACHE)) /* Not needed with NISAM */
	{
	  error=my_errno;
	  goto err;
	}

	if (!key)
	{
	  if (!(key=(char*) my_safe_alloca(table->s->max_unique_length,
					   MAX_KEY_LENGTH)))
	  {
	    error=ENOMEM;
	    goto err;
	  }
	}
	key_copy((uchar*) key,table->record[0],table->key_info+key_nr,0);
	if ((error=(table->file->index_read_idx_map(table->record[1],key_nr,
                                                    (uchar*) key, HA_WHOLE_KEY,
                                                    HA_READ_KEY_EXACT))))
	  goto err;
      }
      if (info->handle_duplicates == DUP_UPDATE)
      {
        /*
          We don't check for other UNIQUE keys - the first row
          that matches, is updated. If update causes a conflict again,
          an error is returned
        */
	assert(table->insert_values != NULL);
        store_record(table,insert_values);
        restore_record(table,record[1]);
        assert(info->update_fields->elements ==
                    info->update_values->elements);
        if (fill_record(thd, *info->update_fields,
                                                 *info->update_values,
                                                 info->ignore))
          goto before_err;

        table->file->restore_auto_increment(prev_insert_id);
        if (table->next_number_field)
          table->file->adjust_next_insert_id_after_explicit_value(
            table->next_number_field->val_int());
        info->touched++;
        if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ &&
             !bitmap_is_subset(table->write_set, table->read_set)) ||
            compare_record(table))
        {
          if ((error=table->file->ha_update_row(table->record[1],
                                                table->record[0])) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
          {
            if (info->ignore &&
                !table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
            {
              goto gok_or_after_err;
            }
            goto err;
          }

          if (error != HA_ERR_RECORD_IS_THE_SAME)
            info->updated++;
          else
            error= 0;
          /*
            If ON DUP KEY UPDATE updates a row instead of inserting one, it's
            like a regular UPDATE statement: it should not affect the value of a
            next SELECT LAST_INSERT_ID() or mysql_insert_id().
            Except if LAST_INSERT_ID(#) was in the INSERT query, which is
            handled separately by THD::arg_of_last_insert_id_function.
          */
          insert_id_for_cur_row= table->file->insert_id_for_cur_row= 0;
          info->copied++;
        }

        if (table->next_number_field)
          table->file->adjust_next_insert_id_after_explicit_value(
            table->next_number_field->val_int());
        info->touched++;

        goto gok_or_after_err;
      }
      else /* DUP_REPLACE */
      {
	/*
	  The manual defines the REPLACE semantics that it is either
	  an INSERT or DELETE(s) + INSERT; FOREIGN KEY checks in
	  InnoDB do not function in the defined way if we allow MySQL
	  to convert the latter operation internally to an UPDATE.
          We also should not perform this conversion if we have 
          timestamp field with ON UPDATE which is different from DEFAULT.
          Another case when conversion should not be performed is when
          we have ON DELETE trigger on table so user may notice that
          we cheat here. Note that it is ok to do such conversion for
          tables which have ON UPDATE but have no ON DELETE triggers,
          we just should not expose this fact to users by invoking
          ON UPDATE triggers.
	*/
	if (last_uniq_key(table,key_nr) &&
	    !table->file->referenced_by_foreign_key() &&
            (table->timestamp_field_type == TIMESTAMP_NO_AUTO_SET ||
             table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH))
        {
          if ((error=table->file->ha_update_row(table->record[1],
					        table->record[0])) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
            goto err;
          if (error != HA_ERR_RECORD_IS_THE_SAME)
            info->deleted++;
          else
            error= 0;
          thd->record_first_successful_insert_id_in_cur_stmt(table->file->insert_id_for_cur_row);
          /*
            Since we pretend that we have done insert we should call
            its after triggers.
          */
          goto after_n_copied_inc;
        }
        else
        {
          if ((error=table->file->ha_delete_row(table->record[1])))
            goto err;
          info->deleted++;
          if (!table->file->has_transactions())
            thd->transaction.stmt.modified_non_trans_table= true;
          /* Let us attempt do write_row() once more */
        }
      }
    }
    thd->record_first_successful_insert_id_in_cur_stmt(table->file->insert_id_for_cur_row);
    /*
      Restore column maps if they where replaced during an duplicate key
      problem.
    */
    if (table->read_set != save_read_set ||
        table->write_set != save_write_set)
      table->column_bitmaps_set(save_read_set, save_write_set);
  }
  else if ((error=table->file->ha_write_row(table->record[0])))
  {
    if (!info->ignore ||
        table->file->is_fatal_error(error, HA_CHECK_DUP))
      goto err;
    table->file->restore_auto_increment(prev_insert_id);
    goto gok_or_after_err;
  }

after_n_copied_inc:
  info->copied++;
  thd->record_first_successful_insert_id_in_cur_stmt(table->file->insert_id_for_cur_row);

gok_or_after_err:
  if (key)
    my_safe_afree(key,table->s->max_unique_length,MAX_KEY_LENGTH);
  if (!table->file->has_transactions())
    thd->transaction.stmt.modified_non_trans_table= true;
  return(0);

err:
  info->last_errno= error;
  /* current_select is NULL if this is a delayed insert */
  if (thd->lex->current_select)
    thd->lex->current_select->no_error= 0;        // Give error
  table->file->print_error(error,MYF(0));
  
before_err:
  table->file->restore_auto_increment(prev_insert_id);
  if (key)
    my_safe_afree(key, table->s->max_unique_length, MAX_KEY_LENGTH);
  table->column_bitmaps_set(save_read_set, save_write_set);
  return(1);
}


/******************************************************************************
  Check that all fields with arn't null_fields are used
******************************************************************************/

int check_that_all_fields_are_given_values(THD *thd, TABLE *entry,
                                           TABLE_LIST *table_list)
{
  int err= 0;
  MY_BITMAP *write_set= entry->write_set;

  for (Field **field=entry->field ; *field ; field++)
  {
    if (!bitmap_is_set(write_set, (*field)->field_index) &&
        ((*field)->flags & NO_DEFAULT_VALUE_FLAG) &&
        ((*field)->real_type() != DRIZZLE_TYPE_ENUM))
    {
      bool view= false;
      if (table_list)
      {
        table_list= table_list->top_table();
        view= test(0);
      }
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_NO_DEFAULT_FOR_FIELD,
                            ER(ER_NO_DEFAULT_FOR_FIELD),
                            (*field)->field_name);
      }
      err= 1;
    }
  }
  return thd->abort_on_warning ? err : 0;
}

/***************************************************************************
  Store records in INSERT ... SELECT *
***************************************************************************/


/*
  make insert specific preparation and checks after opening tables

  SYNOPSIS
    mysql_insert_select_prepare()
    thd         thread handler

  RETURN
    false OK
    true  Error
*/

bool mysql_insert_select_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *first_select_leaf_table;
  

  /*
    Statement-based replication of INSERT ... SELECT ... LIMIT is not safe
    as order of rows is not defined, so in mixed mode we go to row-based.

    Note that we may consider a statement as safe if ORDER BY primary_key
    is present or we SELECT a constant. However it may confuse users to
    see very similiar statements replicated differently.
  */
  if (lex->current_select->select_limit)
  {
    lex->set_stmt_unsafe();
    thd->set_current_stmt_binlog_row_based_if_mixed();
  }
  /*
    SELECT_LEX do not belong to INSERT statement, so we can't add WHERE
    clause if table is VIEW
  */
  
  if (mysql_prepare_insert(thd, lex->query_tables,
                           lex->query_tables->table, lex->field_list, 0,
                           lex->update_list, lex->value_list,
                           lex->duplicates,
                           &select_lex->where, true, false, false))
    return(true);

  /*
    exclude first table from leaf tables list, because it belong to
    INSERT
  */
  assert(select_lex->leaf_tables != 0);
  lex->leaf_tables_insert= select_lex->leaf_tables;
  /* skip all leaf tables belonged to view where we are insert */
  for (first_select_leaf_table= select_lex->leaf_tables->next_leaf;
       first_select_leaf_table &&
       first_select_leaf_table->belong_to_view &&
       first_select_leaf_table->belong_to_view ==
       lex->leaf_tables_insert->belong_to_view;
       first_select_leaf_table= first_select_leaf_table->next_leaf)
  {}
  select_lex->leaf_tables= first_select_leaf_table;
  return(false);
}


select_insert::select_insert(TABLE_LIST *table_list_par, TABLE *table_par,
                             List<Item> *fields_par,
                             List<Item> *update_fields,
                             List<Item> *update_values,
                             enum_duplicates duplic,
                             bool ignore_check_option_errors)
  :table_list(table_list_par), table(table_par), fields(fields_par),
   autoinc_value_of_last_inserted_row(0),
   insert_into_view(table_list_par && 0 != 0)
{
  memset((char*) &info, 0, sizeof(info));
  info.handle_duplicates= duplic;
  info.ignore= ignore_check_option_errors;
  info.update_fields= update_fields;
  info.update_values= update_values;
}


int
select_insert::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  LEX *lex= thd->lex;
  int res;
  table_map map= 0;
  SELECT_LEX *lex_current_select_save= lex->current_select;
  

  unit= u;

  /*
    Since table in which we are going to insert is added to the first
    select, LEX::current_select should point to the first select while
    we are fixing fields from insert list.
  */
  lex->current_select= &lex->select_lex;
  res= check_insert_fields(thd, table_list, *fields, values,
                           !insert_into_view, &map) ||
       setup_fields(thd, 0, values, MARK_COLUMNS_READ, 0, 0);

  if (!res && fields->elements)
  {
    bool saved_abort_on_warning= thd->abort_on_warning;
    thd->abort_on_warning= !info.ignore;
    res= check_that_all_fields_are_given_values(thd, table_list->table, 
                                                table_list);
    thd->abort_on_warning= saved_abort_on_warning;
  }

  if (info.handle_duplicates == DUP_UPDATE && !res)
  {
    Name_resolution_context *context= &lex->select_lex.context;
    Name_resolution_context_state ctx_state;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /* Perform name resolution only in the first table - 'table_list'. */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);

    res= res || check_update_fields(thd, context->table_list,
                                    *info.update_fields, &map);
    /*
      When we are not using GROUP BY and there are no ungrouped aggregate functions 
      we can refer to other tables in the ON DUPLICATE KEY part.
      We use next_name_resolution_table descructively, so check it first (views?)
    */
    assert (!table_list->next_name_resolution_table);
    if (lex->select_lex.group_list.elements == 0 &&
        !lex->select_lex.with_sum_func)
      /*
        We must make a single context out of the two separate name resolution contexts :
        the INSERT table and the tables in the SELECT part of INSERT ... SELECT.
        To do that we must concatenate the two lists
      */  
      table_list->next_name_resolution_table= 
        ctx_state.get_first_name_resolution_table();

    res= res || setup_fields(thd, 0, *info.update_values,
                             MARK_COLUMNS_READ, 0, 0);
    if (!res)
    {
      /*
        Traverse the update values list and substitute fields from the
        select for references (Item_ref objects) to them. This is done in
        order to get correct values from those fields when the select
        employs a temporary table.
      */
      List_iterator<Item> li(*info.update_values);
      Item *item;

      while ((item= li++))
      {
        item->transform(&Item::update_value_transformer,
                        (uchar*)lex->current_select);
      }
    }

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);
  }

  lex->current_select= lex_current_select_save;
  if (res)
    return(1);
  /*
    if it is INSERT into join view then check_insert_fields already found
    real table for insert
  */
  table= table_list->table;

  /*
    Is table which we are changing used somewhere in other parts of
    query
  */
  if (unique_table(thd, table_list, table_list->next_global, 0))
  {
    /* Using same table for INSERT and SELECT */
    lex->current_select->options|= OPTION_BUFFER_RESULT;
    lex->current_select->join->select_options|= OPTION_BUFFER_RESULT;
  }
  else if (!(lex->current_select->options & OPTION_BUFFER_RESULT))
  {
    /*
      We must not yet prepare the result table if it is the same as one of the 
      source tables (INSERT SELECT). The preparation may disable 
      indexes on the result table, which may be used during the select, if it
      is the same table (Bug #6034). Do the preparation after the select phase
      in select_insert::prepare2().
      We won't start bulk inserts at all if this statement uses functions or
      should invoke triggers since they may access to the same table too.
    */
    table->file->ha_start_bulk_insert((ha_rows) 0);
  }
  restore_record(table,s->default_values);		// Get empty record
  table->next_number_field=table->found_next_number_field;

#ifdef HAVE_REPLICATION
  if (thd->slave_thread &&
      (info.handle_duplicates == DUP_UPDATE) &&
      (table->next_number_field != NULL) &&
      rpl_master_has_bug(&active_mi->rli, 24432))
    return(1);
#endif

  thd->cuted_fields=0;
  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (info.handle_duplicates == DUP_REPLACE)
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (info.handle_duplicates == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  thd->abort_on_warning= !info.ignore;
  table->mark_columns_needed_for_insert();


  return(res);
}


/*
  Finish the preparation of the result table.

  SYNOPSIS
    select_insert::prepare2()
    void

  DESCRIPTION
    If the result table is the same as one of the source tables (INSERT SELECT),
    the result table is not finally prepared at the join prepair phase.
    Do the final preparation now.
		       
  RETURN
    0   OK
*/

int select_insert::prepare2(void)
{
  
  if (thd->lex->current_select->options & OPTION_BUFFER_RESULT)
    table->file->ha_start_bulk_insert((ha_rows) 0);
  return(0);
}


void select_insert::cleanup()
{
  /* select_insert/select_create are never re-used in prepared statement */
  assert(0);
}

select_insert::~select_insert()
{
  
  if (table)
  {
    table->next_number_field=0;
    table->auto_increment_field_not_null= false;
    table->file->ha_reset();
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  thd->abort_on_warning= 0;
  return;
}


bool select_insert::send_data(List<Item> &values)
{
  
  bool error=0;

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return(0);
  }

  thd->count_cuted_fields= CHECK_FIELD_WARN;	// Calculate cuted fields
  store_values(values);
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  if (thd->is_error())
    return(1);

  error= write_record(thd, table, &info);
    
  if (!error)
  {
    if (info.handle_duplicates == DUP_UPDATE)
    {
      /*
        Restore fields of the record since it is possible that they were
        changed by ON DUPLICATE KEY UPDATE clause.
    
        If triggers exist then whey can modify some fields which were not
        originally touched by INSERT ... SELECT, so we have to restore
        their original values for the next row.
      */
      restore_record(table, s->default_values);
    }
    if (table->next_number_field)
    {
      /*
        If no value has been autogenerated so far, we need to remember the
        value we just saw, we may need to send it to client in the end.
      */
      if (thd->first_successful_insert_id_in_cur_stmt == 0) // optimization
        autoinc_value_of_last_inserted_row= 
          table->next_number_field->val_int();
      /*
        Clear auto-increment field for the next record, if triggers are used
        we will clear it twice, but this should be cheap.
      */
      table->next_number_field->reset();
    }
  }
  return(error);
}


void select_insert::store_values(List<Item> &values)
{
  if (fields->elements)
    fill_record(thd, *fields, values, 1);
  else
    fill_record(thd, table->field, values, 1);
}

void select_insert::send_error(uint errcode,const char *err)
{
  

  my_message(errcode, err, MYF(0));

  return;
}


bool select_insert::send_eof()
{
  int error;
  bool const trans_table= table->file->has_transactions();
  uint64_t id;
  bool changed;
  THD::killed_state killed_status= thd->killed;
  
  error= table->file->ha_end_bulk_insert();
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  if ((changed= (info.copied || info.deleted || info.updated)))
  {
    /*
      We must invalidate the table in the query cache before binlog writing
      and ha_autocommit_or_rollback.
    */
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= true;
  }
  assert(trans_table || !changed || 
              thd->transaction.stmt.modified_non_trans_table);

  /*
    Write to binlog before commiting transaction.  No statement will
    be written by the binlog_query() below in RBR mode.  All the
    events are in the transaction cache and will be written when
    ha_autocommit_or_rollback() is issued below.
  */
  if (mysql_bin_log.is_open())
  {
    if (!error)
      thd->clear_error();
    thd->binlog_query(THD::ROW_QUERY_TYPE,
                      thd->query, thd->query_length,
                      trans_table, false, killed_status);
  }
  table->file->ha_release_auto_increment();

  if (error)
  {
    table->file->print_error(error,MYF(0));
    return(1);
  }
  char buff[160];
  if (info.ignore)
    sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	    (ulong) (info.records - info.copied), (ulong) thd->cuted_fields);
  else
    sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	    (ulong) (info.deleted+info.updated), (ulong) thd->cuted_fields);
  thd->row_count_func= info.copied + info.deleted +
                       ((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                        info.touched : info.updated);

  id= (thd->first_successful_insert_id_in_cur_stmt > 0) ?
    thd->first_successful_insert_id_in_cur_stmt :
    (thd->arg_of_last_insert_id_function ?
     thd->first_successful_insert_id_in_prev_stmt :
     (info.copied ? autoinc_value_of_last_inserted_row : 0));
  ::my_ok(thd, (ulong) thd->row_count_func, id, buff);
  return(0);
}

void select_insert::abort() {

  
  /*
    If the creation of the table failed (due to a syntax error, for
    example), no table will have been opened and therefore 'table'
    will be NULL. In that case, we still need to execute the rollback
    and the end of the function.
   */
  if (table)
  {
    bool changed, transactional_table;

    table->file->ha_end_bulk_insert();

    /*
      If at least one row has been inserted/modified and will stay in
      the table (the table doesn't have transactions) we must write to
      the binlog (and the error code will make the slave stop).

      For many errors (example: we got a duplicate key error while
      inserting into a MyISAM table), no row will be added to the table,
      so passing the error to the slave will not help since there will
      be an error code mismatch (the inserts will succeed on the slave
      with no error).

      If table creation failed, the number of rows modified will also be
      zero, so no check for that is made.
    */
    changed= (info.copied || info.deleted || info.updated);
    transactional_table= table->file->has_transactions();
    if (thd->transaction.stmt.modified_non_trans_table)
    {
        if (mysql_bin_log.is_open())
          thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query, thd->query_length,
                            transactional_table, false);
        if (!thd->current_stmt_binlog_row_based && !can_rollback_data())
          thd->transaction.all.modified_non_trans_table= true;
    }
    assert(transactional_table || !changed ||
		thd->transaction.stmt.modified_non_trans_table);
    table->file->ha_release_auto_increment();
  }

  return;
}


/***************************************************************************
  CREATE TABLE (SELECT) ...
***************************************************************************/

/*
  Create table from lists of fields and items (or just return TABLE
  object for pre-opened existing table).

  SYNOPSIS
    create_table_from_items()
      thd          in     Thread object
      create_info  in     Create information (like MAX_ROWS, ENGINE or
                          temporary table flag)
      create_table in     Pointer to TABLE_LIST object providing database
                          and name for table to be created or to be open
      alter_info   in/out Initial list of columns and indexes for the table
                          to be created
      items        in     List of items which should be used to produce rest
                          of fields for the table (corresponding fields will
                          be added to the end of alter_info->create_list)
      lock         out    Pointer to the MYSQL_LOCK object for table created
                          (or open temporary table) will be returned in this
                          parameter. Since this table is not included in
                          THD::lock caller is responsible for explicitly
                          unlocking this table.
      hooks

  NOTES
    This function behaves differently for base and temporary tables:
    - For base table we assume that either table exists and was pre-opened
      and locked at open_and_lock_tables() stage (and in this case we just
      emit error or warning and return pre-opened TABLE object) or special
      placeholder was put in table cache that guarantees that this table
      won't be created or opened until the placeholder will be removed
      (so there is an exclusive lock on this table).
    - We don't pre-open existing temporary table, instead we either open
      or create and then open table in this function.

    Since this function contains some logic specific to CREATE TABLE ...
    SELECT it should be changed before it can be used in other contexts.

  RETURN VALUES
    non-zero  Pointer to TABLE object for table created or opened
    0         Error
*/

static TABLE *create_table_from_items(THD *thd, HA_CREATE_INFO *create_info,
                                      TABLE_LIST *create_table,
                                      Alter_info *alter_info,
                                      List<Item> *items,
                                      MYSQL_LOCK **lock,
                                      TABLEOP_HOOKS *hooks)
{
  TABLE tmp_table;		// Used during 'Create_field()'
  TABLE_SHARE share;
  TABLE *table= 0;
  uint select_field_count= items->elements;
  /* Add selected items to field list */
  List_iterator_fast<Item> it(*items);
  Item *item;
  Field *tmp_field;
  bool not_used;

  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      create_table->table->db_stat)
  {
    /* Table already exists and was open at open_and_lock_tables() stage. */
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      create_info->table_existed= 1;		// Mark that table existed
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          create_table->table_name);
      return(create_table->table);
    }

    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_table->table_name);
    return(0);
  }

  tmp_table.alias= 0;
  tmp_table.timestamp_field= 0;
  tmp_table.s= &share;
  init_tmp_table_share(thd, &share, "", 0, "", "");

  tmp_table.s->db_create_options=0;
  tmp_table.s->blob_ptr_size= portable_sizeof_char_ptr;
  tmp_table.s->db_low_byte_first= 
        test(create_info->db_type == myisam_hton ||
             create_info->db_type == heap_hton);
  tmp_table.null_row=tmp_table.maybe_null=0;

  while ((item=it++))
  {
    Create_field *cr_field;
    Field *field, *def_field;
    if (item->type() == Item::FUNC_ITEM)
      if (item->result_type() != STRING_RESULT)
        field= item->tmp_table_field(&tmp_table);
      else
        field= item->tmp_table_field_from_field_type(&tmp_table, 0);
    else
      field= create_tmp_field(thd, &tmp_table, item, item->type(),
                              (Item ***) 0, &tmp_field, &def_field, 0, 0, 0, 0,
                              0);
    if (!field ||
	!(cr_field=new Create_field(field,(item->type() == Item::FIELD_ITEM ?
					   ((Item_field *)item)->field :
					   (Field*) 0))))
      return(0);
    if (item->maybe_null)
      cr_field->flags &= ~NOT_NULL_FLAG;
    alter_info->create_list.push_back(cr_field);
  }

  /*
    Create and lock table.

    Note that we either creating (or opening existing) temporary table or
    creating base table on which name we have exclusive lock. So code below
    should not cause deadlocks or races.

    We don't log the statement, it will be logged later.

    If this is a HEAP table, the automatic DELETE FROM which is written to the
    binlog when a HEAP table is opened for the first time since startup, must
    not be written: 1) it would be wrong (imagine we're in CREATE SELECT: we
    don't want to delete from it) 2) it would be written before the CREATE
    TABLE, which is a wrong order. So we keep binary logging disabled when we
    open_table().
  */
  {
    tmp_disable_binlog(thd);
    if (!mysql_create_table_no_lock(thd, create_table->db,
                                    create_table->table_name,
                                    create_info, alter_info, 0,
                                    select_field_count))
    {
      if (create_info->table_existed &&
          !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
      {
        /*
          This means that someone created table underneath server
          or it was created via different mysqld front-end to the
          cluster. We don't have much options but throw an error.
        */
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_table->table_name);
        return(0);
      }

      if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
      {
        VOID(pthread_mutex_lock(&LOCK_open));
        if (reopen_name_locked_table(thd, create_table, false))
        {
          quick_rm_table(create_info->db_type, create_table->db,
                         table_case_name(create_info, create_table->table_name),
                         0);
        }
        else
          table= create_table->table;
        VOID(pthread_mutex_unlock(&LOCK_open));
      }
      else
      {
        if (!(table= open_table(thd, create_table, thd->mem_root, (bool*) 0,
                                MYSQL_OPEN_TEMPORARY_ONLY)) &&
            !create_info->table_existed)
        {
          /*
            This shouldn't happen as creation of temporary table should make
            it preparable for open. But let us do close_temporary_table() here
            just in case.
          */
          drop_temporary_table(thd, create_table);
        }
      }
    }
    reenable_binlog(thd);
    if (!table)                                   // open failed
      return(0);
  }

  table->reginfo.lock_type=TL_WRITE;
  hooks->prelock(&table, 1);                    // Call prelock hooks
  if (! ((*lock)= mysql_lock_tables(thd, &table, 1,
                                    MYSQL_LOCK_IGNORE_FLUSH, &not_used)) ||
        hooks->postlock(&table, 1))
  {
    if (*lock)
    {
      mysql_unlock_tables(thd, *lock);
      *lock= 0;
    }

    if (!create_info->table_existed)
      drop_open_table(thd, table, create_table->db, create_table->table_name);
    return(0);
  }
  return(table);
}


int
select_create::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  MYSQL_LOCK *extra_lock= NULL;
  

  TABLEOP_HOOKS *hook_ptr= NULL;
  /*
    For row-based replication, the CREATE-SELECT statement is written
    in two pieces: the first one contain the CREATE TABLE statement
    necessary to create the table and the second part contain the rows
    that should go into the table.

    For non-temporary tables, the start of the CREATE-SELECT
    implicitly commits the previous transaction, and all events
    forming the statement will be stored the transaction cache. At end
    of the statement, the entire statement is committed as a
    transaction, and all events are written to the binary log.

    On the master, the table is locked for the duration of the
    statement, but since the CREATE part is replicated as a simple
    statement, there is no way to lock the table for accesses on the
    slave.  Hence, we have to hold on to the CREATE part of the
    statement until the statement has finished.
   */
  class MY_HOOKS : public TABLEOP_HOOKS {
  public:
    MY_HOOKS(select_create *x, TABLE_LIST *create_table,
             TABLE_LIST *select_tables)
      : ptr(x), all_tables(*create_table)
      {
        all_tables.next_global= select_tables;
      }

  private:
    virtual int do_postlock(TABLE **tables, uint count)
    {
      THD *thd= const_cast<THD*>(ptr->get_thd());
      if (int error= decide_logging_format(thd, &all_tables))
        return error;

      TABLE const *const table = *tables;
      if (thd->current_stmt_binlog_row_based  &&
          !table->s->tmp_table &&
          !ptr->get_create_info()->table_existed)
      {
        ptr->binlog_show_create_table(tables, count);
      }
      return 0;
    }

    select_create *ptr;
    TABLE_LIST all_tables;
  };

  MY_HOOKS hooks(this, create_table, select_tables);
  hook_ptr= &hooks;

  unit= u;

  /*
    Start a statement transaction before the create if we are using
    row-based replication for the statement.  If we are creating a
    temporary table, we need to start a statement transaction.
  */
  if ((thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) == 0 &&
      thd->current_stmt_binlog_row_based)
  {
    thd->binlog_start_trans_and_stmt();
  }

  if (!(table= create_table_from_items(thd, create_info, create_table,
                                       alter_info, &values,
                                       &extra_lock, hook_ptr)))
    return(-1);				// abort() deletes table

  if (extra_lock)
  {
    assert(m_plock == NULL);

    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
      m_plock= &m_lock;
    else
      m_plock= &thd->extra_lock;

    *m_plock= extra_lock;
  }

  if (table->s->fields < values.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1);
    return(-1);
  }

 /* First field to copy */
  field= table->field+table->s->fields - values.elements;

  /* Mark all fields that are given values */
  for (Field **f= field ; *f ; f++)
    bitmap_set_bit(table->write_set, (*f)->field_index);

  /* Don't set timestamp if used */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  table->next_number_field=table->found_next_number_field;

  restore_record(table,s->default_values);      // Get empty record
  thd->cuted_fields=0;
  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (info.handle_duplicates == DUP_REPLACE)
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (info.handle_duplicates == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  table->file->ha_start_bulk_insert((ha_rows) 0);
  thd->abort_on_warning= !info.ignore;
  if (check_that_all_fields_are_given_values(thd, table, table_list))
    return(1);
  table->mark_columns_needed_for_insert();
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  return(0);
}

void
select_create::binlog_show_create_table(TABLE **tables, uint count)
{
  /*
    Note 1: In RBR mode, we generate a CREATE TABLE statement for the
    created table by calling store_create_info() (behaves as SHOW
    CREATE TABLE).  In the event of an error, nothing should be
    written to the binary log, even if the table is non-transactional;
    therefore we pretend that the generated CREATE TABLE statement is
    for a transactional table.  The event will then be put in the
    transaction cache, and any subsequent events (e.g., table-map
    events and binrow events) will also be put there.  We can then use
    ha_autocommit_or_rollback() to either throw away the entire
    kaboodle of events, or write them to the binary log.

    We write the CREATE TABLE statement here and not in prepare()
    since there potentially are sub-selects or accesses to information
    schema that will do a close_thread_tables(), destroying the
    statement transaction cache.
  */
  assert(thd->current_stmt_binlog_row_based);
  assert(tables && *tables && count > 0);

  char buf[2048];
  String query(buf, sizeof(buf), system_charset_info);
  int result;
  TABLE_LIST tmp_table_list;

  memset(&tmp_table_list, 0, sizeof(tmp_table_list));
  tmp_table_list.table = *tables;
  query.length(0);      // Have to zero it since constructor doesn't

  result= store_create_info(thd, &tmp_table_list, &query, create_info);
  assert(result == 0); /* store_create_info() always return 0 */

  thd->binlog_query(THD::STMT_QUERY_TYPE,
                    query.ptr(), query.length(),
                    /* is_trans */ true,
                    /* suppress_use */ false);
}

void select_create::store_values(List<Item> &values)
{
  fill_record(thd, field, values, 1);
}


void select_create::send_error(uint errcode,const char *err)
{
  

  /*
    This will execute any rollbacks that are necessary before writing
    the transcation cache.

    We disable the binary log since nothing should be written to the
    binary log.  This disabling is important, since we potentially do
    a "roll back" of non-transactional tables by removing the table,
    and the actual rollback might generate events that should not be
    written to the binary log.

  */
  tmp_disable_binlog(thd);
  select_insert::send_error(errcode, err);
  reenable_binlog(thd);

  return;
}


bool select_create::send_eof()
{
  bool tmp=select_insert::send_eof();
  if (tmp)
    abort();
  else
  {
    /*
      Do an implicit commit at end of statement for non-temporary
      tables.  This can fail, but we should unlock the table
      nevertheless.
    */
    if (!table->s->tmp_table)
    {
      ha_autocommit_or_rollback(thd, 0);
      end_active_trans(thd);
    }

    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    if (m_plock)
    {
      mysql_unlock_tables(thd, *m_plock);
      *m_plock= NULL;
      m_plock= NULL;
    }
  }
  return tmp;
}


void select_create::abort()
{
  

  /*
    In select_insert::abort() we roll back the statement, including
    truncating the transaction cache of the binary log. To do this, we
    pretend that the statement is transactional, even though it might
    be the case that it was not.

    We roll back the statement prior to deleting the table and prior
    to releasing the lock on the table, since there might be potential
    for failure if the rollback is executed after the drop or after
    unlocking the table.

    We also roll back the statement regardless of whether the creation
    of the table succeeded or not, since we need to reset the binary
    log state.
  */
  tmp_disable_binlog(thd);
  select_insert::abort();
  thd->transaction.stmt.modified_non_trans_table= false;
  reenable_binlog(thd);


  if (m_plock)
  {
    mysql_unlock_tables(thd, *m_plock);
    *m_plock= NULL;
    m_plock= NULL;
  }

  if (table)
  {
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    if (!create_info->table_existed)
      drop_open_table(thd, table, create_table->db, create_table->table_name);
    table=0;                                    // Safety
  }
  return;
}


/*****************************************************************************
  Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List_iterator_fast<List_item>;
#endif /* HAVE_EXPLICIT_TEMPLATE_INSTANTIATION */
