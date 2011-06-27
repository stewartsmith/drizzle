/* Copyright (C) 2000-2003 MySQL AB

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
  UNION  of select's
  UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com>
*/
#include <config.h>

#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/item/type_holder.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_union.h>
#include <drizzled/select_union.h>
#include <drizzled/sql_lex.h>
#include <drizzled/session.h>
#include <drizzled/item/subselect.h>

namespace drizzled {

bool drizzle_union(Session *session, LEX *, select_result *result,
		   Select_Lex_Unit *unit, uint64_t setup_tables_done_option)
{
  bool res= unit->prepare(session, result, SELECT_NO_UNLOCK | setup_tables_done_option);
  if (not res)
    res= unit->exec();
  if (res)
    unit->cleanup();
  return res;
}


/***************************************************************************
** store records in temporary table for UNION
***************************************************************************/

int select_union::prepare(List<Item> &, Select_Lex_Unit *u)
{
  unit= u;
  return 0;
}


bool select_union::send_data(List<Item> &values)
{
  int error= 0;
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  fill_record(session, table->getFields(), values, true);
  if (session->is_error())
    return 1;

  if ((error= table->cursor->insertRecord(table->getInsertRecord())))
  {
    /* create_myisam_from_heap will generate error if needed */
    if (table->cursor->is_fatal_error(error, HA_CHECK_DUP))
    {
      my_error(ER_USE_SQL_BIG_RESULT, MYF(0));
      return true;
    }
  }
  return 0;
}


bool select_union::send_eof()
{
  return 0;
}


bool select_union::flush()
{
  int error;
  if ((error=table->cursor->extra(HA_EXTRA_NO_CACHE)))
  {
    table->print_error(error, MYF(0));
    return 1;
  }
  return 0;
}

/*
  Create a temporary table to store the result of select_union.

  SYNOPSIS
    select_union::create_result_table()
      session                thread handle
      column_types       a list of items used to define columns of the
                         temporary table
      is_union_distinct  if set, the temporary table will eliminate
                         duplicates on insert
      options            create options
      table_alias        name of the temporary table

  DESCRIPTION
    Create a temporary table that is used to store the result of a UNION,
    derived table, or a materialized cursor.

  RETURN VALUE
    0                    The table has been created successfully.
    1                    create_tmp_table failed.
*/

bool
select_union::create_result_table(Session *session_arg, List<Item> *column_types,
                                  bool is_union_distinct, uint64_t options,
                                  const char *table_alias)
{
  assert(table == NULL);
  tmp_table_param.init();
  tmp_table_param.field_count= column_types->size();

  if (! (table= create_tmp_table(session_arg, &tmp_table_param, *column_types,
                                 (Order*) NULL, is_union_distinct, 1,
                                 options, HA_POS_ERROR, (char*) table_alias)))
  {
    return true;
  }

  table->cursor->extra(HA_EXTRA_WRITE_CACHE);
  table->cursor->extra(HA_EXTRA_IGNORE_DUP_KEY);

  return false;
}


/**
  Reset and empty the temporary table that stores the materialized query result.

  @note The cleanup performed here is exactly the same as for the two temp
  tables of JOIN - exec_tmp_table_[1 | 2].
*/

void select_union::cleanup()
{
  table->cursor->extra(HA_EXTRA_RESET_STATE);
  table->cursor->ha_delete_all_rows();
  table->free_io_cache();
  table->filesort_free_buffers();
}


/*
  initialization procedures before fake_select_lex preparation()

  SYNOPSIS
    Select_Lex_Unit::init_prepare_fake_select_lex()
    session		- thread handler

  RETURN
    options of SELECT
*/

void
Select_Lex_Unit::init_prepare_fake_select_lex(Session *session_arg)
{
  session_arg->lex().current_select= fake_select_lex;
  fake_select_lex->table_list.link_in_list((unsigned char *)&result_table_list,
					   (unsigned char **)
					   &result_table_list.next_local);
  fake_select_lex->context.table_list=
    fake_select_lex->context.first_name_resolution_table=
    fake_select_lex->get_table_list();

  for (Order *order= (Order *) global_parameters->order_list.first;
       order;
       order= order->next)
    order->item= &order->item_ptr;

  for (Order *order= (Order *)global_parameters->order_list.first;
       order;
       order=order->next)
  {
    (*order->item)->walk(&Item::change_context_processor, 0,
                         (unsigned char*) &fake_select_lex->context);
  }
}


bool Select_Lex_Unit::prepare(Session *session_arg, select_result *sel_result,
                              uint64_t additional_options)
{
  Select_Lex *lex_select_save= session_arg->lex().current_select;
  Select_Lex *sl, *first_sl= first_select();
  select_result *tmp_result;
  bool is_union_select;
  Table *empty_table= 0;

  describe= test(additional_options & SELECT_DESCRIBE);

  /*
    result object should be reassigned even if preparing already done for
    max/min subquery (ALL/ANY optimization)
  */
  result= sel_result;

  if (prepared)
  {
    if (describe)
    {
      /* fast reinit for EXPLAIN */
      for (sl= first_sl; sl; sl= sl->next_select())
      {
	sl->join->result= result;
	select_limit_cnt= HA_POS_ERROR;
	offset_limit_cnt= 0;
	if (result->prepare(sl->join->fields_list, this))
	{
	  return true;
	}
	sl->join->select_options|= SELECT_DESCRIBE;
	sl->join->reinit();
      }
    }
    return false;
  }
  prepared= 1;
  saved_error= false;

  session_arg->lex().current_select= sl= first_sl;
  found_rows_for_union= first_sl->options & OPTION_FOUND_ROWS;
  is_union_select= is_union() || fake_select_lex;

  /* Global option */

  if (is_union_select)
  {
    tmp_result= union_result= new select_union;
    if (describe)
      tmp_result= sel_result;
  }
  else
    tmp_result= sel_result;

  sl->context.resolve_in_select_list= true;

  for (;sl; sl= sl->next_select())
  {
    bool can_skip_order_by;
    sl->options|=  SELECT_NO_UNLOCK;
    Join *join= new Join(session_arg, sl->item_list,
			 sl->options | session_arg->options | additional_options,
			 tmp_result);
    /*
      setup_tables_done_option should be set only for very first SELECT,
      because it protect from secont setup_tables call for select-like non
      select commands (DELETE/INSERT/...) and they use only very first
      SELECT (for union it can be only INSERT ... SELECT).
    */
    additional_options&= ~OPTION_SETUP_TABLES_DONE;
    if (!join)
      goto err;

    session_arg->lex().current_select= sl;

    can_skip_order_by= is_union_select && !(sl->braces && sl->explicit_limit);

    saved_error= join->prepare(&sl->ref_pointer_array,
                               (TableList*) sl->table_list.first,
                               sl->with_wild,
                               sl->where,
                               (can_skip_order_by ? 0 :
                                sl->order_list.size()) +
                               sl->group_list.size(),
                               can_skip_order_by ?
                               (Order*) NULL : (Order *)sl->order_list.first,
                               (Order*) sl->group_list.first,
                               sl->having,
                               sl, this);
    /* There are no * in the statement anymore (for PS) */
    sl->with_wild= 0;

    if (saved_error || (saved_error= session_arg->is_fatal_error))
      goto err;
    /*
      Use items list of underlaid select for derived tables to preserve
      information about fields lengths and exact types
    */
    if (!is_union_select)
      types= first_sl->item_list;
    else if (sl == first_sl)
    {
      /*
        We need to create an empty table object. It is used
        to create tmp_table fields in Item_type_holder.
        The main reason of this is that we can't create
        field object without table.
      */
      assert(!empty_table);
      empty_table= (Table*) session->mem.calloc(sizeof(Table));
      types.clear();
      List<Item>::iterator it(sl->item_list.begin());
      while (Item* item_tmp= it++)
      {
	/* Error's in 'new' will be detected after loop */
	types.push_back(new Item_type_holder(session_arg, item_tmp));
      }

      if (session_arg->is_fatal_error)
	goto err; // out of memory
    }
    else
    {
      if (types.size() != sl->item_list.size())
      {
	my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
		   ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT),MYF(0));
	goto err;
      }
      List<Item>::iterator it(sl->item_list.begin());
      List<Item>::iterator tp(types.begin());
      Item *type, *item_tmp;
      while ((type= tp++, item_tmp= it++))
      {
        if (((Item_type_holder*)type)->join_types(session_arg, item_tmp))
	  return true;
      }
    }
  }

  if (is_union_select)
  {
    /*
      Check that it was possible to aggregate
      all collations together for UNION.
    */
    List<Item>::iterator tp(types.begin());
    Item *type;
    uint64_t create_options;

    while ((type= tp++))
    {
      if (type->result_type() == STRING_RESULT &&
          type->collation.derivation == DERIVATION_NONE)
      {
        my_error(ER_CANT_AGGREGATE_NCOLLATIONS, MYF(0), "UNION");
        goto err;
      }
    }

    create_options= (first_sl->options | session_arg->options |
                     TMP_TABLE_ALL_COLUMNS);

    if (union_result->create_result_table(session, &types, test(union_distinct),
                                          create_options, ""))
      goto err;
    memset(&result_table_list, 0, sizeof(result_table_list));
    result_table_list.setSchemaName((char*) "");
    result_table_list.alias= "union";
    result_table_list.setTableName((char *) "union");
    result_table_list.table= table= union_result->table;

    session_arg->lex().current_select= lex_select_save;
    if (item_list.is_empty())
      table->fill_item_list(item_list);
    else
    {
      /*
        We're in execution of a prepared statement or stored procedure:
        reset field items to point at fields from the created temporary table.
      */
      assert(1); // Olaf: should this be assert(false)?
    }
  }

  session_arg->lex().current_select= lex_select_save;

  return(saved_error || session_arg->is_fatal_error);

err:
  session_arg->lex().current_select= lex_select_save;
  return true;
}


bool Select_Lex_Unit::exec()
{
  Select_Lex *lex_select_save= session->lex().current_select;
  Select_Lex *select_cursor=first_select();
  uint64_t add_rows=0;
  ha_rows examined_rows= 0;

  if (executed && uncacheable.none() && ! describe)
    return false;
  executed= 1;

  if (uncacheable.any() || ! item || ! item->assigned() || describe)
  {
    if (item)
      item->reset_value_registration();
    if (optimized && item)
    {
      if (item->assigned())
      {
        item->assigned(0); // We will reinit & rexecute unit
        item->reset();
        table->cursor->ha_delete_all_rows();
      }
      /* re-enabling indexes for next subselect iteration */
      if (union_distinct && table->cursor->ha_enable_indexes(HA_KEY_SWITCH_ALL))
      {
        assert(0);
      }
    }
    for (Select_Lex *sl= select_cursor; sl; sl= sl->next_select())
    {
      ha_rows records_at_start= 0;
      session->lex().current_select= sl;

      if (optimized)
	saved_error= sl->join->reinit();
      else
      {
        set_limit(sl);
	if (sl == global_parameters || describe)
	{
	  offset_limit_cnt= 0;
	  /*
	    We can't use LIMIT at this stage if we are using ORDER BY for the
	    whole query
	  */
	  if (sl->order_list.first || describe)
	    select_limit_cnt= HA_POS_ERROR;
        }

        /*
          When using braces, SQL_CALC_FOUND_ROWS affects the whole query:
          we don't calculate found_rows() per union part.
          Otherwise, SQL_CALC_FOUND_ROWS should be done on all sub parts.
        */
        sl->join->select_options=
          (select_limit_cnt == HA_POS_ERROR || sl->braces) ?
          sl->options & ~OPTION_FOUND_ROWS : sl->options | found_rows_for_union;

	saved_error= sl->join->optimize();
      }
      if (!saved_error)
      {
	records_at_start= table->cursor->stats.records;
	sl->join->exec();
        if (sl == union_distinct)
	{
	  if (table->cursor->ha_disable_indexes(HA_KEY_SWITCH_ALL))
	    return true;
	  table->no_keyread=1;
	}
	saved_error= sl->join->error;
	offset_limit_cnt= (ha_rows)(sl->offset_limit ?
                                    sl->offset_limit->val_uint() :
                                    0);
	if (!saved_error)
	{
	  examined_rows+= session->examined_row_count;
	  if (union_result->flush())
	  {
	    session->lex().current_select= lex_select_save;
	    return 1;
	  }
	}
      }
      if (saved_error)
      {
	session->lex().current_select= lex_select_save;
	return(saved_error);
      }
      /* Needed for the following test and for records_at_start in next loop */
      int error= table->cursor->info(HA_STATUS_VARIABLE);
      if (error)
      {
        table->print_error(error, MYF(0));
        return 1;
      }
      if (found_rows_for_union && !sl->braces &&
          select_limit_cnt != HA_POS_ERROR)
      {
	/*
	  This is a union without braces. Remember the number of rows that
	  could also have been part of the result set.
	  We get this from the difference of between total number of possible
	  rows and actual rows added to the temporary table.
	*/
	add_rows+= (uint64_t) (session->limit_found_rows - (uint64_t)
			      ((table->cursor->stats.records -  records_at_start)));
      }
    }
  }
  optimized= 1;

  /* Send result to 'result' */
  saved_error= true;
  {
    if (!session->is_fatal_error)				// Check if EOM
    {
      set_limit(global_parameters);
      init_prepare_fake_select_lex(session);
      Join *join= fake_select_lex->join;
      if (!join)
      {
	/*
	  allocate JOIN for fake select only once (prevent
	  select_query automatic allocation)
          TODO: The above is nonsense. select_query() will not allocate the
          join if one already exists. There must be some other reason why we
          don't let it allocate the join. Perhaps this is because we need
          some special parameter values passed to join constructor?
	*/
	fake_select_lex->join= new Join(session, item_list, fake_select_lex->options, result);
  fake_select_lex->join->no_const_tables= true;

	/*
	  Fake Select_Lex should have item list for correctref_array
	  allocation.
	*/
	fake_select_lex->item_list= item_list;
        saved_error= select_query(session, &fake_select_lex->ref_pointer_array,
                              &result_table_list,
                              0, item_list, NULL,
                              global_parameters->order_list.size(),
                              (Order*)global_parameters->order_list.first,
                              (Order*) NULL, NULL,
                              fake_select_lex->options | SELECT_NO_UNLOCK,
                              result, this, fake_select_lex);
      }
      else
      {
        if (describe)
        {
          /*
            In EXPLAIN command, constant subqueries that do not use any
            tables are executed two times:
             - 1st time is a real evaluation to get the subquery value
             - 2nd time is to produce EXPLAIN output rows.
            1st execution sets certain members (e.g. select_result) to perform
            subquery execution rather than EXPLAIN line production. In order
            to reset them back, we re-do all of the actions (yes it is ugly):
          */
	        join->reset(session, item_list, fake_select_lex->options, result);
          saved_error= select_query(session, &fake_select_lex->ref_pointer_array,
                                &result_table_list,
                                0, item_list, NULL,
                                global_parameters->order_list.size(),
                                (Order*)global_parameters->order_list.first,
                                (Order*) NULL, NULL,
                                fake_select_lex->options | SELECT_NO_UNLOCK,
                                result, this, fake_select_lex);
        }
        else
        {
          join->examined_rows= 0;
          saved_error= join->reinit();
          join->exec();
        }
      }

      fake_select_lex->table_list.clear();
      if (!saved_error)
      {
	session->limit_found_rows = (uint64_t)table->cursor->stats.records + add_rows;
        session->examined_row_count+= examined_rows;
      }
      /*
	Mark for slow query log if any of the union parts didn't use
	indexes efficiently
      */
    }
  }
  session->lex().current_select= lex_select_save;
  return(saved_error);
}


bool Select_Lex_Unit::cleanup()
{
  int error= 0;

  if (cleaned)
  {
    return false;
  }
  cleaned= 1;

  if (union_result)
  {
    safe_delete(union_result);
    table= 0; // Safety
  }

  for (Select_Lex *sl= first_select(); sl; sl= sl->next_select())
    error|= sl->cleanup();

  if (fake_select_lex)
  {
    Join *join;
    if ((join= fake_select_lex->join))
    {
      join->tables_list= 0;
      join->tables= 0;
    }
    error|= fake_select_lex->cleanup();
    if (fake_select_lex->order_list.size())
    {
      Order *ord;
      for (ord= (Order*)fake_select_lex->order_list.first; ord; ord= ord->next)
        (*ord->item)->cleanup();
    }
  }

  return(error);
}


void Select_Lex_Unit::reinit_exec_mechanism()
{
  prepared= optimized= executed= 0;
}


/*
  change select_result object of unit

  SYNOPSIS
    Select_Lex_Unit::change_result()
    result	new select_result object
    old_result	old select_result object

  RETURN
    false - OK
    true  - error
*/

bool Select_Lex_Unit::change_result(select_result_interceptor *new_result,
                                       select_result_interceptor *old_result)
{
  bool res= false;
  for (Select_Lex *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl->join && sl->join->result == old_result)
      if (sl->join->change_result(new_result))
	return true;
  }
  if (fake_select_lex && fake_select_lex->join)
    res= fake_select_lex->join->change_result(new_result);
  return (res);
}

/*
  Get column type information for this unit.

  SYNOPSIS
    Select_Lex_Unit::get_unit_column_types()

  DESCRIPTION
    For a single-select the column types are taken
    from the list of selected items. For a union this function
    assumes that Select_Lex_Unit::prepare has been called
    and returns the type holders that were created for unioned
    column types of all selects.

  NOTES
    The implementation of this function should be in sync with
    Select_Lex_Unit::prepare()
*/

List<Item> *Select_Lex_Unit::get_unit_column_types()
{
  Select_Lex *sl= first_select();

  if (is_union())
  {
    assert(prepared);
    /* Types are generated during prepare */
    return &types;
  }

  return &sl->item_list;
}

bool Select_Lex::cleanup()
{
  bool error= false;

  if (join)
  {
    assert((Select_Lex*)join->select_lex == this);
    error= join->destroy();
    safe_delete(join);
  }
  for (Select_Lex_Unit *lex_unit= first_inner_unit(); lex_unit ;
       lex_unit= lex_unit->next_unit())
  {
    error= (bool) ((uint32_t) error | (uint32_t) lex_unit->cleanup());
  }
  non_agg_fields.clear();
  inner_refs_list.clear();
  return(error);
}


void Select_Lex::cleanup_all_joins(bool full)
{
  Select_Lex_Unit *unit;
  Select_Lex *sl;

  if (join)
    join->cleanup(full);

  for (unit= first_inner_unit(); unit; unit= unit->next_unit())
    for (sl= unit->first_select(); sl; sl= sl->next_select())
      sl->cleanup_all_joins(full);
}

} /* namespace drizzled */
