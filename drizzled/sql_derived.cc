/* Copyright (C) 2002-2003 MySQL AB

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
  Derived tables
  These were introduced by Sinisa <sinisa@mysql.com>
*/
#include <config.h>

#include <drizzled/sql_lex.h>
#include <drizzled/select_union.h>
#include <drizzled/sql_select.h>
#include <drizzled/session.h>
#include <drizzled/open_tables_state.h>

namespace drizzled {

/*
  Call given derived table processor (preparing or filling tables)

  SYNOPSIS
    handle_derived()
    lex                 LEX for this thread
    processor           procedure of derived table processing

  RETURN
    false  OK
    true   Error
*/
bool handle_derived(LEX *lex, bool (*processor)(Session*, LEX*, TableList*))
{
  bool res= false;
  if (lex->derived_tables)
  {
    lex->session->derived_tables_processing= true;
    for (Select_Lex *sl= lex->all_selects_list; sl; sl= sl->next_select_in_list())
    {
      for (TableList *cursor= sl->get_table_list(); cursor; cursor= cursor->next_local)
      {
        if ((res= (*processor)(lex->session, lex, cursor)))
          goto out;
      }
      if (lex->describe)
      {
        /*
          Force join->join_tmp creation, because we will use this JOIN
          twice for EXPLAIN and we have to have unchanged join for EXPLAINing
        */
        sl->uncacheable.set(UNCACHEABLE_EXPLAIN);
        sl->master_unit()->uncacheable.set(UNCACHEABLE_EXPLAIN);
      }
    }
  }
out:
  lex->session->derived_tables_processing= false;
  return res;
}

/*
  Create temporary table structure (but do not fill it)

  SYNOPSIS
    derived_prepare()
    session			Thread handle
    lex                 LEX for this thread
    orig_table_list     TableList for the upper SELECT

  IMPLEMENTATION
    Derived table is resolved with temporary table.

    After table creation, the above TableList is updated with a new table.

    This function is called before any command containing derived table
    is executed.

    Derived tables is stored in session->derived_tables and freed in
    close_thread_tables()

  RETURN
    false  OK
    true   Error
*/
bool derived_prepare(Session *session, LEX *, TableList *orig_table_list)
{
  Select_Lex_Unit *unit= orig_table_list->derived;
  uint64_t create_options;
  bool res= false;
  if (unit)
  {
    Select_Lex *first_select= unit->first_select();
    Table *table= 0;
    select_union *derived_result;

    /* prevent name resolving out of derived table */
    for (Select_Lex *sl= first_select; sl; sl= sl->next_select())
      sl->context.outer_context= 0;

    derived_result= new select_union;

    // Select_Lex_Unit::prepare correctly work for single select
    if ((res= unit->prepare(session, derived_result, 0)))
      goto exit;

    create_options= (first_select->options | session->options | TMP_TABLE_ALL_COLUMNS);
    /*
      Temp table is created so that it hounours if UNION without ALL is to be
      processed

      As 'distinct' parameter we always pass false (0), because underlying
      query will control distinct condition by itself. Correct test of
      distinct underlying query will be is_union &&
      !unit->union_distinct->next_select() (i.e. it is union and last distinct
      SELECT is last SELECT of UNION).
    */
    if ((res= derived_result->create_result_table(session, &unit->types, false,
                                                  create_options,
                                                  orig_table_list->alias)))
      goto exit;

    table= derived_result->table;

exit:
    /*
      if it is preparation PS only or commands that need only VIEW structure
      then we do not need real data and we can skip execution (and parameters
      is not defined, too)
    */
    if (res)
    {
      table= 0;
      delete derived_result;
    }
    else
    {
      orig_table_list->derived_result= derived_result;
      orig_table_list->table= table;
      orig_table_list->setTableName(const_cast<char *>(table->getShare()->getTableName()));
      orig_table_list->table_name_length= table->getShare()->getTableNameSize();
      table->derived_select_number= first_select->select_number;
      orig_table_list->setSchemaName((char *)"");
      orig_table_list->db_length= 0;
      /* Force read of table stats in the optimizer */
      table->cursor->info(HA_STATUS_VARIABLE);
      /* Add new temporary table to list of open derived tables */
      table->setNext(session->open_tables.getDerivedTables());
      session->open_tables.setDerivedTables(table);
    }
  }

  return res;
}

/*
  fill derived table

  SYNOPSIS
    derived_filling()
    session			Thread handle
    lex                 LEX for this thread
    unit                node that contains all SELECT's for derived tables
    orig_table_list     TableList for the upper SELECT

  IMPLEMENTATION
    Derived table is resolved with temporary table. It is created based on the
    queries defined. After temporary table is filled, if this is not EXPLAIN,
    then the entire unit / node is deleted. unit is deleted if UNION is used
    for derived table and node is deleted is it is a  simple SELECT.
    If you use this function, make sure it's not called at prepare.
    Due to evaluation of LIMIT clause it can not be used at prepared stage.

  RETURN
    false  OK
    true   Error
*/
bool derived_filling(Session *session, LEX *lex, TableList *orig_table_list)
{
  Table *table= orig_table_list->table;
  Select_Lex_Unit *unit= orig_table_list->derived;
  bool res= false;

  /*check that table creation pass without problem and it is derived table */
  if (table && unit)
  {
    Select_Lex *first_select= unit->first_select();
    select_union *derived_result= orig_table_list->derived_result;
    Select_Lex *save_current_select= lex->current_select;
    if (unit->is_union())
    {
      /* execute union without clean up */
      res= unit->exec();
    }
    else
    {
      unit->set_limit(first_select);
      if (unit->select_limit_cnt == HA_POS_ERROR)
	      first_select->options&= ~OPTION_FOUND_ROWS;

      lex->current_select= first_select;
      res= select_query(session, &first_select->ref_pointer_array,
                        (TableList*) first_select->table_list.first,
                        first_select->with_wild,
                        first_select->item_list, first_select->where,
                        (first_select->order_list.elements+
                        first_select->group_list.elements),
                        (Order *) first_select->order_list.first,
                        (Order *) first_select->group_list.first,
                        first_select->having,
                        (first_select->options | session->options | SELECT_NO_UNLOCK),
                        derived_result, unit, first_select);
    }

    if (! res)
    {
      /*
        Here we entirely fix both TableList and list of SELECT's as
        there were no derived tables
      */
      if (derived_result->flush())
        res= true;

      if (! lex->describe)
        unit->cleanup();
    }
    else
      unit->cleanup();
    lex->current_select= save_current_select;
  }
  return res;
}

} /* namespace drizzled */
