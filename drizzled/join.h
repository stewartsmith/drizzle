/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
 * Defines the Join class
 */

#pragma once

#include <drizzled/dynamic_array.h>
#include <drizzled/optimizer/position.h>
#include <drizzled/sql_select.h>
#include <drizzled/tmp_table_param.h>
#include <bitset>

namespace drizzled {

class Join : public memory::SqlAlloc, boost::noncopyable
{
  /**
   * Contains a partial query execution plan which is extended during
   * cost-based optimization.
   */
  optimizer::Position positions[MAX_TABLES+1];

  /**
   * Contains the optimal query execution plan after cost-based optimization
   * has taken place. 
   */
  optimizer::Position best_positions[MAX_TABLES+1];

public:
  JoinTable *join_tab;
  JoinTable **best_ref;
  JoinTable **map2table;    /**< mapping between table indexes and JoinTables */
  JoinTable *join_tab_save; /**< saved join_tab for subquery reexecution */

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
    to other tables than the first non-constant table in the Join.
    It's also set if order_st/GROUP BY is empty.
  */
  bool simple_order;
  bool simple_group;
  /**
    Is set only in case if we have a GROUP BY clause
    and no ORDER BY after constant elimination of 'order'.
  */
  bool no_order;
  /** Is set if we have a GROUP BY and we have ORDER BY on a constant. */
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
  List<Item> &fields_list; /**< hold field list passed to select_query */
  List<TableList> *join_list; /**< list of joined tables in reverse order */
  /** unit structure (with global parameters) for this select */
  Select_Lex_Unit *unit;
  /** select that processed */
  Select_Lex *select_lex;
  optimizer::SqlSelect *select; /**< created in optimization phase */

  /**
    Bitmap of nested joins embedding the position at the end of the current
    partial join (valid only during join optimizer run).
  */
  std::bitset<64> cur_embedding_map;

  /**
   * The cost for the final query execution plan chosen after optimization
   * has completed. The QEP is stored in the best_positions variable.
   */
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
  DrizzleLock *lock;

  Join *tmp_join; /**< copy of this Join to be used with temporary tables */
  Rollup rollup;				/**< Used with rollup */
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

  Order *order;
  Order *group_list; /**< hold parameters of select_query */
  COND *conds;                            // ---"---
  Item *conds_history; /**< store WHERE for explain */
  TableList *tables_list; /**< hold 'tables' parameter of select_query */
  COND_EQUAL *cond_equal;
  JoinTable *return_tab; /**< used only for outer joins */
  Item **ref_pointer_array; /**< used pointer reference for this select */
  /** Copy of above to be used with different lists */
  Item **items0;
  Item **items1;
  Item **items2;
  Item **items3;
  Item **current_ref_pointer_array;
  uint32_t ref_pointer_array_size; ///< size of above in bytes
  const char *zero_result_cause; ///< not 0 if exec must return zero result

  /*
    storage for caching buffers allocated during query execution.
    These buffers allocations need to be cached as the thread memory pool is
    cleared only at the end of the execution of the whole query and not caching
    allocations that occur in repetition at execution time will result in
    excessive memory usage.
  */
  SortField *sortorder;                        // make_unireg_sortorder()
  Table **table_reexec;                         // make_simple_join()
  JoinTable *join_tab_reexec;                    // make_simple_join()
  /* end of allocation caching storage */

  /** Constructors */
  Join(Session *session_arg, 
       List<Item> &fields_arg, 
       uint64_t select_options_arg,
       select_result *result_arg);

  /** 
   * This method is currently only used when a subselect EXPLAIN is performed.
   * I pulled out the init() method and have simply reset the values to what
   * was previously in the init() method.  See the note about the hack in 
   * sql_union.cc...
   */
  void reset(Session *session_arg, 
             List<Item> &fields_arg, 
             uint64_t select_options_arg,
             select_result *result_arg);

  int prepare(Item ***rref_pointer_array, 
              TableList *tables,
              uint32_t wind_num,
              COND *conds,
              uint32_t og_num,
              Order *order,
              Order *group,
              Item *having,
              Select_Lex *select,
              Select_Lex_Unit *unit);

  int optimize();
  int reinit();
  void exec();
  int destroy();
  void restore_tmp();
  bool alloc_func_list();
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
    items0= ref_pointer_array + all_fields.size();
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
  /** Cleanup this Join, possibly for reuse */
  void cleanup(bool full);
  void clear();
  void save_join_tab();
  void init_save_join_tab();
  bool send_row_on_empty_set()
  {
    return (do_send_rows && tmp_table_param.sum_func_count != 0 &&
	    !group_list);
  }
  bool change_result(select_result *result);
  bool is_top_level_join() const;

  /**
   * Copy the partial query plan into the optimal query plan.
   *
   * @param[in] size the size of the plan which is to be copied
   */
  void copyPartialPlanIntoOptimalPlan(uint32_t size)
  {
    memcpy(best_positions, positions, 
           sizeof(optimizer::Position) * size);
  }

  void cache_const_exprs();

  /**
   * @param[in] index the index of the position to retrieve
   * @return a reference to the specified position in the optimal
   *         query plan
   */
  optimizer::Position &getPosFromOptimalPlan(uint32_t index)
  {
    return best_positions[index];
  }

  /**
   * @param[in] index the index of the position to retrieve
   * @return a reference to the specified position in the partial
   *         query plan
   */
  optimizer::Position &getPosFromPartialPlan(uint32_t index)
  {
    return positions[index];
  }

  /**
   * @param[in] index the index of the position to set
   * @param[in] in_pos the value to set the position to
   */
  void setPosInPartialPlan(uint32_t index, optimizer::Position &in_pos)
  {
    positions[index]= in_pos;
  }

  /**
   * @return a pointer to the first position in the partial query plan
   */
  optimizer::Position *getFirstPosInPartialPlan()
  {
    return positions;
  }

  /**
   * @param[in] index the index of the operator to retrieve from the partial
   *                  query plan
   * @return a pointer to the position in the partial query plan
   */
  optimizer::Position *getSpecificPosInPartialPlan(int32_t index)
  {
    return positions + index;
  }

};

enum_nested_loop_state evaluate_join_record(Join *join, JoinTable *join_tab, int error);
enum_nested_loop_state evaluate_null_complemented_join_record(Join *join, JoinTable *join_tab);
enum_nested_loop_state flush_cached_records(Join *join, JoinTable *join_tab, bool skip_last);
enum_nested_loop_state end_send(Join *join, JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state end_write(Join *join, JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state end_update(Join *join, JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state end_unique_update(Join *join, JoinTable *join_tab, bool end_of_records);

} /* namespace drizzled */

