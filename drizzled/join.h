/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 *
 * Defines the JOIN class
 */

#ifndef DRIZZLED_JOIN_H
#define DRIZZLED_JOIN_H

class JOIN :public Sql_alloc
{
  JOIN(const JOIN &rhs);                        /**< not implemented */
  JOIN& operator=(const JOIN &rhs);             /**< not implemented */
public:
  JOIN_TAB *join_tab;
  JOIN_TAB **best_ref;
  JOIN_TAB **map2table;    /**< mapping between table indexes and JOIN_TABs */
  JOIN_TAB *join_tab_save; /**< saved join_tab for subquery reexecution */

  Table **table;
  Table **all_tables;
  /**
    The table which has an index that allows to produce the requried ordering.
    A special value of 0x1 means that the ordering will be produced by
    passing 1st non-const table to filesort(). NULL means no such table exists.
  */
  Table *sort_by_table;

  uint32_t tables;        /**< Number of tables in the join */
  uint32_t outer_tables;  /**< Number of tables that are not inside semijoin */
  uint32_t const_tables;
  uint32_t send_group_parts;

  bool sort_and_group;
  bool first_record;
  bool full_join;
  bool group;
  bool no_field_update;
  bool do_send_rows;
  /**
    true when we want to resume nested loop iterations when
    fetching data from a cursor
  */
  bool resume_nested_loop;
  /**
    true <=> optimizer must not mark any table as a constant table.
    This is needed for subqueries in form "a IN (SELECT .. UNION SELECT ..):
    when we optimize the select that reads the results of the union from a
    temporary table, we must not mark the temp. table as constant because
    the number of rows in it may vary from one subquery execution to another.
  */
  bool no_const_tables;
  bool select_distinct;				/**< Set if SELECT DISTINCT */
  /**
    If we have the GROUP BY statement in the query,
    but the group_list was emptied by optimizer, this
    flag is true.
    It happens when fields in the GROUP BY are from
    constant table
  */
  bool group_optimized_away;

  /*
    simple_xxxxx is set if order_st/GROUP BY doesn't include any references
    to other tables than the first non-constant table in the JOIN.
    It's also set if order_st/GROUP BY is empty.
  */
  bool simple_order;
  bool simple_group;
  /**
    Is set only in case if we have a GROUP BY clause
    and no order_st BY after constant elimination of 'order'.
  */
  bool no_order;
  /** Is set if we have a GROUP BY and we have order_st BY on a constant. */
  bool skip_sort_order;
  bool union_part; /**< this subselect is part of union */
  bool optimized; /**< flag to avoid double optimization in EXPLAIN */
  bool need_tmp;
  bool hidden_group_fields;

  table_map const_table_map;
  table_map found_const_table_map;
  table_map outer_join;

  ha_rows send_records;
  ha_rows found_records;
  ha_rows examined_rows;
  ha_rows row_limit;
  ha_rows select_limit;
  /**
    Used to fetch no more than given amount of rows per one
    fetch operation of server side cursor.
    The value is checked in end_send and end_send_group in fashion, similar
    to offset_limit_cnt:
      - fetch_limit= HA_POS_ERROR if there is no cursor.
      - when we open a cursor, we set fetch_limit to 0,
      - on each fetch iteration we add num_rows to fetch to fetch_limit
  */
  ha_rows fetch_limit;

  Session	*session;
  List<Item> *fields;
  List<Item> &fields_list; /**< hold field list passed to mysql_select */
  List<TableList> *join_list; /**< list of joined tables in reverse order */
  /** unit structure (with global parameters) for this select */
  Select_Lex_Unit *unit;
  /** select that processed */
  Select_Lex *select_lex;
  SQL_SELECT *select; /**< created in optimisation phase */
  Array<Item_in_subselect> sj_subselects;

  POSITION positions[MAX_TABLES+1];
  POSITION best_positions[MAX_TABLES+1];

  /**
    Bitmap of nested joins embedding the position at the end of the current
    partial join (valid only during join optimizer run).
  */
  nested_join_map cur_embedding_map;

  double best_read;
  List<Cached_item> group_fields;
  List<Cached_item> group_fields_cache;
  Table *tmp_table;
  /** used to store 2 possible tmp table of SELECT */
  Table *exec_tmp_table1;
  Table *exec_tmp_table2;
  Item_sum **sum_funcs;
  Item_sum ***sum_funcs_end;
  /** second copy of sumfuncs (for queries with 2 temporary tables */
  Item_sum **sum_funcs2;
  Item_sum ***sum_funcs_end2;
  Item *having;
  Item *tmp_having; /**< To store having when processed temporary table */
  Item *having_history; /**< Store having for explain */
  uint64_t select_options;
  select_result *result;
  Tmp_Table_Param tmp_table_param;
  DRIZZLE_LOCK *lock;

  JOIN *tmp_join; /**< copy of this JOIN to be used with temporary tables */
  ROLLUP rollup;				/**< Used with rollup */
  DYNAMIC_ARRAY keyuse;
  Item::cond_result cond_value;
  Item::cond_result having_value;
  List<Item> all_fields; /**< to store all fields that used in query */
  /** Above list changed to use temporary table */
  List<Item> tmp_all_fields1;
  List<Item> tmp_all_fields2;
  List<Item> tmp_all_fields3;
  /** Part, shared with list above, emulate following list */
  List<Item> tmp_fields_list1;
  List<Item> tmp_fields_list2;
  List<Item> tmp_fields_list3;
  int error;

  order_st *order;
  order_st *group_list; /**< hold parameters of mysql_select */
  COND *conds;                            // ---"---
  Item *conds_history; /**< store WHERE for explain */
  TableList *tables_list; /**< hold 'tables' parameter of mysql_select */
  COND_EQUAL *cond_equal;
  JOIN_TAB *return_tab; /**< used only for outer joins */
  Item **ref_pointer_array; /**< used pointer reference for this select */
  /** Copy of above to be used with different lists */
  Item **items0;
  Item **items1;
  Item **items2;
  Item **items3;
  Item **current_ref_pointer_array;
  uint32_t ref_pointer_array_size; ///< size of above in bytes
  const char *zero_result_cause; ///< not 0 if exec must return zero result

  /* Descriptions of temporary tables used to weed-out semi-join duplicates */
  SJ_TMP_TABLE  *sj_tmp_tables;

  table_map cur_emb_sj_nests;

  /*
    storage for caching buffers allocated during query execution.
    These buffers allocations need to be cached as the thread memory pool is
    cleared only at the end of the execution of the whole query and not caching
    allocations that occur in repetition at execution time will result in
    excessive memory usage.
  */
  SORT_FIELD *sortorder;                        // make_unireg_sortorder()
  Table **table_reexec;                         // make_simple_join()
  JOIN_TAB *join_tab_reexec;                    // make_simple_join()
  /* end of allocation caching storage */

  /** Constructors */
  JOIN(Session *session_arg, 
       List<Item> &fields_arg, 
       uint64_t select_options_arg,
       select_result *result_arg)
    :
      join_tab(NULL),
      best_ref(NULL),
      map2table(NULL),
      join_tab_save(NULL),
      table(NULL),
      all_tables(NULL),
      sort_by_table(NULL),
      tables(0),
      outer_tables(0),
      const_tables(0),
      send_group_parts(0),
      sort_and_group(false),
      first_record(false),
      full_join(false),
      group(false),
      no_field_update(false),
      do_send_rows(true),
      resume_nested_loop(false),
      no_const_tables(false),
      select_distinct(false),
      group_optimized_away(false),
      simple_order(false),
      simple_group(false),
      no_order(false),
      skip_sort_order(false),
      union_part(false),
      optimized(false),
      need_tmp(false),
      hidden_group_fields(false),
      const_table_map(NULL),
      found_const_table_map(NULL),
      outer_join(NULL),
      send_records(0),
      found_records(0),
      examined_rows(0),
      row_limit(0),
      select_limit(0),
      fetch_limit(HA_POS_ERROR),
      session(session_arg),
      fields_list(fields_arg), 
      join_list(NULL),
      unit(NULL),
      select_lex(NULL),
      select(NULL),
      sj_subselects(session_arg->mem_root, 4),
      exec_tmp_table1(NULL),
      exec_tmp_table2(NULL),
      sum_funcs(NULL),
      sum_funcs2(NULL),
      having(NULL),
      tmp_having(NULL),
      having_history(NULL),
      select_options(select_options_arg),
      result(result_arg),
      lock(session_arg->lock),
      tmp_join(NULL),
      all_fields(fields_arg),
      error(0),
      cond_equal(NULL),
      return_tab(NULL),
      ref_pointer_array(NULL),
      items0(NULL),
      items1(NULL),
      items2(NULL),
      items3(NULL),
      ref_pointer_array_size(0),
      zero_result_cause(NULL),
      sj_tmp_tables(NULL),
      sortorder(NULL),
      table_reexec(NULL),
      join_tab_reexec(NULL)
  {
    select_distinct= test(select_options & SELECT_DISTINCT);
    if (&fields_list != &fields_arg) /* only copy if not same*/
      fields_list= fields_arg;
    memset(&keyuse, 0, sizeof(keyuse));
    tmp_table_param.init();
    tmp_table_param.end_write_records= HA_POS_ERROR;
    rollup.state= ROLLUP::STATE_NONE;
  }

  /** 
   * This method is currently only used when a subselect EXPLAIN is performed.
   * I pulled out the init() method and have simply reset the values to what
   * was previously in the init() method.  See the note about the hack in 
   * sql_union.cc...
   */
  inline void reset(Session *session_arg, 
       List<Item> &fields_arg, 
       uint64_t select_options_arg,
       select_result *result_arg)
  {
    join_tab= NULL;
    best_ref= NULL;
    map2table= NULL;
    join_tab_save= NULL;
    table= NULL;
    all_tables= NULL;
    sort_by_table= NULL;
    tables= 0;
    outer_tables= 0;
    const_tables= 0;
    send_group_parts= 0;
    sort_and_group= false;
    first_record= false;
    full_join= false;
    group= false;
    no_field_update= false;
    do_send_rows= true;
    resume_nested_loop= false;
    no_const_tables= false;
    select_distinct= false;
    group_optimized_away= false;
    simple_order= false;
    simple_group= false;
    no_order= false;
    skip_sort_order= false;
    union_part= false;
    optimized= false;
    need_tmp= false;
    hidden_group_fields= false;
    const_table_map= NULL;
    found_const_table_map= NULL;
    outer_join= NULL;
    send_records= 0;
    found_records= 0;
    examined_rows= 0;
    row_limit= 0;
    select_limit= 0;
    fetch_limit= HA_POS_ERROR;
    session= session_arg;
    fields_list= fields_arg; 
    join_list= NULL;
    unit= NULL;
    select_lex= NULL;
    select= NULL;
    exec_tmp_table1= NULL;
    exec_tmp_table2= NULL;
    sum_funcs= NULL;
    sum_funcs2= NULL;
    having= NULL;
    tmp_having= NULL;
    having_history= NULL;
    select_options= select_options_arg;
    result= result_arg;
    lock= session_arg->lock;
    tmp_join= NULL;
    all_fields= fields_arg;
    error= 0;
    cond_equal= NULL;
    return_tab= NULL;
    ref_pointer_array= NULL;
    items0= NULL;
    items1= NULL;
    items2= NULL;
    items3= NULL;
    ref_pointer_array_size= 0;
    zero_result_cause= NULL;
    sj_tmp_tables= NULL;
    sortorder= NULL;
    table_reexec= NULL;
    join_tab_reexec= NULL;
    select_distinct= test(select_options & SELECT_DISTINCT);
    if (&fields_list != &fields_arg) /* only copy if not same*/
      fields_list= fields_arg;
    memset(&keyuse, 0, sizeof(keyuse));
    tmp_table_param.init();
    tmp_table_param.end_write_records= HA_POS_ERROR;
    rollup.state= ROLLUP::STATE_NONE;
  }

  int prepare(Item ***rref_pointer_array, 
              TableList *tables,
              uint32_t wind_num,
              COND *conds,
              uint32_t og_num,
              order_st *order,
              order_st *group,
              Item *having,
              Select_Lex *select,
              Select_Lex_Unit *unit);
  int optimize();
  int reinit();
  void exec();
  int destroy();
  void restore_tmp();
  bool alloc_func_list();
  bool flatten_subqueries();
  bool setup_subquery_materialization();
  bool make_sum_func_list(List<Item> &all_fields, 
                          List<Item> &send_fields,
                  			  bool before_group_by,
                          bool recompute= false);

  inline void set_items_ref_array(Item **ptr)
  {
    memcpy(ref_pointer_array, ptr, ref_pointer_array_size);
    current_ref_pointer_array= ptr;
  }
  inline void init_items_ref_array()
  {
    items0= ref_pointer_array + all_fields.elements;
    memcpy(items0, ref_pointer_array, ref_pointer_array_size);
    current_ref_pointer_array= items0;
  }

  bool rollup_init();
  bool rollup_make_fields(List<Item> &all_fields, 
                          List<Item> &fields,
                  			  Item_sum ***func);
  int rollup_send_data(uint32_t idx);
  int rollup_write_data(uint32_t idx, Table *table);
  void remove_subq_pushed_predicates(Item **where);
  /**
    Release memory and, if possible, the open tables held by this execution
    plan (and nested plans). It's used to release some tables before
    the end of execution in order to increase concurrency and reduce
    memory consumption.
  */
  void join_free();
  /** Cleanup this JOIN, possibly for reuse */
  void cleanup(bool full);
  void clear();
  bool save_join_tab();
  bool init_save_join_tab();
  bool send_row_on_empty_set()
  {
    return (do_send_rows && tmp_table_param.sum_func_count != 0 &&
	    !group_list);
  }
  bool change_result(select_result *result);
  bool is_top_level_join() const
  {
    return (unit == &session->lex->unit && (unit->fake_select_lex == 0 ||
                                        select_lex == unit->fake_select_lex));
  }
};

#endif /* DRIZZLED_JOIN_H */
