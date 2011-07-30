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


/* Insert of records */

#include <config.h>
#include <cstdio>
#include <drizzled/sql_select.h>
#include <drizzled/show.h>
#include <drizzled/error.h>
#include <drizzled/name_resolution_context_state.h>
#include <drizzled/probes.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_load.h>
#include <drizzled/field/epoch.h>
#include <drizzled/lock.h>
#include <drizzled/sql_table.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/transaction_services.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/select_insert.h>
#include <drizzled/select_create.h>
#include <drizzled/table/shell.h>
#include <drizzled/alter_info.h>
#include <drizzled/sql_parse.h>
#include <drizzled/sql_lex.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session/transactions.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/table/cache.h>

namespace drizzled {

extern plugin::StorageEngine *heap_engine;
extern plugin::StorageEngine *myisam_engine;

/*
  Check if insert fields are correct.

  SYNOPSIS
    check_insert_fields()
    session                         The current thread.
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

static int check_insert_fields(Session *session, TableList *table_list,
                               List<Item> &fields, List<Item> &values,
                               bool check_unique,
                               table_map *)
{
  Table *table= table_list->table;

  if (fields.size() == 0 && values.size() != 0)
  {
    if (values.size() != table->getShare()->sizeFields())
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
    table->setWriteSet();
  }
  else
  {						// Part field list
    Select_Lex *select_lex= &session->lex().select_lex;
    Name_resolution_context *context= &select_lex->context;
    Name_resolution_context_state ctx_state;
    int res;

    if (fields.size() != values.size())
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return -1;
    }

    session->dup_field= 0;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
    */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);
    res= setup_fields(session, 0, fields, MARK_COLUMNS_WRITE, 0, 0);

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);

    if (res)
      return -1;

    if (check_unique && session->dup_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), session->dup_field->field_name);
      return -1;
    }
    if (table->timestamp_field)	// Don't automaticly set timestamp if used
    {
      if (table->timestamp_field->isWriteSet())
      {
        clear_timestamp_auto_bits(table->timestamp_field_type,
                                  TIMESTAMP_AUTO_SET_ON_INSERT);
      }
      else
      {
        table->setWriteSet(table->timestamp_field->position());
      }
    }
  }

  return 0;
}


/*
  Check update fields for the timestamp field.

  SYNOPSIS
    check_update_fields()
    session                         The current thread.
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

static int check_update_fields(Session *session, TableList *insert_table_list,
                               List<Item> &update_fields,
                               table_map *)
{
  Table *table= insert_table_list->table;
  bool timestamp_mark= false;

  if (table->timestamp_field)
  {
    /*
      Unmark the timestamp field so that we can check if this is modified
      by update_fields
    */
    timestamp_mark= table->write_set->test(table->timestamp_field->position());
    table->write_set->reset(table->timestamp_field->position());
  }

  /* Check the fields we are going to modify */
  if (setup_fields(session, 0, update_fields, MARK_COLUMNS_WRITE, 0, 0))
    return -1;

  if (table->timestamp_field)
  {
    /* Don't set timestamp column if this is modified. */
    if (table->timestamp_field->isWriteSet())
    {
      clear_timestamp_auto_bits(table->timestamp_field_type,
                                TIMESTAMP_AUTO_SET_ON_UPDATE);
    }

    if (timestamp_mark)
    {
      table->setWriteSet(table->timestamp_field->position());
    }
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
void upgrade_lock_type(Session *,
                       thr_lock_type *lock_type,
                       enum_duplicates duplic,
                       bool )
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

bool insert_query(Session *session,TableList *table_list,
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
  uint32_t value_count;
  ulong counter = 1;
  uint64_t id;
  CopyInfo info;
  Table *table= 0;
  List<List_item>::iterator its(values_list.begin());
  List_item *values;
  Name_resolution_context *context;
  Name_resolution_context_state ctx_state;
  Item *unused_conds= 0;


  /*
    Upgrade lock type if the requested lock is incompatible with
    the current connection mode or table operation.
  */
  upgrade_lock_type(session, &table_list->lock_type, duplic,
                    values_list.size() > 1);

  if (session->openTablesLock(table_list))
  {
    DRIZZLE_INSERT_DONE(1, 0);
    return true;
  }

  session->set_proc_info("init");
  session->used_tables=0;
  values= its++;
  value_count= values->size();

  if (prepare_insert(session, table_list, table, fields, values,
			   update_fields, update_values, duplic, &unused_conds,
                           false,
                           (fields.size() || !value_count ||
                            (0) != 0), !ignore))
  {
    if (table != NULL)
      table->cursor->ha_release_auto_increment();
    if (!joins_freed)
      free_underlaid_joins(session, &session->lex().select_lex);
    session->setAbortOnWarning(false);
    DRIZZLE_INSERT_DONE(1, 0);
    return true;
  }

  /* mysql_prepare_insert set table_list->table if it was not set */
  table= table_list->table;

  context= &session->lex().select_lex.context;
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
    if (values->size() != value_count)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), counter);

      if (table != NULL)
        table->cursor->ha_release_auto_increment();
      if (!joins_freed)
        free_underlaid_joins(session, &session->lex().select_lex);
      session->setAbortOnWarning(false);
      DRIZZLE_INSERT_DONE(1, 0);

      return true;
    }
    if (setup_fields(session, 0, *values, MARK_COLUMNS_READ, 0, 0))
    {
      if (table != NULL)
        table->cursor->ha_release_auto_increment();
      if (!joins_freed)
        free_underlaid_joins(session, &session->lex().select_lex);
      session->setAbortOnWarning(false);
      DRIZZLE_INSERT_DONE(1, 0);
      return true;
    }
  }
  its= values_list.begin();

  /* Restore the current context. */
  ctx_state.restore_state(context, table_list);

  /*
    Fill in the given fields and dump it to the table cursor
  */
  info.ignore= ignore;
  info.handle_duplicates=duplic;
  info.update_fields= &update_fields;
  info.update_values= &update_values;

  /*
    Count warnings for all inserts.
    For single line insert, generate an error if try to set a NOT NULL field
    to NULL.
  */
  session->count_cuted_fields= ignore ? CHECK_FIELD_WARN : CHECK_FIELD_ERROR_FOR_NULL;

  session->cuted_fields = 0L;
  table->next_number_field=table->found_next_number_field;

  error=0;
  session->set_proc_info("update");
  if (duplic == DUP_REPLACE)
    table->cursor->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplic == DUP_UPDATE)
    table->cursor->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  {
    if (duplic != DUP_ERROR || ignore)
      table->cursor->extra(HA_EXTRA_IGNORE_DUP_KEY);
    table->cursor->ha_start_bulk_insert(values_list.size());
  }


  session->setAbortOnWarning(not ignore);

  table->mark_columns_needed_for_insert();

  while ((values= its++))
  {
    if (fields.size() || !value_count)
    {
      table->restoreRecordAsDefault();	// Get empty record
      if (fill_record(session, fields, *values))
      {
	if (values_list.size() != 1 && ! session->is_error())
	{
	  info.records++;
	  continue;
	}
	/*
	  TODO: set session->abort_on_warning if values_list.elements == 1
	  and check that all items return warning in case of problem with
	  storing field.
        */
	error=1;
	break;
      }
    }
    else
    {
      table->restoreRecordAsDefault();	// Get empty record

      if (fill_record(session, table->getFields(), *values))
      {
	if (values_list.size() != 1 && ! session->is_error())
	{
	  info.records++;
	  continue;
	}
	error=1;
	break;
      }
    }

    // Release latches in case bulk insert takes a long time
    plugin::TransactionalStorageEngine::releaseTemporaryLatches(session);

    error=write_record(session, table ,&info);
    if (error)
      break;
    session->row_count++;
  }

  free_underlaid_joins(session, &session->lex().select_lex);
  joins_freed= true;

  /*
    Now all rows are inserted.  Time to update logs and sends response to
    user
  */
  {
    /*
      Do not do this release if this is a delayed insert, it would steal
      auto_inc values from the delayed_insert thread as they share Table.
    */
    table->cursor->ha_release_auto_increment();
    if (table->cursor->ha_end_bulk_insert() && !error)
    {
      table->print_error(errno,MYF(0));
      error=1;
    }
    if (duplic != DUP_ERROR || ignore)
      table->cursor->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

    transactional_table= table->cursor->has_transactions();

    changed= (info.copied || info.deleted || info.updated);
    if ((changed && error <= 0) || session->transaction.stmt.hasModifiedNonTransData())
    {
      if (session->transaction.stmt.hasModifiedNonTransData())
	session->transaction.all.markModifiedNonTransData();
    }
    assert(transactional_table || !changed || session->transaction.stmt.hasModifiedNonTransData());

  }
  session->set_proc_info("end");
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
  id= (session->first_successful_insert_id_in_cur_stmt > 0) ?
    session->first_successful_insert_id_in_cur_stmt :
    (session->arg_of_last_insert_id_function ?
     session->first_successful_insert_id_in_prev_stmt :
     ((table->next_number_field && info.copied) ?
     table->next_number_field->val_int() : 0));
  table->next_number_field=0;
  session->count_cuted_fields= CHECK_FIELD_IGNORE;
  table->auto_increment_field_not_null= false;
  if (duplic == DUP_REPLACE)
    table->cursor->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  if (error)
  {
    if (table != NULL)
      table->cursor->ha_release_auto_increment();
    if (!joins_freed)
      free_underlaid_joins(session, &session->lex().select_lex);
    session->setAbortOnWarning(false);
    DRIZZLE_INSERT_DONE(1, 0);
    return true;
  }

  if (values_list.size() == 1 && (!(session->options & OPTION_WARNINGS) ||
				    !session->cuted_fields))
  {
    session->row_count_func= info.copied + info.deleted + info.updated;
    session->my_ok((ulong) session->rowCount(),
                   info.copied + info.deleted + info.touched, id);
  }
  else
  {
    char buff[160];
    if (ignore)
      snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO), (ulong) info.records,
              (ulong) (info.records - info.copied), (ulong) session->cuted_fields);
    else
      snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO), (ulong) info.records,
	      (ulong) (info.deleted + info.updated), (ulong) session->cuted_fields);
    session->row_count_func= info.copied + info.deleted + info.updated;
    session->my_ok((ulong) session->rowCount(),
                   info.copied + info.deleted + info.touched, id, buff);
  }
  session->status_var.inserted_row_count+= session->rowCount();
  session->setAbortOnWarning(false);
  DRIZZLE_INSERT_DONE(0, session->rowCount());

  return false;
}


/*
  Check if table can be updated

  SYNOPSIS
     prepare_insert_check_table()
     session		Thread handle
     table_list		Table list
     fields		List of fields to be updated
     where		Pointer to where clause
     select_insert      Check is making for SELECT ... INSERT

   RETURN
     false ok
     true  ERROR
*/

static bool prepare_insert_check_table(Session *session, TableList *table_list,
                                             List<Item> &,
                                             bool select_insert)
{


  /*
     first table in list is the one we'll INSERT into, requires INSERT_ACL.
     all others require SELECT_ACL only. the ACL requirement below is for
     new leaves only anyway (view-constituents), so check for SELECT rather
     than INSERT.
  */

  if (setup_tables_and_check_access(session, &session->lex().select_lex.context,
                                    &session->lex().select_lex.top_join_list,
                                    table_list,
                                    &session->lex().select_lex.leaf_tables,
                                    select_insert))
    return true;

  return false;
}


/*
  Prepare items in INSERT statement

  SYNOPSIS
    prepare_insert()
    session			Thread handler
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

bool prepare_insert(Session *session, TableList *table_list,
                          Table *table, List<Item> &fields, List_item *values,
                          List<Item> &update_fields, List<Item> &update_values,
                          enum_duplicates duplic,
                          COND **,
                          bool select_insert,
                          bool check_fields, bool abort_on_warning)
{
  Select_Lex *select_lex= &session->lex().select_lex;
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
    because INSERT ... SELECT share Select_Lex it with SELECT.
  */
  if (not select_insert)
  {
    for (Select_Lex_Unit *un= select_lex->first_inner_unit();
         un;
         un= un->next_unit())
    {
      for (Select_Lex *sl= un->first_select();
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
    if (table_list->set_insert_values(session->mem_root))
      return true;
  }

  if (prepare_insert_check_table(session, table_list, fields, select_insert))
    return true;


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

    res= check_insert_fields(session, context->table_list, fields, *values,
                             !insert_into_view, &map) ||
      setup_fields(session, 0, *values, MARK_COLUMNS_READ, 0, 0);

    if (!res && check_fields)
    {
      bool saved_abort_on_warning= session->abortOnWarning();

      session->setAbortOnWarning(abort_on_warning);
      res= check_that_all_fields_are_given_values(session,
                                                  table ? table :
                                                  context->table_list->table,
                                                  context->table_list);
      session->setAbortOnWarning(saved_abort_on_warning);
    }

    if (!res && duplic == DUP_UPDATE)
    {
      res= check_update_fields(session, context->table_list, update_fields, &map);
    }

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);

    if (not res)
      res= setup_fields(session, 0, update_values, MARK_COLUMNS_READ, 0, 0);
  }

  if (res)
    return res;

  if (not table)
    table= table_list->table;

  if (not select_insert)
  {
    TableList *duplicate;
    if ((duplicate= unique_table(table_list, table_list->next_global, true)))
    {
      my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->alias);

      return true;
    }
  }

  if (duplic == DUP_UPDATE || duplic == DUP_REPLACE)
    table->prepare_for_position();

  return false;
}


	/* Check if there is more uniq keys after field */

static int last_uniq_key(Table *table,uint32_t keynr)
{
  while (++keynr < table->getShare()->sizeKeys())
    if (table->key_info[keynr].flags & HA_NOSAME)
      return 0;
  return 1;
}


/*
  Write a record to table with optional deleting of conflicting records,
  invoke proper triggers if needed.

  SYNOPSIS
     write_record()
      session   - thread context
      table - table to which record should be written
      info  - CopyInfo structure describing handling of duplicates
              and which is used for counting number of records inserted
              and deleted.

  NOTE
    Once this record will be written to table after insert trigger will
    be invoked. If instead of inserting new record we will update old one
    then both on update triggers will work instead. Similarly both on
    delete triggers will be invoked if we will delete conflicting records.

    Sets session->transaction.stmt.modified_non_trans_data to true if table which is updated didn't have
    transactions.

  RETURN VALUE
    0     - success
    non-0 - error
*/


int write_record(Session *session, Table *table,CopyInfo *info)
{
  int error;
  std::vector<unsigned char> key;
  boost::dynamic_bitset<> *save_read_set, *save_write_set;
  uint64_t prev_insert_id= table->cursor->next_insert_id;
  uint64_t insert_id_for_cur_row= 0;


  info->records++;
  save_read_set=  table->read_set;
  save_write_set= table->write_set;

  if (info->handle_duplicates == DUP_REPLACE || info->handle_duplicates == DUP_UPDATE)
  {
    while ((error=table->cursor->insertRecord(table->getInsertRecord())))
    {
      uint32_t key_nr;
      /*
        If we do more than one iteration of this loop, from the second one the
        row will have an explicit value in the autoinc field, which was set at
        the first call of handler::update_auto_increment(). So we must save
        the autogenerated value to avoid session->insert_id_for_cur_row to become
        0.
      */
      if (table->cursor->insert_id_for_cur_row > 0)
        insert_id_for_cur_row= table->cursor->insert_id_for_cur_row;
      else
        table->cursor->insert_id_for_cur_row= insert_id_for_cur_row;
      bool is_duplicate_key_error;
      if (table->cursor->is_fatal_error(error, HA_CHECK_DUP))
	goto err;
      is_duplicate_key_error= table->cursor->is_fatal_error(error, 0);
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
      if ((int) (key_nr = table->get_dup_key(error)) < 0)
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
          key_nr == table->getShare()->next_number_index &&
	  (insert_id_for_cur_row > 0))
	goto err;
      if (table->cursor->getEngine()->check_flag(HTON_BIT_DUPLICATE_POS))
      {
	if (table->cursor->rnd_pos(table->getUpdateRecord(),table->cursor->dup_ref))
	  goto err;
      }
      else
      {
	if (table->cursor->extra(HA_EXTRA_FLUSH_CACHE)) /* Not needed with NISAM */
	{
	  error=errno;
	  goto err;
	}

	if (not key.size())
	{
          key.resize(table->getShare()->max_unique_length);
	}
	key_copy(&key[0], table->getInsertRecord(), table->key_info+key_nr, 0);
	if ((error=(table->cursor->index_read_idx_map(table->getUpdateRecord(),key_nr,
                                                    &key[0], HA_WHOLE_KEY,
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
	assert(table->insert_values.size());
        table->storeRecordAsInsert();
        table->restoreRecord();
        assert(info->update_fields->size() ==
                    info->update_values->size());
        if (fill_record(session, *info->update_fields,
                                                 *info->update_values,
                                                 info->ignore))
          goto before_err;

        table->cursor->restore_auto_increment(prev_insert_id);
        if (table->next_number_field)
          table->cursor->adjust_next_insert_id_after_explicit_value(
            table->next_number_field->val_int());
        info->touched++;

        if (! table->records_are_comparable() || table->compare_records())
        {
          if ((error=table->cursor->updateRecord(table->getUpdateRecord(),
                                                table->getInsertRecord())) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
          {
            if (info->ignore &&
                !table->cursor->is_fatal_error(error, HA_CHECK_DUP_KEY))
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
            next SELECT LAST_INSERT_ID() or insert_id().
            Except if LAST_INSERT_ID(#) was in the INSERT query, which is
            handled separately by Session::arg_of_last_insert_id_function.
          */
          insert_id_for_cur_row= table->cursor->insert_id_for_cur_row= 0;
          info->copied++;
        }

        if (table->next_number_field)
          table->cursor->adjust_next_insert_id_after_explicit_value(
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
	    !table->cursor->referenced_by_foreign_key() &&
            (table->timestamp_field_type == TIMESTAMP_NO_AUTO_SET ||
             table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH))
        {
          if ((error=table->cursor->updateRecord(table->getUpdateRecord(),
					        table->getInsertRecord())) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
            goto err;
          if (error != HA_ERR_RECORD_IS_THE_SAME)
            info->deleted++;
          else
            error= 0;
          session->record_first_successful_insert_id_in_cur_stmt(table->cursor->insert_id_for_cur_row);
          /*
            Since we pretend that we have done insert we should call
            its after triggers.
          */
          goto after_n_copied_inc;
        }
        else
        {
          if ((error=table->cursor->deleteRecord(table->getUpdateRecord())))
            goto err;
          info->deleted++;
          if (!table->cursor->has_transactions())
            session->transaction.stmt.markModifiedNonTransData();
          /* Let us attempt do write_row() once more */
        }
      }
    }
    session->record_first_successful_insert_id_in_cur_stmt(table->cursor->insert_id_for_cur_row);
    /*
      Restore column maps if they where replaced during an duplicate key
      problem.
    */
    if (table->read_set != save_read_set ||
        table->write_set != save_write_set)
      table->column_bitmaps_set(*save_read_set, *save_write_set);
  }
  else if ((error=table->cursor->insertRecord(table->getInsertRecord())))
  {
    if (!info->ignore ||
        table->cursor->is_fatal_error(error, HA_CHECK_DUP))
      goto err;
    table->cursor->restore_auto_increment(prev_insert_id);
    goto gok_or_after_err;
  }

after_n_copied_inc:
  info->copied++;
  session->record_first_successful_insert_id_in_cur_stmt(table->cursor->insert_id_for_cur_row);

gok_or_after_err:
  if (!table->cursor->has_transactions())
    session->transaction.stmt.markModifiedNonTransData();
  return 0;

err:
  info->last_errno= error;
  /* current_select is NULL if this is a delayed insert */
  if (session->lex().current_select)
    session->lex().current_select->no_error= 0;        // Give error
  table->print_error(error,MYF(0));

before_err:
  table->cursor->restore_auto_increment(prev_insert_id);
  table->column_bitmaps_set(*save_read_set, *save_write_set);
  return 1;
}


/******************************************************************************
  Check that all fields with arn't null_fields are used
******************************************************************************/

int check_that_all_fields_are_given_values(Session *session, Table *entry,
                                           TableList *)
{
  int err= 0;

  for (Field **field=entry->getFields() ; *field ; field++)
  {
    if (not (*field)->isWriteSet())
    {
      /*
       * If the field doesn't have any default value
       * and there is no actual value specified in the
       * INSERT statement, throw error ER_NO_DEFAULT_FOR_FIELD.
       */
      if (((*field)->flags & NO_DEFAULT_VALUE_FLAG) &&
        ((*field)->real_type() != DRIZZLE_TYPE_ENUM))
      {
        my_error(ER_NO_DEFAULT_FOR_FIELD, MYF(0), (*field)->field_name);
        err= 1;
      }
    }
    else
    {
      /*
       * However, if an actual NULL value was specified
       * for the field and the field is a NOT NULL field,
       * throw ER_BAD_NULL_ERROR.
       *
       * Per the SQL standard, inserting NULL into a NOT NULL
       * field requires an error to be thrown.
       */
      if (((*field)->flags & NOT_NULL_FLAG) &&
          (*field)->is_null())
      {
        my_error(ER_BAD_NULL_ERROR, MYF(0), (*field)->field_name);
        err= 1;
      }
    }
  }
  return session->abortOnWarning() ? err : 0;
}

/***************************************************************************
  Store records in INSERT ... SELECT *
***************************************************************************/


/*
  make insert specific preparation and checks after opening tables

  SYNOPSIS
    insert_select_prepare()
    session         thread handler

  RETURN
    false OK
    true  Error
*/

bool insert_select_prepare(Session *session)
{
  LEX *lex= &session->lex();
  Select_Lex *select_lex= &lex->select_lex;

  /*
    Select_Lex do not belong to INSERT statement, so we can't add WHERE
    clause if table is VIEW
  */

  if (prepare_insert(session, lex->query_tables,
                           lex->query_tables->table, lex->field_list, 0,
                           lex->update_list, lex->value_list,
                           lex->duplicates,
                           &select_lex->where, true, false, false))
    return true;

  /*
    exclude first table from leaf tables list, because it belong to
    INSERT
  */
  assert(select_lex->leaf_tables != 0);
  lex->leaf_tables_insert= select_lex->leaf_tables;
  /* skip all leaf tables belonged to view where we are insert */
  select_lex->leaf_tables= select_lex->leaf_tables->next_leaf;
  return false;
}


select_insert::select_insert(TableList *table_list_par, Table *table_par,
                             List<Item> *fields_par,
                             List<Item> *update_fields,
                             List<Item> *update_values,
                             enum_duplicates duplic,
                             bool ignore_check_option_errors) :
  table_list(table_list_par), table(table_par), fields(fields_par),
  autoinc_value_of_last_inserted_row(0),
  insert_into_view(table_list_par && 0 != 0)
{
  info.handle_duplicates= duplic;
  info.ignore= ignore_check_option_errors;
  info.update_fields= update_fields;
  info.update_values= update_values;
}


int
select_insert::prepare(List<Item> &values, Select_Lex_Unit *u)
{
  int res;
  table_map map= 0;
  Select_Lex *lex_current_select_save= session->lex().current_select;


  unit= u;

  /*
    Since table in which we are going to insert is added to the first
    select, LEX::current_select should point to the first select while
    we are fixing fields from insert list.
  */
  session->lex().current_select= &session->lex().select_lex;
  res= check_insert_fields(session, table_list, *fields, values,
                           !insert_into_view, &map) ||
       setup_fields(session, 0, values, MARK_COLUMNS_READ, 0, 0);

  if (!res && fields->size())
  {
    bool saved_abort_on_warning= session->abortOnWarning();
    session->setAbortOnWarning(not info.ignore);
    res= check_that_all_fields_are_given_values(session, table_list->table,
                                                table_list);
    session->setAbortOnWarning(saved_abort_on_warning);
  }

  if (info.handle_duplicates == DUP_UPDATE && !res)
  {
    Name_resolution_context *context= &session->lex().select_lex.context;
    Name_resolution_context_state ctx_state;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /* Perform name resolution only in the first table - 'table_list'. */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);

    res= res || check_update_fields(session, context->table_list,
                                    *info.update_fields, &map);
    /*
      When we are not using GROUP BY and there are no ungrouped aggregate functions
      we can refer to other tables in the ON DUPLICATE KEY part.
      We use next_name_resolution_table descructively, so check it first (views?)
    */
    assert (!table_list->next_name_resolution_table);
    if (session->lex().select_lex.group_list.elements == 0 and
        not session->lex().select_lex.with_sum_func)
      /*
        We must make a single context out of the two separate name resolution contexts :
        the INSERT table and the tables in the SELECT part of INSERT ... SELECT.
        To do that we must concatenate the two lists
      */
      table_list->next_name_resolution_table=
        ctx_state.get_first_name_resolution_table();

    res= res || setup_fields(session, 0, *info.update_values,
                             MARK_COLUMNS_READ, 0, 0);
    if (!res)
    {
      /*
        Traverse the update values list and substitute fields from the
        select for references (Item_ref objects) to them. This is done in
        order to get correct values from those fields when the select
        employs a temporary table.
      */
      List<Item>::iterator li(info.update_values->begin());
      Item *item;

      while ((item= li++))
      {
        item->transform(&Item::update_value_transformer,
                        (unsigned char*)session->lex().current_select);
      }
    }

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);
  }

  session->lex().current_select= lex_current_select_save;
  if (res)
    return 1;
  /*
    if it is INSERT into join view then check_insert_fields already found
    real table for insert
  */
  table= table_list->table;

  /*
    Is table which we are changing used somewhere in other parts of
    query
  */
  if (unique_table(table_list, table_list->next_global))
  {
    /* Using same table for INSERT and SELECT */
    session->lex().current_select->options|= OPTION_BUFFER_RESULT;
    session->lex().current_select->join->select_options|= OPTION_BUFFER_RESULT;
  }
  else if (not (session->lex().current_select->options & OPTION_BUFFER_RESULT))
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
    table->cursor->ha_start_bulk_insert((ha_rows) 0);
  }
  table->restoreRecordAsDefault();		// Get empty record
  table->next_number_field=table->found_next_number_field;

  session->cuted_fields=0;

  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->cursor->extra(HA_EXTRA_IGNORE_DUP_KEY);

  if (info.handle_duplicates == DUP_REPLACE)
    table->cursor->extra(HA_EXTRA_WRITE_CAN_REPLACE);

  if (info.handle_duplicates == DUP_UPDATE)
    table->cursor->extra(HA_EXTRA_INSERT_WITH_UPDATE);

  session->setAbortOnWarning(not info.ignore);
  table->mark_columns_needed_for_insert();


  return res;
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
  if (session->lex().current_select->options & OPTION_BUFFER_RESULT)
    table->cursor->ha_start_bulk_insert((ha_rows) 0);

  return 0;
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
    table->cursor->ha_reset();
  }
  session->count_cuted_fields= CHECK_FIELD_IGNORE;
  session->setAbortOnWarning(false);
  return;
}


bool select_insert::send_data(List<Item> &values)
{

  bool error= false;

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return false;
  }

  session->count_cuted_fields= CHECK_FIELD_WARN;	// Calculate cuted fields
  store_values(values);
  session->count_cuted_fields= CHECK_FIELD_IGNORE;
  if (session->is_error())
    return true;

  // Release latches in case bulk insert takes a long time
  plugin::TransactionalStorageEngine::releaseTemporaryLatches(session);

  error= write_record(session, table, &info);
  table->auto_increment_field_not_null= false;

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
      table->restoreRecordAsDefault();
    }
    if (table->next_number_field)
    {
      /*
        If no value has been autogenerated so far, we need to remember the
        value we just saw, we may need to send it to client in the end.
      */
      if (session->first_successful_insert_id_in_cur_stmt == 0) // optimization
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
  if (fields->size())
    fill_record(session, *fields, values, true);
  else
    fill_record(session, table->getFields(), values, true);
}

void select_insert::send_error(drizzled::error_t errcode,const char *err)
{
  my_message(errcode, err, MYF(0));
}


bool select_insert::send_eof()
{
  int error;
  bool const trans_table= table->cursor->has_transactions();
  uint64_t id;
  bool changed;

  error= table->cursor->ha_end_bulk_insert();
  table->cursor->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  table->cursor->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  if ((changed= (info.copied || info.deleted || info.updated)))
  {
    /*
      We must invalidate the table in the query cache before binlog writing
      and autocommitOrRollback.
    */
    if (session->transaction.stmt.hasModifiedNonTransData())
      session->transaction.all.markModifiedNonTransData();
  }
  assert(trans_table || !changed ||
              session->transaction.stmt.hasModifiedNonTransData());

  table->cursor->ha_release_auto_increment();

  if (error)
  {
    table->print_error(error,MYF(0));
    DRIZZLE_INSERT_SELECT_DONE(error, 0);
    return 1;
  }
  char buff[160];
  if (info.ignore)
    snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO), (ulong) info.records,
	    (ulong) (info.records - info.copied), (ulong) session->cuted_fields);
  else
    snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO), (ulong) info.records,
	    (ulong) (info.deleted+info.updated), (ulong) session->cuted_fields);
  session->row_count_func= info.copied + info.deleted + info.updated;

  id= (session->first_successful_insert_id_in_cur_stmt > 0) ?
    session->first_successful_insert_id_in_cur_stmt :
    (session->arg_of_last_insert_id_function ?
     session->first_successful_insert_id_in_prev_stmt :
     (info.copied ? autoinc_value_of_last_inserted_row : 0));
  session->my_ok((ulong) session->rowCount(),
                 info.copied + info.deleted + info.touched, id, buff);
  session->status_var.inserted_row_count+= session->rowCount();
  DRIZZLE_INSERT_SELECT_DONE(0, session->rowCount());
  return 0;
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

    table->cursor->ha_end_bulk_insert();

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
    transactional_table= table->cursor->has_transactions();
    assert(transactional_table || !changed ||
		session->transaction.stmt.hasModifiedNonTransData());
    table->cursor->ha_release_auto_increment();
  }

  if (DRIZZLE_INSERT_SELECT_DONE_ENABLED())
  {
    DRIZZLE_INSERT_SELECT_DONE(0, info.copied + info.deleted + info.updated);
  }

  return;
}


/***************************************************************************
  CREATE TABLE (SELECT) ...
***************************************************************************/

/*
  Create table from lists of fields and items (or just return Table
  object for pre-opened existing table).

  SYNOPSIS
    create_table_from_items()
      session          in     Thread object
      create_info  in     Create information (like MAX_ROWS, ENGINE or
                          temporary table flag)
      create_table in     Pointer to TableList object providing database
                          and name for table to be created or to be open
      alter_info   in/out Initial list of columns and indexes for the table
                          to be created
      items        in     List of items which should be used to produce rest
                          of fields for the table (corresponding fields will
                          be added to the end of alter_info->create_list)
      lock         out    Pointer to the DrizzleLock object for table created
                          (or open temporary table) will be returned in this
                          parameter. Since this table is not included in
                          Session::lock caller is responsible for explicitly
                          unlocking this table.
      hooks

  NOTES
    This function behaves differently for base and temporary tables:
    - For base table we assume that either table exists and was pre-opened
      and locked at openTablesLock() stage (and in this case we just
      emit error or warning and return pre-opened Table object) or special
      placeholder was put in table cache that guarantees that this table
      won't be created or opened until the placeholder will be removed
      (so there is an exclusive lock on this table).
    - We don't pre-open existing temporary table, instead we either open
      or create and then open table in this function.

    Since this function contains some logic specific to CREATE TABLE ...
    SELECT it should be changed before it can be used in other contexts.

  RETURN VALUES
    non-zero  Pointer to Table object for table created or opened
    0         Error
*/

static Table *create_table_from_items(Session *session, HA_CREATE_INFO *create_info,
                                      TableList *create_table,
				      message::Table &table_proto,
                                      AlterInfo *alter_info,
                                      List<Item> *items,
                                      bool is_if_not_exists,
                                      DrizzleLock **lock,
				      const identifier::Table& identifier)
{
  TableShare share(message::Table::INTERNAL);
  uint32_t select_field_count= items->size();
  /* Add selected items to field list */
  List<Item>::iterator it(items->begin());
  Item *item;
  Field *tmp_field;

  if (not (identifier.isTmp()) && create_table->table->db_stat)
  {
    /* Table already exists and was open at openTablesLock() stage. */
    if (is_if_not_exists)
    {
      create_info->table_existed= 1;		// Mark that table existed
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          create_table->getTableName());
      return create_table->table;
    }

    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_table->getTableName());
    return NULL;
  }

  {
    table::Shell tmp_table(share);		// Used during 'CreateField()'

    if (not table_proto.engine().name().compare("MyISAM"))
      tmp_table.getMutableShare()->db_low_byte_first= true;
    else if (not table_proto.engine().name().compare("MEMORY"))
      tmp_table.getMutableShare()->db_low_byte_first= true;

    tmp_table.in_use= session;

    while ((item=it++))
    {
      CreateField *cr_field;
      Field *field, *def_field;
      if (item->type() == Item::FUNC_ITEM)
      {
        if (item->result_type() != STRING_RESULT)
        {
          field= item->tmp_table_field(&tmp_table);
        }
        else
        {
          field= item->tmp_table_field_from_field_type(&tmp_table, 0);
        }
      }
      else
      {
        field= create_tmp_field(session, &tmp_table, item, item->type(),
                                (Item ***) 0, &tmp_field, &def_field, false,
                                false, false, 0);
      }

      if (!field ||
          !(cr_field=new CreateField(field,(item->type() == Item::FIELD_ITEM ?
                                            ((Item_field *)item)->field :
                                            (Field*) 0))))
      {
        return NULL;
      }

      if (item->maybe_null)
      {
        cr_field->flags &= ~NOT_NULL_FLAG;
      }

      alter_info->create_list.push_back(cr_field);
    }
  }

  /*
    Create and lock table.

    Note that we either creating (or opening existing) temporary table or
    creating base table on which name we have exclusive lock. So code below
    should not cause deadlocks or races.
  */
  Table *table= 0;
  {
    if (not create_table_no_lock(session,
				 identifier,
				 create_info,
				 table_proto,
				 alter_info,
				 false,
				 select_field_count,
				 is_if_not_exists))
    {
      if (create_info->table_existed && not identifier.isTmp())
      {
        /*
          This means that someone created table underneath server
          or it was created via different mysqld front-end to the
          cluster. We don't have much options but throw an error.
        */
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_table->getTableName());
        return NULL;
      }

      if (not identifier.isTmp())
      {
        /* CREATE TABLE... has found that the table already exists for insert and is adapting to use it */
        boost::mutex::scoped_lock scopedLock(table::Cache::mutex());

        if (create_table->table)
        {
          table::Concurrent *concurrent_table= static_cast<table::Concurrent *>(create_table->table);

          if (concurrent_table->reopen_name_locked_table(create_table, session))
          {
            (void)plugin::StorageEngine::dropTable(*session, identifier);
          }
          else
          {
            table= create_table->table;
          }
        }
        else
        {
          (void)plugin::StorageEngine::dropTable(*session, identifier);
        }
      }
      else
      {
        if (not (table= session->openTable(create_table, (bool*) 0,
                                           DRIZZLE_OPEN_TEMPORARY_ONLY)) &&
            not create_info->table_existed)
        {
          /*
            This shouldn't happen as creation of temporary table should make
            it preparable for open. But let us do close_temporary_table() here
            just in case.
          */
          session->open_tables.drop_temporary_table(identifier);
        }
      }
    }
    if (not table)                                   // open failed
      return NULL;
  }

  table->reginfo.lock_type=TL_WRITE;
  if (not ((*lock)= session->lockTables(&table, 1, DRIZZLE_LOCK_IGNORE_FLUSH)))
  {
    if (*lock)
    {
      session->unlockTables(*lock);
      *lock= 0;
    }

    if (not create_info->table_existed)
      session->drop_open_table(table, identifier);

    return NULL;
  }

  return table;
}


int
select_create::prepare(List<Item> &values, Select_Lex_Unit *u)
{
  DrizzleLock *extra_lock= NULL;
  /*
    For replication, the CREATE-SELECT statement is written
    in two pieces: the first transaction messsage contains
    the CREATE TABLE statement as a CreateTableStatement message
    necessary to create the table.

    The second transaction message contains all the InsertStatement
    and associated InsertRecords that should go into the table.
   */

  unit= u;

  if (not (table= create_table_from_items(session, create_info, create_table,
					  table_proto,
					  alter_info, &values,
					  is_if_not_exists,
					  &extra_lock, identifier)))
  {
    return(-1);				// abort() deletes table
  }

  if (extra_lock)
  {
    assert(m_plock == NULL);

    if (identifier.isTmp())
      m_plock= &m_lock;
    else
      m_plock= &session->open_tables.extra_lock;

    *m_plock= extra_lock;
  }

  if (table->getShare()->sizeFields() < values.size())
  {
    my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1);
    return(-1);
  }

 /* First field to copy */
  field= table->getFields() + table->getShare()->sizeFields() - values.size();

  /* Mark all fields that are given values */
  for (Field **f= field ; *f ; f++)
  {
    table->setWriteSet((*f)->position());
  }

  /* Don't set timestamp if used */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  table->next_number_field=table->found_next_number_field;

  table->restoreRecordAsDefault();      // Get empty record
  session->cuted_fields=0;
  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->cursor->extra(HA_EXTRA_IGNORE_DUP_KEY);

  if (info.handle_duplicates == DUP_REPLACE)
    table->cursor->extra(HA_EXTRA_WRITE_CAN_REPLACE);

  if (info.handle_duplicates == DUP_UPDATE)
    table->cursor->extra(HA_EXTRA_INSERT_WITH_UPDATE);

  table->cursor->ha_start_bulk_insert((ha_rows) 0);
  session->setAbortOnWarning(not info.ignore);
  if (check_that_all_fields_are_given_values(session, table, table_list))
    return 1;

  table->mark_columns_needed_for_insert();
  table->cursor->extra(HA_EXTRA_WRITE_CACHE);
  return 0;
}

void select_create::store_values(List<Item> &values)
{
  fill_record(session, field, values, true);
}


void select_create::send_error(drizzled::error_t errcode,const char *err)
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
  select_insert::send_error(errcode, err);
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
    if (!table->getShare()->getType())
    {
      TransactionServices::autocommitOrRollback(*session, 0);
      (void) session->endActiveTransaction();
    }

    table->cursor->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->cursor->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    if (m_plock)
    {
      session->unlockTables(*m_plock);
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
  select_insert::abort();

  if (m_plock)
  {
    session->unlockTables(*m_plock);
    *m_plock= NULL;
    m_plock= NULL;
  }

  if (table)
  {
    table->cursor->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->cursor->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    if (not create_info->table_existed)
      session->drop_open_table(table, identifier);
    table= NULL;                                    // Safety
  }
}

} /* namespace drizzled */
