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
 * Implementation of the Join class
 * 
 * @defgroup Query_Optimizer  Query Optimizer
 * @{
 */

#include <config.h>

#include <float.h>
#include <math.h>

#include <drizzled/item/cache.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/copy_string.h>
#include <drizzled/item/uint.h>
#include <drizzled/cached_item.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_select.h> /* include join.h */
#include <drizzled/lock.h>
#include <drizzled/nested_join.h>
#include <drizzled/join.h>
#include <drizzled/join_cache.h>
#include <drizzled/show.h>
#include <drizzled/field/blob.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/optimizer/position.h>
#include <drizzled/optimizer/sargable_param.h>
#include <drizzled/optimizer/key_use.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/optimizer/sum.h>
#include <drizzled/optimizer/explain_plan.h>
#include <drizzled/optimizer/access_method_factory.h>
#include <drizzled/optimizer/access_method.h>
#include <drizzled/records.h>
#include <drizzled/probes.h>
#include <drizzled/internal/my_bit.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/session.h>
#include <drizzled/select_result.h>
#include <drizzled/debug.h>
#include <drizzled/item/subselect.h>
#include <drizzled/my_hash.h>
#include <drizzled/sql_lex.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>
#include <algorithm>

using namespace std;

namespace drizzled {

extern plugin::StorageEngine *heap_engine;

/** Declarations of static functions used in this source file. */
static bool make_group_fields(Join *main_join, Join *curr_join);
static void calc_group_buffer(Join *join, Order *group);
static bool alloc_group_fields(Join *join, Order *group);
static uint32_t cache_record_length(Join *join, uint32_t index);
static double prev_record_reads(Join *join, uint32_t idx, table_map found_ref);
static bool get_best_combination(Join *join);
static void set_position(Join *join,
                         uint32_t index,
                         JoinTable *table,
                         optimizer::KeyUse *key);
static bool choose_plan(Join *join,table_map join_tables);
static void best_access_path(Join *join, JoinTable *s,
                             Session *session,
                             table_map remaining_tables,
                             uint32_t idx,
                             double record_count,
                             double read_time);
static void optimize_straight_join(Join *join, table_map join_tables);
static bool greedy_search(Join *join, table_map remaining_tables, uint32_t depth, uint32_t prune_level);
static bool best_extension_by_limited_search(Join *join,
                                             table_map remaining_tables,
                                             uint32_t idx,
                                             double record_count,
                                             double read_time,
                                             uint32_t depth,
                                             uint32_t prune_level);
static uint32_t determine_search_depth(Join* join);
static void make_simple_join(Join*, Table*);
static void make_outerjoin_info(Join *join);
static bool make_join_select(Join *join, optimizer::SqlSelect *select,COND *item);
static void make_join_readinfo(Join&);
static void update_depend_map(Join *join);
static void update_depend_map(Join *join, Order *order);
static Order *remove_constants(Join *join,Order *first_order,COND *cond, bool change_list, bool *simple_order);
static void return_zero_rows(Join *join,
                            select_result *res,
                            TableList *tables,
                            List<Item> &fields,
                            bool send_row,
                            uint64_t select_options,
                            const char *info,
                            Item *having);
static COND *simplify_joins(Join *join, List<TableList> *join_list, COND *conds, bool top);
static int remove_duplicates(Join *join,Table *entry,List<Item> &fields, Item *having);
static int setup_without_group(Session *session, 
                               Item **ref_pointer_array,
                               TableList *tables,
                               TableList *,
                               List<Item> &fields,
                               List<Item> &all_fields,
                               COND **conds,
                               Order *order,
                               Order *group,
                               bool *hidden_group_fields);
static bool make_join_statistics(Join *join, TableList *leaves, COND *conds, DYNAMIC_ARRAY *keyuse);
static uint32_t build_bitmap_for_nested_joins(List<TableList> *join_list, uint32_t first_unused);
static Table *get_sort_by_table(Order *a, Order *b,TableList *tables);
static void reset_nj_counters(List<TableList> *join_list);
static bool test_if_subpart(Order *a,Order *b);
static void restore_prev_nj_state(JoinTable *last);
static bool add_ref_to_table_cond(Session *session, JoinTable *join_tab);
static void free_blobs(Field **ptr); /* Rename this method...conflicts with another in global namespace... */

Join::Join(Session *session_arg, 
           List<Item> &fields_arg, 
           uint64_t select_options_arg,
           select_result *result_arg) :
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
  const_table_map(0),
  found_const_table_map(0),
  outer_join(0),
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
  exec_tmp_table1(NULL),
  exec_tmp_table2(NULL),
  sum_funcs(NULL),
  sum_funcs2(NULL),
  having(NULL),
  tmp_having(NULL),
  having_history(NULL),
  select_options(select_options_arg),
  result(result_arg),
  lock(session_arg->open_tables.lock),
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
  rollup.setState(Rollup::STATE_NONE);
}

  /** 
   * This method is currently only used when a subselect EXPLAIN is performed.
   * I pulled out the init() method and have simply reset the values to what
   * was previously in the init() method.  See the note about the hack in 
   * sql_union.cc...
   */
void Join::reset(Session *session_arg, 
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
  const_table_map= 0;
  found_const_table_map= 0;
  outer_join= 0;
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
  lock= session_arg->open_tables.lock;
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
  sortorder= NULL;
  table_reexec= NULL;
  join_tab_reexec= NULL;
  select_distinct= test(select_options & SELECT_DISTINCT);
  if (&fields_list != &fields_arg) /* only copy if not same*/
    fields_list= fields_arg;
  memset(&keyuse, 0, sizeof(keyuse));
  tmp_table_param.init();
  tmp_table_param.end_write_records= HA_POS_ERROR;
  rollup.setState(Rollup::STATE_NONE);
}

bool Join::is_top_level_join() const
{
  return unit == &session->lex().unit && (unit->fake_select_lex == 0 || select_lex == unit->fake_select_lex);
}

/**
  Prepare of whole select (including sub queries in future).

  @todo
    Add check of calculation of GROUP functions and fields:
    SELECT COUNT(*)+table.col1 from table1;

  @retval
    -1   on error
  @retval
    0   on success
*/
int Join::prepare(Item ***rref_pointer_array,
                  TableList *tables_init,
                  uint32_t wild_num,
                  COND *conds_init,
                  uint32_t og_num,
                  Order *order_init,
                  Order *group_init,
                  Item *having_init,
                  Select_Lex *select_lex_arg,
                  Select_Lex_Unit *unit_arg)
{
  // to prevent double initialization on EXPLAIN
  if (optimized)
    return 0;

  conds= conds_init;
  order= order_init;
  group_list= group_init;
  having= having_init;
  tables_list= tables_init;
  select_lex= select_lex_arg;
  select_lex->join= this;
  join_list= &select_lex->top_join_list;
  union_part= unit_arg->is_union();

  session->lex().current_select->is_item_list_lookup= 1;
  /*
    If we have already executed SELECT, then it have not sense to prevent
    its table from update (see unique_table())
  */
  if (session->derived_tables_processing)
    select_lex->exclude_from_table_unique_test= true;

  /* Check that all tables, fields, conds and order are ok */

  if (!(select_options & OPTION_SETUP_TABLES_DONE) &&
      setup_tables_and_check_access(session, &select_lex->context, join_list, tables_list, &select_lex->leaf_tables, false))
  {
      return(-1);
  }

  TableList *table_ptr;
  for (table_ptr= select_lex->leaf_tables; table_ptr; table_ptr= table_ptr->next_leaf)
  {
    tables++;
  }


  if (setup_wild(session, fields_list, &all_fields, wild_num))
    return -1;
  select_lex->setup_ref_array(session, og_num);
  if (setup_fields(session, *rref_pointer_array, fields_list, MARK_COLUMNS_READ, &all_fields, 1) ||
    setup_without_group(session, *rref_pointer_array, tables_list, select_lex->leaf_tables, fields_list,
    all_fields, &conds, order, group_list, &hidden_group_fields))
    return -1;

  ref_pointer_array= *rref_pointer_array;

  if (having)
  {
    nesting_map save_allow_sum_func= session->lex().allow_sum_func;
    session->setWhere("having clause");
    session->lex().allow_sum_func|= 1 << select_lex_arg->nest_level;
    select_lex->having_fix_field= 1;
    bool having_fix_rc= (!having->fixed &&
       (having->fix_fields(session, &having) ||
        having->check_cols(1)));
    select_lex->having_fix_field= 0;
    if (having_fix_rc || session->is_error())
      return(-1);
    session->lex().allow_sum_func= save_allow_sum_func;
  }

  {
    Item_subselect *subselect;
    Item_in_subselect *in_subs= NULL;
    /*
      Are we in a subquery predicate?
      TODO: the block below will be executed for every PS execution without need.
    */
    if ((subselect= select_lex->master_unit()->item))
    {
      if (subselect->substype() == Item_subselect::IN_SUBS)
        in_subs= (Item_in_subselect*)subselect;

      {
        bool do_materialize= true;
        /*
          Check if the subquery predicate can be executed via materialization.
          The required conditions are:
          1. Subquery predicate is an IN/=ANY subq predicate
          2. Subquery is a single SELECT (not a UNION)
          3. Subquery is not a table-less query. In this case there is no
             point in materializing.
          4. Subquery predicate is a top-level predicate
             (this implies it is not negated)
             TODO: this is a limitation that should be lifeted once we
             implement correct NULL semantics (WL#3830)
          5. Subquery is non-correlated
             TODO:
             This is an overly restrictive condition. It can be extended to:
             (Subquery is non-correlated ||
              Subquery is correlated to any query outer to IN predicate ||
              (Subquery is correlated to the immediate outer query &&
               Subquery !contains {GROUP BY, ORDER BY [LIMIT],
               aggregate functions) && subquery predicate is not under "NOT IN"))
          6. No execution method was already chosen (by a prepared statement).

          (*) The subquery must be part of a SELECT statement. The current
               condition also excludes multi-table update statements.

          We have to determine whether we will perform subquery materialization
          before calling the IN=>EXISTS transformation, so that we know whether to
          perform the whole transformation or only that part of it which wraps
          Item_in_subselect in an Item_in_optimizer.
        */
        if (do_materialize &&
            in_subs  &&                                                   // 1
            !select_lex->master_unit()->first_select()->next_select() &&  // 2
            select_lex->master_unit()->first_select()->leaf_tables &&     // 3
            session->lex().sql_command == SQLCOM_SELECT)                       // *
        {
          if (in_subs->is_top_level_item() &&                             // 4
              !in_subs->is_correlated &&                                  // 5
              in_subs->exec_method == Item_in_subselect::NOT_TRANSFORMED) // 6
            in_subs->exec_method= Item_in_subselect::MATERIALIZATION;
        }

        Item_subselect::trans_res trans_res;
        if ((trans_res= subselect->select_transformer(this)) !=
            Item_subselect::RES_OK)
        {
          return((trans_res == Item_subselect::RES_ERROR));
        }
      }
    }
  }

  if (order)
  {
    Order *ord;
    for (ord= order; ord; ord= ord->next)
    {
      Item *item= *ord->item;
      if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
        item->split_sum_func(session, ref_pointer_array, all_fields);
    }
  }

  if (having && having->with_sum_func)
    having->split_sum_func(session, ref_pointer_array, all_fields,
                           &having, true);
  if (select_lex->inner_sum_func_list)
  {
    Item_sum *end=select_lex->inner_sum_func_list;
    Item_sum *item_sum= end;
    do
    {
      item_sum= item_sum->next;
      item_sum->split_sum_func(session, ref_pointer_array,
                               all_fields, item_sum->ref_by, false);
    } while (item_sum != end);
  }

  if (select_lex->inner_refs_list.size() &&
      fix_inner_refs(session, all_fields, select_lex, ref_pointer_array))
    return(-1);

  /*
    Check if there are references to un-aggregated columns when computing
    aggregate functions with implicit grouping (there is no GROUP BY).

    MODE_ONLY_FULL_GROUP_BY is enabled here by default
  */
  if (! group_list && 
      select_lex->full_group_by_flag.test(NON_AGG_FIELD_USED) &&
      select_lex->full_group_by_flag.test(SUM_FUNC_USED))
  {
    my_message(ER_MIX_OF_GROUP_FUNC_AND_FIELDS,
               ER(ER_MIX_OF_GROUP_FUNC_AND_FIELDS), MYF(0));
    return(-1);
  }
  {
    /* Caclulate the number of groups */
    send_group_parts= 0;
    for (Order *group_tmp= group_list ; group_tmp ; group_tmp= group_tmp->next)
      send_group_parts++;
  }

  if (error)
    return(-1);

  /* 
   * The below will create the new table for
   * CREATE TABLE ... SELECT
   *
   * @see create_table_from_items() in drizzled/sql_insert.cc
   */
  if (result && result->prepare(fields_list, unit_arg))
    return(-1);

  /* Init join struct */
  count_field_types(select_lex, &tmp_table_param, all_fields, 0);
  ref_pointer_array_size= all_fields.size() * sizeof(Item*);
  this->group= group_list != 0;
  unit= unit_arg;

#ifdef RESTRICTED_GROUP
  if (sum_func_count && !group_list && (func_count || field_count))
  {
    my_message(ER_WRONG_SUM_SELECT,ER(ER_WRONG_SUM_SELECT),MYF(0));
    return(-1);
  }
#endif
  if (select_lex->olap == ROLLUP_TYPE && rollup_init())
    return(-1);

  if (alloc_func_list())
    return(-1);

  return 0; // All OK
}

/*
  Remove the predicates pushed down into the subquery

  SYNOPSIS
    Join::remove_subq_pushed_predicates()
      where   IN  Must be NULL
              OUT The remaining WHERE condition, or NULL

  DESCRIPTION
    Given that this join will be executed using (unique|index)_subquery,
    without "checking NULL", remove the predicates that were pushed down
    into the subquery.

    If the subquery compares scalar values, we can remove the condition that
    was wrapped into trig_cond (it will be checked when needed by the subquery
    engine)

    If the subquery compares row values, we need to keep the wrapped
    equalities in the WHERE clause: when the left (outer) tuple has both NULL
    and non-NULL values, we'll do a full table scan and will rely on the
    equalities corresponding to non-NULL parts of left tuple to filter out
    non-matching records.

    TODO: We can remove the equalities that will be guaranteed to be true by the
    fact that subquery engine will be using index lookup. This must be done only
    for cases where there are no conversion errors of significance, e.g. 257
    that is searched in a byte. But this requires homogenization of the return
    codes of all Field*::store() methods.
*/
void Join::remove_subq_pushed_predicates(Item **where)
{
  if (conds->type() == Item::FUNC_ITEM &&
      ((Item_func *)this->conds)->functype() == Item_func::EQ_FUNC &&
      ((Item_func *)conds)->arguments()[0]->type() == Item::REF_ITEM &&
      ((Item_func *)conds)->arguments()[1]->type() == Item::FIELD_ITEM &&
      test_if_ref ((Item_field *)((Item_func *)conds)->arguments()[1],
                   ((Item_func *)conds)->arguments()[0]))
  {
    *where= 0;
    return;
  }
}

/**
  global select optimisation.

  @note
    error code saved in field 'error'

  @retval
    0   success
  @retval
    1   error
*/
int Join::optimize()
{
  // to prevent double initialization on EXPLAIN
  if (optimized)
    return 0;
  optimized= 1;

  session->set_proc_info("optimizing");
  row_limit= ((select_distinct || order || group_list) ? HA_POS_ERROR :
        unit->select_limit_cnt);
  /* select_limit is used to decide if we are likely to scan the whole table */
  select_limit= unit->select_limit_cnt;
  if (having || (select_options & OPTION_FOUND_ROWS))
    select_limit= HA_POS_ERROR;
  do_send_rows = (unit->select_limit_cnt) ? 1 : 0;
  // Ignore errors of execution if option IGNORE present
  if (session->lex().ignore)
    session->lex().current_select->no_error= 1;

#ifdef HAVE_REF_TO_FIELDS     // Not done yet
  /* Add HAVING to WHERE if possible */
  if (having && !group_list && !sum_func_count)
  {
    if (!conds)
    {
      conds= having;
      having= 0;
    }
    else
    {
      conds= new Item_cond_and(conds,having);

      /*
        Item_cond_and can't be fixed after creation, so we do not check
        conds->fixed
      */
      conds->fix_fields(session, &conds);
      conds->change_ref_to_fields(session, tables_list);
      conds->top_level_item();
      having= 0;
    }
  }
#endif

  /* Convert all outer joins to inner joins if possible */
  conds= simplify_joins(this, join_list, conds, true);
  build_bitmap_for_nested_joins(join_list, 0);

  conds= optimize_cond(this, conds, join_list, &cond_value);
  if (session->is_error())
  {
    error= 1;
    return 1;
  }

  {
    having= optimize_cond(this, having, join_list, &having_value);
    if (session->is_error())
    {
      error= 1;
      return 1;
    }
    if (select_lex->where)
      select_lex->cond_value= cond_value;
    if (select_lex->having)
      select_lex->having_value= having_value;

    if (cond_value == Item::COND_FALSE || having_value == Item::COND_FALSE ||
        (!unit->select_limit_cnt && !(select_options & OPTION_FOUND_ROWS)))
    {           /* Impossible cond */
      zero_result_cause=  having_value == Item::COND_FALSE ?
                           "Impossible HAVING" : "Impossible WHERE";
      tables = 0;
      goto setup_subq_exit;
    }
  }

  /* Optimize count(*), cmin() and cmax() */
  if (tables_list && tmp_table_param.sum_func_count && ! group_list)
  {
    int res;
    /*
      optimizer::sum_query() returns HA_ERR_KEY_NOT_FOUND if no rows match
      to the WHERE conditions,
      or 1 if all items were resolved,
      or 0, or an error number HA_ERR_...
    */
    if ((res= optimizer::sum_query(select_lex->leaf_tables, all_fields, conds)))
    {
      if (res == HA_ERR_KEY_NOT_FOUND)
      {
        zero_result_cause= "No matching min/max row";
        tables = 0;
        goto setup_subq_exit;
      }
      if (res > 1)
      {
        error= res;
        return 1;
      }
      if (res < 0)
      {
        zero_result_cause= "No matching min/max row";
        tables = 0;
        goto setup_subq_exit;
      }
      zero_result_cause= "Select tables optimized away";
      tables_list= 0;       // All tables resolved
      const_tables= tables;
      /*
        Extract all table-independent conditions and replace the WHERE
        clause with them. All other conditions were computed by optimizer::sum_query
        and the MIN/MAX/COUNT function(s) have been replaced by constants,
        so there is no need to compute the whole WHERE clause again.
        Notice that make_cond_for_table() will always succeed to remove all
        computed conditions, because optimizer::sum_query() is applicable only to
        conjunctions.
        Preserve conditions for EXPLAIN.
      */
      if (conds && !(session->lex().describe & DESCRIBE_EXTENDED))
      {
        COND *table_independent_conds= make_cond_for_table(conds, PSEUDO_TABLE_BITS, 0, 0);
        conds= table_independent_conds;
      }
      goto setup_subq_exit;
    }
  }
  if (!tables_list)
  {
    error= 0;
    return 0;
  }
  error= -1;          // Error is sent to client
  sort_by_table= get_sort_by_table(order, group_list, select_lex->leaf_tables);

  /* Calculate how to do the join */
  session->set_proc_info("statistics");
  if (make_join_statistics(this, select_lex->leaf_tables, conds, &keyuse) ||
      session->is_fatal_error)
  {
    return 1;
  }

  /* Remove distinct if only const tables */
  select_distinct= select_distinct && (const_tables != tables);
  session->set_proc_info("preparing");
  if (result->initialize_tables(this))
  {
    return 1;        // error == -1
  }
  if (const_table_map != found_const_table_map &&
      !(select_options & SELECT_DESCRIBE) &&
      (!conds ||
       !(conds->used_tables() & RAND_TABLE_BIT) ||
       select_lex->master_unit() == &session->lex().unit)) // upper level SELECT
  {
    zero_result_cause= "no matching row in const table";
    goto setup_subq_exit;
  }
  if (!(session->options & OPTION_BIG_SELECTS) &&
      best_read > (double) session->variables.max_join_size &&
      !(select_options & SELECT_DESCRIBE))
  {
    my_message(ER_TOO_BIG_SELECT, ER(ER_TOO_BIG_SELECT), MYF(0));
    error= -1;
    return 1;
  }
  if (const_tables && !(select_options & SELECT_NO_UNLOCK))
    session->unlockSomeTables(table, const_tables);
  if (!conds && outer_join)
  {
    /* Handle the case where we have an OUTER JOIN without a WHERE */
    conds=new Item_int((int64_t) 1,1);  // Always true
  }
  select= optimizer::make_select(*table, const_table_map,
                                 const_table_map, conds, 1, &error);
  if (error)
  {
    error= -1;
    return 1;
  }

  reset_nj_counters(join_list);
  make_outerjoin_info(this);

  /*
    Among the equal fields belonging to the same multiple equality
    choose the one that is to be retrieved first and substitute
    all references to these in where condition for a reference for
    the selected field.
  */
  if (conds)
  {
    conds= substitute_for_best_equal_field(conds, cond_equal, map2table);
    conds->update_used_tables();
  }

  /*
    Permorm the the optimization on fields evaluation mentioned above
    for all on expressions.
  */
  for (JoinTable *tab= join_tab + const_tables; tab < join_tab + tables ; tab++)
  {
    if (*tab->on_expr_ref)
    {
      *tab->on_expr_ref= substitute_for_best_equal_field(*tab->on_expr_ref,
                                                         tab->cond_equal,
                                                         map2table);
      (*tab->on_expr_ref)->update_used_tables();
    }
  }

  if (conds &&!outer_join && const_table_map != found_const_table_map &&
      (select_options & SELECT_DESCRIBE) &&
      select_lex->master_unit() == &session->lex().unit) // upper level SELECT
  {
    conds=new Item_int(0, 1);  // Always false
  }

  if (make_join_select(this, select, conds))
  {
    zero_result_cause=
      "Impossible WHERE noticed after reading const tables";
    goto setup_subq_exit;
  }

  error= -1;          /* if goto err */

  /* Optimize distinct away if possible */
  {
    Order *org_order= order;
    order= remove_constants(this, order,conds,1, &simple_order);
    if (session->is_error())
    {
      error= 1;
      return 1;
    }

    /*
      If we are using ORDER BY NULL or ORDER BY const_expression,
      return result in any order (even if we are using a GROUP BY)
    */
    if (!order && org_order)
      skip_sort_order= 1;
  }
  /*
     Check if we can optimize away GROUP BY/DISTINCT.
     We can do that if there are no aggregate functions, the
     fields in DISTINCT clause (if present) and/or columns in GROUP BY
     (if present) contain direct references to all key parts of
     an unique index (in whatever order) and if the key parts of the
     unique index cannot contain NULLs.
     Note that the unique keys for DISTINCT and GROUP BY should not
     be the same (as long as they are unique).

     The FROM clause must contain a single non-constant table.
  */
  if (tables - const_tables == 1 && (group_list || select_distinct) &&
      ! tmp_table_param.sum_func_count &&
      (! join_tab[const_tables].select ||
       ! join_tab[const_tables].select->quick ||
       join_tab[const_tables].select->quick->get_type() !=
       optimizer::QuickSelectInterface::QS_TYPE_GROUP_MIN_MAX))
  {
    if (group_list && list_contains_unique_index(join_tab[const_tables].table, find_field_in_order_list, (void *) group_list))
    {
      /*
        We have found that grouping can be removed since groups correspond to
        only one row anyway, but we still have to guarantee correct result
        order. The line below effectively rewrites the query from GROUP BY
        <fields> to ORDER BY <fields>. There are two exceptions:
        - if skip_sort_order is set (see above), then we can simply skip
          GROUP BY;
        - we can only rewrite ORDER BY if the ORDER BY fields are 'compatible'
          with the GROUP BY ones, i.e. either one is a prefix of another.
          We only check if the ORDER BY is a prefix of GROUP BY. In this case
          test_if_subpart() copies the ASC/DESC attributes from the original
          ORDER BY fields.
          If GROUP BY is a prefix of order_st BY, then it is safe to leave
          'order' as is.
       */
      if (! order || test_if_subpart(group_list, order))
          order= skip_sort_order ? 0 : group_list;
      /*
        If we have an IGNORE INDEX FOR GROUP BY(fields) clause, this must be
        rewritten to IGNORE INDEX FOR order_st BY(fields).
      */
      join_tab->table->keys_in_use_for_order_by=
        join_tab->table->keys_in_use_for_group_by;
      group_list= 0;
      group= 0;
    }
    if (select_distinct &&
       list_contains_unique_index(join_tab[const_tables].table,
                                 find_field_in_item_list,
                                 (void *) &fields_list))
    {
      select_distinct= 0;
    }
  }
  if (group_list || tmp_table_param.sum_func_count)
  {
    if (! hidden_group_fields && rollup.getState() == Rollup::STATE_NONE)
      select_distinct=0;
  }
  else if (select_distinct && tables - const_tables == 1)
  {
    /*
      We are only using one table. In this case we change DISTINCT to a
      GROUP BY query if:
      - The GROUP BY can be done through indexes (no sort) and the order_st
        BY only uses selected fields.
        (In this case we can later optimize away GROUP BY and order_st BY)
      - We are scanning the whole table without LIMIT
        This can happen if:
        - We are using CALC_FOUND_ROWS
        - We are using an ORDER BY that can't be optimized away.

      We don't want to use this optimization when we are using LIMIT
      because in this case we can just create a temporary table that
      holds LIMIT rows and stop when this table is full.
    */
    JoinTable *tab= &join_tab[const_tables];
    bool all_order_fields_used;
    if (order)
      skip_sort_order= test_if_skip_sort_order(tab, order, select_limit, 1,
        &tab->table->keys_in_use_for_order_by);
    if ((group_list=create_distinct_group(session, select_lex->ref_pointer_array,
                                          order, fields_list, all_fields,
                  &all_order_fields_used)))
    {
      bool skip_group= (skip_sort_order &&
        test_if_skip_sort_order(tab, group_list, select_limit, 1,
                                &tab->table->keys_in_use_for_group_by) != 0);
      count_field_types(select_lex, &tmp_table_param, all_fields, 0);
      if ((skip_group && all_order_fields_used) ||
          select_limit == HA_POS_ERROR ||
          (order && !skip_sort_order))
      {
        /*  Change DISTINCT to GROUP BY */
        select_distinct= 0;
        no_order= !order;
        if (all_order_fields_used)
        {
          if (order && skip_sort_order)
          {
            /*
              Force MySQL to read the table in sorted order to get result in
              ORDER BY order.
            */
            tmp_table_param.quick_group=0;
          }
          order=0;
        }
        group=1;        // For end_write_group
      }
      else
        group_list= 0;
    }
    else if (session->is_fatal_error)     // End of memory
      return 1;
  }
  simple_group= 0;
  {
    Order *old_group_list;
    group_list= remove_constants(this, (old_group_list= group_list), conds,
                                 rollup.getState() == Rollup::STATE_NONE,
                                 &simple_group);
    if (session->is_error())
    {
      error= 1;
      return 1;
    }
    if (old_group_list && !group_list)
      select_distinct= 0;
  }
  if (!group_list && group)
  {
    order=0;          // The output has only one row
    simple_order=1;
    select_distinct= 0;                       // No need in distinct for 1 row
    group_optimized_away= 1;
  }

  calc_group_buffer(this, group_list);
  send_group_parts= tmp_table_param.group_parts; /* Save org parts */

  if (test_if_subpart(group_list, order) ||
      (!group_list && tmp_table_param.sum_func_count))
    order=0;

  // Can't use sort on head table if using row cache
  if (full_join)
  {
    if (group_list)
      simple_group=0;
    if (order)
      simple_order=0;
  }

  /*
    Check if we need to create a temporary table.
    This has to be done if all tables are not already read (const tables)
    and one of the following conditions holds:
    - We are using DISTINCT (simple distinct's are already optimized away)
    - We are using an ORDER BY or GROUP BY on fields not in the first table
    - We are using different ORDER BY and GROUP BY orders
    - The user wants us to buffer the result.
  */
  need_tmp= (const_tables != tables &&
       ((select_distinct || !simple_order || !simple_group) ||
        (group_list && order) ||
        test(select_options & OPTION_BUFFER_RESULT)));

  // No cache for MATCH == 'Don't use join buffering when we use MATCH'.
  make_join_readinfo(*this);

  /* Create all structures needed for materialized subquery execution. */
  if (setup_subquery_materialization())
    return 1;

  /* Cache constant expressions in WHERE, HAVING, ON clauses. */
  cache_const_exprs();

  /*
    is this simple IN subquery?
  */
  if (!group_list && !order &&
      unit->item && unit->item->substype() == Item_subselect::IN_SUBS &&
      tables == 1 && conds &&
      !unit->is_union())
  {
    if (!having)
    {
      Item *where= conds;
      if (join_tab[0].type == AM_EQ_REF && join_tab[0].ref.items[0]->name == in_left_expr_name)
      {
        remove_subq_pushed_predicates(&where);
        save_index_subquery_explain_info(join_tab, where);
        join_tab[0].type= AM_UNIQUE_SUBQUERY;
        error= 0;
        return(unit->item->change_engine(new subselect_uniquesubquery_engine(session, join_tab, unit->item, where)));
      }
      else if (join_tab[0].type == AM_REF &&
         join_tab[0].ref.items[0]->name == in_left_expr_name)
      {
        remove_subq_pushed_predicates(&where);
        save_index_subquery_explain_info(join_tab, where);
        join_tab[0].type= AM_INDEX_SUBQUERY;
        error= 0;
        return(unit->item->change_engine(new subselect_indexsubquery_engine(session, join_tab, unit->item, where, NULL, 0)));
      }
    } 
    else if (join_tab[0].type == AM_REF_OR_NULL &&
         join_tab[0].ref.items[0]->name == in_left_expr_name &&
               having->name == in_having_cond)
    {
      join_tab[0].type= AM_INDEX_SUBQUERY;
      error= 0;
      conds= remove_additional_cond(conds);
      save_index_subquery_explain_info(join_tab, conds);
      return(unit->item->change_engine(new subselect_indexsubquery_engine(session, join_tab, unit->item, conds, having, 1)));
    }

  }
  /*
    Need to tell handlers that to play it safe, it should fetch all
    columns of the primary key of the tables: this is because MySQL may
    build row pointers for the rows, and for all columns of the primary key
    the read set has not necessarily been set by the server code.
  */
  if (need_tmp || select_distinct || group_list || order)
  {
    for (uint32_t i = const_tables; i < tables; i++)
      join_tab[i].table->prepare_for_position();
  }

  if (const_tables != tables)
  {
    /*
      Because filesort always does a full table scan or a quick range scan
      we must add the removed reference to the select for the table.
      We only need to do this when we have a simple_order or simple_group
      as in other cases the join is done before the sort.
    */
    if ((order || group_list) &&
        (join_tab[const_tables].type != AM_ALL) &&
        (join_tab[const_tables].type != AM_REF_OR_NULL) &&
        ((order && simple_order) || (group_list && simple_group)))
    {
      if (add_ref_to_table_cond(session,&join_tab[const_tables])) {
        return 1;
      }
    }

    if (!(select_options & SELECT_BIG_RESULT) &&
        ((group_list &&
          (!simple_group ||
           !test_if_skip_sort_order(&join_tab[const_tables], group_list,
                                    unit->select_limit_cnt, 0,
                                    &join_tab[const_tables].table->
                                    keys_in_use_for_group_by))) ||
         select_distinct) &&
        tmp_table_param.quick_group)
    {
      need_tmp=1; simple_order=simple_group=0;  // Force tmp table without sort
    }
    if (order)
    {
      /*
        Force using of tmp table if sorting by a SP or UDF function due to
        their expensive and probably non-deterministic nature.
      */
      for (Order *tmp_order= order; tmp_order ; tmp_order=tmp_order->next)
      {
        Item *item= *tmp_order->item;
        if (item->is_expensive())
        {
          /* Force tmp table without sort */
          need_tmp=1; simple_order=simple_group=0;
          break;
        }
      }
    }
  }

  tmp_having= having;
  if (select_options & SELECT_DESCRIBE)
  {
    error= 0;
    return 0;
  }
  having= 0;

  /*
    The loose index scan access method guarantees that all grouping or
    duplicate row elimination (for distinct) is already performed
    during data retrieval, and that all MIN/MAX functions are already
    computed for each group. Thus all MIN/MAX functions should be
    treated as regular functions, and there is no need to perform
    grouping in the main execution loop.
    Notice that currently loose index scan is applicable only for
    single table queries, thus it is sufficient to test only the first
    join_tab element of the plan for its access method.
  */
  if (join_tab->is_using_loose_index_scan())
    tmp_table_param.precomputed_group_by= true;

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    session->set_proc_info("Creating tmp table");

    init_items_ref_array();

    tmp_table_param.hidden_field_count= all_fields.size() - fields_list.size();
    Order *tmp_group= (((not simple_group) or not (getDebug().test(debug::NO_KEY_GROUP))) ? group_list : NULL);

    /*
      Pushing LIMIT to the temporary table creation is not applicable
      when there is ORDER BY or GROUP BY or there is no GROUP BY, but
      there are aggregate functions, because in all these cases we need
      all result rows.
    */
    ha_rows tmp_rows_limit= ((order == 0 || skip_sort_order) &&
                             !tmp_group &&
                             !session->lex().current_select->with_sum_func) ?
                            select_limit : HA_POS_ERROR;

    if (!(exec_tmp_table1=
          create_tmp_table(session, &tmp_table_param, all_fields,
                           tmp_group,
                           group_list ? 0 : select_distinct,
                           group_list && simple_group,
                           select_options,
                           tmp_rows_limit,
                           (char *) "")))
    {
      return 1;
    }

    /*
      We don't have to store rows in temp table that doesn't match HAVING if:
      - we are sorting the table and writing complete group rows to the
        temp table.
      - We are using DISTINCT without resolving the distinct as a GROUP BY
        on all columns.

      If having is not handled here, it will be checked before the row
      is sent to the client.
    */
    if (tmp_having && (sort_and_group || (exec_tmp_table1->distinct && !group_list)))
      having= tmp_having;

    /* if group or order on first table, sort first */
    if (group_list && simple_group)
    {
      session->set_proc_info("Sorting for group");
      if (create_sort_index(session, this, group_list,
          HA_POS_ERROR, HA_POS_ERROR, false) ||
          alloc_group_fields(this, group_list) ||
          make_sum_func_list(all_fields, fields_list, 1) ||
          setup_sum_funcs(session, sum_funcs))
      {
        return 1;
      }
      group_list=0;
    }
    else
    {
      if (make_sum_func_list(all_fields, fields_list, 0) ||
          setup_sum_funcs(session, sum_funcs))
      {
        return 1;
      }

      if (!group_list && ! exec_tmp_table1->distinct && order && simple_order)
      {
        session->set_proc_info("Sorting for order");
        if (create_sort_index(session, this, order, HA_POS_ERROR, HA_POS_ERROR, true))
        {
          return 1;
        }
        order=0;
      }
    }

    /*
      Optimize distinct when used on some of the tables
      SELECT DISTINCT t1.a FROM t1,t2 WHERE t1.b=t2.b
      In this case we can stop scanning t2 when we have found one t1.a
    */

    if (exec_tmp_table1->distinct)
    {
      table_map used_tables= session->used_tables;
      JoinTable *last_join_tab= join_tab+tables-1;
      do
      {
        if (used_tables & last_join_tab->table->map)
          break;
        last_join_tab->not_used_in_distinct=1;
      } while (last_join_tab-- != join_tab);
      /* Optimize "select distinct b from t1 order by key_part_1 limit #" */
      if (order && skip_sort_order)
      {
        /* Should always succeed */
        if (test_if_skip_sort_order(&join_tab[const_tables],
                  order, unit->select_limit_cnt, 0,
                                          &join_tab[const_tables].table->
                                            keys_in_use_for_order_by))
          order= 0;
      }
    }

    /*
      If this join belongs to an uncacheable subquery save
      the original join
    */
    if (select_lex->uncacheable.any() && not is_top_level_join())
      init_save_join_tab();
  }

  error= 0;
  return 0;

setup_subq_exit:
  /* Even with zero matching rows, subqueries in the HAVING clause
     may need to be evaluated if there are aggregate functions in the query.
  */
  if (setup_subquery_materialization())
    return 1;
  error= 0;
  return 0;
}

/**
  Restore values in temporary join.
*/
void Join::restore_tmp()
{
  memcpy(tmp_join, this, (size_t) sizeof(Join));
}

int Join::reinit()
{
  unit->offset_limit_cnt= (ha_rows)(select_lex->offset_limit ?
                                    select_lex->offset_limit->val_uint() :
                                    0UL);

  first_record= 0;

  if (exec_tmp_table1)
  {
    exec_tmp_table1->cursor->extra(HA_EXTRA_RESET_STATE);
    exec_tmp_table1->cursor->ha_delete_all_rows();
    exec_tmp_table1->free_io_cache();
    exec_tmp_table1->filesort_free_buffers();
  }
  if (exec_tmp_table2)
  {
    exec_tmp_table2->cursor->extra(HA_EXTRA_RESET_STATE);
    exec_tmp_table2->cursor->ha_delete_all_rows();
    exec_tmp_table2->free_io_cache();
    exec_tmp_table2->filesort_free_buffers();
  }
  if (items0)
    set_items_ref_array(items0);

  if (join_tab_save)
    memcpy(join_tab, join_tab_save, sizeof(JoinTable) * tables);

  if (tmp_join)
    restore_tmp();

  /* Reset of sum functions */
  if (sum_funcs)
  {
    Item_sum *func, **func_ptr= sum_funcs;
    while ((func= *(func_ptr++)))
      func->clear();
  }

  return 0;
}

/**
   @brief Save the original join layout

   @details Saves the original join layout so it can be reused in
   re-execution and for EXPLAIN.

   @return Operation status
   @retval 0      success.
   @retval 1      error occurred.
*/
void Join::init_save_join_tab()
{
  tmp_join= (Join*)session->mem.alloc(sizeof(Join));

  error= 0;              // Ensure that tmp_join.error= 0
  restore_tmp();
}

void Join::save_join_tab()
{
  if (! join_tab_save && select_lex->master_unit()->uncacheable.any())
  {
    join_tab_save= (JoinTable*)session->mem.memdup(join_tab, sizeof(JoinTable) * tables);
  }
}

/**
  Exec select.

  @todo
    Note, that create_sort_index calls test_if_skip_sort_order and may
    finally replace sorting with index scan if there is a LIMIT clause in
    the query.  It's never shown in EXPLAIN!

  @todo
    When can we have here session->net.report_error not zero?
*/
void Join::exec()
{
  List<Item> *columns_list= &fields_list;
  int      tmp_error;

  session->set_proc_info("executing");
  error= 0;

  if (!tables_list && (tables || !select_lex->with_sum_func))
  {                                           
    /* Only test of functions */
    if (select_options & SELECT_DESCRIBE)
    {
      optimizer::ExplainPlan planner(this, 
                                     false,
                                     false,
                                     false,
                                     (zero_result_cause ? zero_result_cause : "No tables used"));
      planner.printPlan();
    }
    else
    {
      result->send_fields(*columns_list);
      /*
        We have to test for 'conds' here as the WHERE may not be constant
        even if we don't have any tables for prepared statements or if
        conds uses something like 'rand()'.
      */
      if (cond_value != Item::COND_FALSE &&
          (!conds || conds->val_int()) &&
          (!having || having->val_int()))
      {
        if (do_send_rows && result->send_data(fields_list))
          error= 1;
        else
        {
          error= (int) result->send_eof();
          send_records= ((select_options & OPTION_FOUND_ROWS) ? 1 : session->sent_row_count);
        }
      }
      else
      {
        error= (int) result->send_eof();
        send_records= 0;
      }
    }
    /* Single select (without union) always returns 0 or 1 row */
    session->limit_found_rows= send_records;
    session->examined_row_count= 0;
    return;
  }
  /*
    Don't reset the found rows count if there're no tables as
    FOUND_ROWS() may be called. Never reset the examined row count here.
    It must be accumulated from all join iterations of all join parts.
  */
  if (tables)
    session->limit_found_rows= 0;

  if (zero_result_cause)
  {
    return_zero_rows(this, result, select_lex->leaf_tables, *columns_list, send_row_on_empty_set(), select_options, zero_result_cause, having);
    return;
  }

  if (select_options & SELECT_DESCRIBE)
  {
    /*
      Check if we managed to optimize ORDER BY away and don't use temporary
      table to resolve order_st BY: in that case, we only may need to do
      filesort for GROUP BY.
    */
    if (!order && !no_order && (!skip_sort_order || !need_tmp))
    {
      /* Reset 'order' to 'group_list' and reinit variables describing 'order' */
      order= group_list;
      simple_order= simple_group;
      skip_sort_order= 0;
    }
    if (order && (order != group_list || !(select_options & SELECT_BIG_RESULT)))
    {
      if (const_tables == tables 
        || ((simple_order || skip_sort_order) 
          && test_if_skip_sort_order(&join_tab[const_tables], order, select_limit, 0, &join_tab[const_tables].table->keys_in_use_for_query)))
      order= 0;
    }
    having= tmp_having;
    optimizer::ExplainPlan planner(this,
                                   need_tmp,
                                   order != 0 && ! skip_sort_order,
                                   select_distinct,
                                   ! tables ? "No tables used" : NULL);
    planner.printPlan();
    return;
  }

  Join *curr_join= this;
  List<Item> *curr_all_fields= &all_fields;
  List<Item> *curr_fields_list= &fields_list;
  Table *curr_tmp_table= 0;
  /*
    Initialize examined rows here because the values from all join parts
    must be accumulated in examined_row_count. Hence every join
    iteration must count from zero.
  */
  curr_join->examined_rows= 0;

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    if (tmp_join)
    {
      /*
        We are in a non cacheable sub query. Get the saved join structure
        after optimization.
        (curr_join may have been modified during last exection and we need
        to reset it)
      */
      curr_join= tmp_join;
    }
    curr_tmp_table= exec_tmp_table1;

    /* Copy data to the temporary table */
    session->set_proc_info("Copying to tmp table");
    if (! curr_join->sort_and_group && curr_join->const_tables != curr_join->tables)
      curr_join->join_tab[curr_join->const_tables].sorted= 0;
    if ((tmp_error= do_select(curr_join, NULL, curr_tmp_table)))
    {
      error= tmp_error;
      return;
    }
    curr_tmp_table->cursor->info(HA_STATUS_VARIABLE);

    if (curr_join->having)
      curr_join->having= curr_join->tmp_having= 0; // Allready done

    /* Change sum_fields reference to calculated fields in tmp_table */
    curr_join->all_fields= *curr_all_fields;
    if (!items1)
    {
      items1= items0 + all_fields.size();
      if (sort_and_group || curr_tmp_table->group)
      {
        if (change_to_use_tmp_fields(session, items1,
                  tmp_fields_list1, tmp_all_fields1,
                  fields_list.size(), all_fields))
          return;
      }
      else
      {
        if (change_refs_to_tmp_fields(session, items1,
                    tmp_fields_list1, tmp_all_fields1,
                    fields_list.size(), all_fields))
          return;
      }
      curr_join->tmp_all_fields1= tmp_all_fields1;
      curr_join->tmp_fields_list1= tmp_fields_list1;
      curr_join->items1= items1;
    }
    curr_all_fields= &tmp_all_fields1;
    curr_fields_list= &tmp_fields_list1;
    curr_join->set_items_ref_array(items1);

    if (sort_and_group || curr_tmp_table->group)
    {
      curr_join->tmp_table_param.field_count+= curr_join->tmp_table_param.sum_func_count
                                             + curr_join->tmp_table_param.func_count;
      curr_join->tmp_table_param.sum_func_count= 0;
      curr_join->tmp_table_param.func_count= 0;
    }
    else
    {
      curr_join->tmp_table_param.field_count+= curr_join->tmp_table_param.func_count;
      curr_join->tmp_table_param.func_count= 0;
    }

    if (curr_tmp_table->group)
    {           // Already grouped
      if (!curr_join->order && !curr_join->no_order && !skip_sort_order)
        curr_join->order= curr_join->group_list;  /* order by group */
      curr_join->group_list= 0;
    }

    /*
      If we have different sort & group then we must sort the data by group
      and copy it to another tmp table
      This code is also used if we are using distinct something
      we haven't been able to store in the temporary table yet
      like SEC_TO_TIME(SUM(...)).
    */

    if ((curr_join->group_list && (!test_if_subpart(curr_join->group_list, curr_join->order) || curr_join->select_distinct)) 
        || (curr_join->select_distinct && curr_join->tmp_table_param.using_indirect_summary_function))
    {         /* Must copy to another table */
      /* Free first data from old join */
      curr_join->join_free();
      make_simple_join(curr_join, curr_tmp_table);
      calc_group_buffer(curr_join, group_list);
      count_field_types(select_lex, &curr_join->tmp_table_param,
      curr_join->tmp_all_fields1,
      curr_join->select_distinct && !curr_join->group_list);
      curr_join->tmp_table_param.hidden_field_count= curr_join->tmp_all_fields1.size()
                                                   - curr_join->tmp_fields_list1.size();

      if (exec_tmp_table2)
      {
        curr_tmp_table= exec_tmp_table2;
      }
      else
      {
        /* group data to new table */

        /*
          If the access method is loose index scan then all MIN/MAX
          functions are precomputed, and should be treated as regular
          functions. See extended comment in Join::exec.
        */
        if (curr_join->join_tab->is_using_loose_index_scan())
          curr_join->tmp_table_param.precomputed_group_by= true;

        if (!(curr_tmp_table=
              exec_tmp_table2= create_tmp_table(session,
                                                &curr_join->tmp_table_param,
                                                *curr_all_fields,
                                                NULL,
                                                curr_join->select_distinct &&
                                                !curr_join->group_list,
                                                1, curr_join->select_options,
                                                HA_POS_ERROR,
                                                (char *) "")))
        {
          return;
        }

        curr_join->exec_tmp_table2= exec_tmp_table2;
      }
      if (curr_join->group_list)
      {
        session->set_proc_info("Creating sort index");
        if (curr_join->join_tab == join_tab)
          save_join_tab();
        if (create_sort_index(session, curr_join, curr_join->group_list, HA_POS_ERROR, HA_POS_ERROR, false) ||
            make_group_fields(this, curr_join))
        {
          return;
        }
        sortorder= curr_join->sortorder;
      }

      session->set_proc_info("Copying to group table");
      tmp_error= -1;
      if (curr_join != this)
      {
        if (sum_funcs2)
        {
          curr_join->sum_funcs= sum_funcs2;
          curr_join->sum_funcs_end= sum_funcs_end2;
        }
        else
        {
          curr_join->alloc_func_list();
          sum_funcs2= curr_join->sum_funcs;
          sum_funcs_end2= curr_join->sum_funcs_end;
        }
      }
      if (curr_join->make_sum_func_list(*curr_all_fields, *curr_fields_list, 1, true))
        return;
      curr_join->group_list= 0;

      if (!curr_join->sort_and_group && (curr_join->const_tables != curr_join->tables))
        curr_join->join_tab[curr_join->const_tables].sorted= 0;
      
      if (setup_sum_funcs(curr_join->session, curr_join->sum_funcs) 
        || (tmp_error= do_select(curr_join, NULL, curr_tmp_table)))
      {
        error= tmp_error;
        return;
      }
      curr_join->join_tab->read_record.end_read_record();
      curr_join->const_tables= curr_join->tables; // Mark free for cleanup()
      curr_join->join_tab[0].table= 0;           // Table is freed

      // No sum funcs anymore
      if (!items2)
      {
        items2= items1 + all_fields.size();
        if (change_to_use_tmp_fields(session, items2,
                  tmp_fields_list2, tmp_all_fields2,
                  fields_list.size(), tmp_all_fields1))
          return;
        curr_join->tmp_fields_list2= tmp_fields_list2;
        curr_join->tmp_all_fields2= tmp_all_fields2;
      }
      curr_fields_list= &curr_join->tmp_fields_list2;
      curr_all_fields= &curr_join->tmp_all_fields2;
      curr_join->set_items_ref_array(items2);
      curr_join->tmp_table_param.field_count+= curr_join->tmp_table_param.sum_func_count;
      curr_join->tmp_table_param.sum_func_count= 0;
    }
    if (curr_tmp_table->distinct)
      curr_join->select_distinct=0;   /* Each row is unique */

    curr_join->join_free();     /* Free quick selects */
    if (curr_join->select_distinct && ! curr_join->group_list)
    {
      session->set_proc_info("Removing duplicates");
      if (curr_join->tmp_having)
        curr_join->tmp_having->update_used_tables();

      if (remove_duplicates(curr_join, curr_tmp_table,
          *curr_fields_list, curr_join->tmp_having))
        return;
      
      curr_join->tmp_having=0;
      curr_join->select_distinct=0;
    }
    curr_tmp_table->reginfo.lock_type= TL_UNLOCK;
    make_simple_join(curr_join, curr_tmp_table);
    calc_group_buffer(curr_join, curr_join->group_list);
    count_field_types(select_lex, &curr_join->tmp_table_param, *curr_all_fields, 0);

  }

  if (curr_join->group || curr_join->tmp_table_param.sum_func_count)
  {
    if (make_group_fields(this, curr_join))
      return;

    if (! items3)
    {
      if (! items0)
        init_items_ref_array();
      items3= ref_pointer_array + (all_fields.size() * 4);
      setup_copy_fields(session, &curr_join->tmp_table_param,
      items3, tmp_fields_list3, tmp_all_fields3,
      curr_fields_list->size(), *curr_all_fields);
      tmp_table_param.save_copy_funcs= curr_join->tmp_table_param.copy_funcs;
      tmp_table_param.save_copy_field= curr_join->tmp_table_param.copy_field;
      tmp_table_param.save_copy_field_end= curr_join->tmp_table_param.copy_field_end;
      curr_join->tmp_all_fields3= tmp_all_fields3;
      curr_join->tmp_fields_list3= tmp_fields_list3;
    }
    else
    {
      curr_join->tmp_table_param.copy_funcs= tmp_table_param.save_copy_funcs;
      curr_join->tmp_table_param.copy_field= tmp_table_param.save_copy_field;
      curr_join->tmp_table_param.copy_field_end= tmp_table_param.save_copy_field_end;
    }
    curr_fields_list= &tmp_fields_list3;
    curr_all_fields= &tmp_all_fields3;
    curr_join->set_items_ref_array(items3);

    if (curr_join->make_sum_func_list(*curr_all_fields, *curr_fields_list,
              1, true) ||
        setup_sum_funcs(curr_join->session, curr_join->sum_funcs) ||
        session->is_fatal_error)
      return;
  }
  if (curr_join->group_list || curr_join->order)
  {
    session->set_proc_info("Sorting result");
    /* If we have already done the group, add HAVING to sorted table */
    if (curr_join->tmp_having && ! curr_join->group_list && ! curr_join->sort_and_group)
    {
      // Some tables may have been const
      curr_join->tmp_having->update_used_tables();
      JoinTable *curr_table= &curr_join->join_tab[curr_join->const_tables];
      table_map used_tables= (curr_join->const_table_map |
            curr_table->table->map);

      Item* sort_table_cond= make_cond_for_table(curr_join->tmp_having, used_tables, used_tables, 0);
      if (sort_table_cond)
      {
        if (!curr_table->select)
          curr_table->select= new optimizer::SqlSelect;
        if (!curr_table->select->cond)
          curr_table->select->cond= sort_table_cond;
        else          // This should never happen
        {
          curr_table->select->cond= new Item_cond_and(curr_table->select->cond, sort_table_cond);
          /*
            Item_cond_and do not need fix_fields for execution, its parameters
            are fixed or do not need fix_fields, too
          */
          curr_table->select->cond->quick_fix_field();
        }
        curr_table->select_cond= curr_table->select->cond;
        curr_table->select_cond->top_level_item();
        curr_join->tmp_having= make_cond_for_table(curr_join->tmp_having, ~ (table_map) 0, ~used_tables, 0);
      }
    }
    {
      if (group)
        curr_join->select_limit= HA_POS_ERROR;
      else
      {
        /*
          We can abort sorting after session->select_limit rows if we there is no
          WHERE clause for any tables after the sorted one.
        */
        JoinTable *curr_table= &curr_join->join_tab[curr_join->const_tables+1];
        JoinTable *end_table= &curr_join->join_tab[curr_join->tables];
        for (; curr_table < end_table ; curr_table++)
        {
          /*
            table->keyuse is set in the case there was an original WHERE clause
            on the table that was optimized away.
          */
          if (curr_table->select_cond ||
              (curr_table->keyuse && !curr_table->first_inner))
          {
            /* We have to sort all rows */
            curr_join->select_limit= HA_POS_ERROR;
            break;
          }
        }
      }
      if (curr_join->join_tab == join_tab)
        save_join_tab();
      /*
        Here we sort rows for order_st BY/GROUP BY clause, if the optimiser
        chose FILESORT to be faster than INDEX SCAN or there is no
        suitable index present.
        Note, that create_sort_index calls test_if_skip_sort_order and may
        finally replace sorting with index scan if there is a LIMIT clause in
        the query. XXX: it's never shown in EXPLAIN!
        OPTION_FOUND_ROWS supersedes LIMIT and is taken into account.
      */
      if (create_sort_index(session, curr_join,
          curr_join->group_list ?
          curr_join->group_list : curr_join->order,
          curr_join->select_limit,
          (select_options & OPTION_FOUND_ROWS ?
           HA_POS_ERROR : unit->select_limit_cnt),
                            curr_join->group_list ? true : false))
        return;

      sortorder= curr_join->sortorder;
      if (curr_join->const_tables != curr_join->tables &&
          !curr_join->join_tab[curr_join->const_tables].table->sort.io_cache)
      {
        /*
          If no IO cache exists for the first table then we are using an
          INDEX SCAN and no filesort. Thus we should not remove the sorted
          attribute on the INDEX SCAN.
        */
        skip_sort_order= 1;
      }
    }
  }
  /* XXX: When can we have here session->is_error() not zero? */
  if (session->is_error())
  {
    error= session->is_error();
    return;
  }
  curr_join->having= curr_join->tmp_having;
  curr_join->fields= curr_fields_list;

  session->set_proc_info("Sending data");
  result->send_fields(*curr_fields_list);
  error= do_select(curr_join, curr_fields_list, NULL);
  session->limit_found_rows= curr_join->send_records;

  /* Accumulate the counts from all join iterations of all join parts. */
  session->examined_row_count+= curr_join->examined_rows;

  /*
    With EXPLAIN EXTENDED we have to restore original ref_array
    for a derived table which is always materialized.
    Otherwise we would not be able to print the query  correctly.
  */
  if (items0 && (session->lex().describe & DESCRIBE_EXTENDED) && select_lex->linkage == DERIVED_TABLE_TYPE)
    set_items_ref_array(items0);

  return;
}

/**
  Clean up join.

  @return
    Return error that hold Join.
*/
int Join::destroy()
{
  select_lex->join= 0;

  if (tmp_join)
  {
    if (join_tab != tmp_join->join_tab)
    {
      JoinTable *tab, *end;
      for (tab= join_tab, end= tab+tables ; tab != end ; tab++)
        tab->cleanup();
    }
    tmp_join->tmp_join= 0;
    tmp_table_param.copy_field=0;
    return(tmp_join->destroy());
  }
  cond_equal= 0;

  cleanup(1);
  exec_tmp_table1= NULL;
  exec_tmp_table2= NULL;
  delete select;
  delete_dynamic(&keyuse);

  return(error);
}

/**
  Setup for execution all subqueries of a query, for which the optimizer
  chose hash semi-join.

  @details Iterate over all subqueries of the query, and if they are under an
  IN predicate, and the optimizer chose to compute it via hash semi-join:
  - try to initialize all data structures needed for the materialized execution
    of the IN predicate,
  - if this fails, then perform the IN=>EXISTS transformation which was
    previously blocked during Join::prepare.

  This method is part of the "code generation" query processing phase.

  This phase must be called after substitute_for_best_equal_field() because
  that function may replace items with other items from a multiple equality,
  and we need to reference the correct items in the index access method of the
  IN predicate.

  @return Operation status
  @retval false     success.
  @retval true      error occurred.
*/
bool Join::setup_subquery_materialization()
{
  for (Select_Lex_Unit *un= select_lex->first_inner_unit(); un;
       un= un->next_unit())
  {
    for (Select_Lex *sl= un->first_select(); sl; sl= sl->next_select())
    {
      Item_subselect *subquery_predicate= sl->master_unit()->item;
      if (subquery_predicate &&
          subquery_predicate->substype() == Item_subselect::IN_SUBS)
      {
        Item_in_subselect *in_subs= (Item_in_subselect*) subquery_predicate;
        if (in_subs->exec_method == Item_in_subselect::MATERIALIZATION &&
            in_subs->setup_engine())
          return true;
      }
    }
  }
  return false;
}

/**
  Partially cleanup Join after it has executed: close index or rnd read
  (table cursors), free quick selects.

    This function is called in the end of execution of a Join, before the used
    tables are unlocked and closed.

    For a join that is resolved using a temporary table, the first sweep is
    performed against actual tables and an intermediate result is inserted
    into the temprorary table.
    The last sweep is performed against the temporary table. Therefore,
    the base tables and associated buffers used to fill the temporary table
    are no longer needed, and this function is called to free them.

    For a join that is performed without a temporary table, this function
    is called after all rows are sent, but before EOF packet is sent.

    For a simple SELECT with no subqueries this function performs a full
    cleanup of the Join and calls unlockReadTables to free used base
    tables.

    If a Join is executed for a subquery or if it has a subquery, we can't
    do the full cleanup and need to do a partial cleanup only.
    - If a Join is not the top level join, we must not unlock the tables
    because the outer select may not have been evaluated yet, and we
    can't unlock only selected tables of a query.
    - Additionally, if this Join corresponds to a correlated subquery, we
    should not free quick selects and join buffers because they will be
    needed for the next execution of the correlated subquery.
    - However, if this is a Join for a [sub]select, which is not
    a correlated subquery itself, but has subqueries, we can free it
    fully and also free Joins of all its subqueries. The exception
    is a subquery in SELECT list, e.g: @n
    SELECT a, (select cmax(b) from t1) group by c @n
    This subquery will not be evaluated at first sweep and its value will
    not be inserted into the temporary table. Instead, it's evaluated
    when selecting from the temporary table. Therefore, it can't be freed
    here even though it's not correlated.

  @todo
    Unlock tables even if the join isn't top level select in the tree
*/
void Join::join_free()
{
  Select_Lex_Unit *tmp_unit;
  Select_Lex *sl;
  /*
    Optimization: if not EXPLAIN and we are done with the Join,
    free all tables.
  */
  bool full= (select_lex->uncacheable.none() && ! session->lex().describe);
  bool can_unlock= full;

  cleanup(full);

  for (tmp_unit= select_lex->first_inner_unit();
       tmp_unit;
       tmp_unit= tmp_unit->next_unit())
    for (sl= tmp_unit->first_select(); sl; sl= sl->next_select())
    {
      Item_subselect *subselect= sl->master_unit()->item;
      bool full_local= full && (!subselect || 
                                (subselect->is_evaluated() &&
                                !subselect->is_uncacheable()));
      /*
        If this join is evaluated, we can fully clean it up and clean up all
        its underlying joins even if they are correlated -- they will not be
        used any more anyway.
        If this join is not yet evaluated, we still must clean it up to
        close its table cursors -- it may never get evaluated, as in case of
        ... HAVING false OR a IN (SELECT ...))
        but all table cursors must be closed before the unlock.
      */
      sl->cleanup_all_joins(full_local);
      /* Can't unlock if at least one Join is still needed */
      can_unlock= can_unlock && full_local;
    }

  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (can_unlock && lock && session->open_tables.lock &&
      !(select_options & SELECT_NO_UNLOCK) &&
      !select_lex->subquery_in_having &&
      (select_lex == (session->lex().unit.fake_select_lex ?
                      session->lex().unit.fake_select_lex : &session->lex().select_lex)))
  {
    /*
      TODO: unlock tables even if the join isn't top level select in the
      tree.
    */
    session->unlockReadTables(lock);           // Don't free join->lock
    lock= 0;
  }

  return;
}


/**
  Free resources of given join.

  @param fill   true if we should free all resources, call with full==1
                should be last, before it this function can be called with
                full==0

  @note
    With subquery this function definitely will be called several times,
    but even for simple query it can be called several times.
*/
void Join::cleanup(bool full)
{
  if (table)
  {
    /*
      Only a sorted table may be cached.  This sorted table is always the
      first non const table in join->table
    */
    if (tables > const_tables) // Test for not-const tables
    {
      table[const_tables]->free_io_cache();
      table[const_tables]->filesort_free_buffers(full);
    }
  }

  if (join_tab)
  {
    JoinTable *tab,*end;

    if (full)
    {
      for (tab= join_tab, end= tab+tables; tab != end; tab++)
        tab->cleanup();
      table= 0;
    }
    else
    {
      for (tab= join_tab, end= tab+tables; tab != end; tab++)
      {
        if (tab->table)
          tab->table->cursor->ha_index_or_rnd_end();
      }
    }
  }

  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (full)
  {
    if (tmp_join)
      tmp_table_param.copy_field= 0;
    group_fields.delete_elements();
    /*
      We can't call delete_elements() on copy_funcs as this will cause
      problems in free_elements() as some of the elements are then deleted.
    */
    tmp_table_param.copy_funcs.clear();
    /*
      If we have tmp_join and 'this' Join is not tmp_join and
      tmp_table_param.copy_field's  of them are equal then we have to remove
      pointer to  tmp_table_param.copy_field from tmp_join, because it qill
      be removed in tmp_table_param.cleanup().
    */
    if (tmp_join &&
        tmp_join != this &&
        tmp_join->tmp_table_param.copy_field ==
        tmp_table_param.copy_field)
    {
      tmp_join->tmp_table_param.copy_field=
        tmp_join->tmp_table_param.save_copy_field= 0;
    }
    tmp_table_param.cleanup();
  }
  return;
}

/*
  used only in Join::clear
*/
static void clear_tables(Join *join)
{
  /*
    must clear only the non-const tables, as const tables
    are not re-calculated.
  */
  for (uint32_t i= join->const_tables; i < join->tables; i++)
  {
    join->table[i]->mark_as_null_row();   // All fields are NULL
  }
}

/**
  Make an array of pointers to sum_functions to speed up
  sum_func calculation.

  @retval
    0 ok
  @retval
    1 Error
*/
bool Join::alloc_func_list()
{
  uint32_t func_count, group_parts;

  func_count= tmp_table_param.sum_func_count;
  /*
    If we are using rollup, we need a copy of the summary functions for
    each level
  */
  if (rollup.getState() != Rollup::STATE_NONE)
    func_count*= (send_group_parts+1);

  group_parts= send_group_parts;
  /*
    If distinct, reserve memory for possible
    disctinct->group_by optimization
  */
  if (select_distinct)
  {
    group_parts+= fields_list.size();
    /*
      If the order_st clause is specified then it's possible that
      it also will be optimized, so reserve space for it too
    */
    if (order)
    {
      Order *ord;
      for (ord= order; ord; ord= ord->next)
        group_parts++;
    }
  }

  /* This must use calloc() as rollup_make_fields depends on this */
  sum_funcs= (Item_sum**) session->mem.calloc(sizeof(Item_sum**) * (func_count+1) +
              sizeof(Item_sum***) * (group_parts+1));
  sum_funcs_end= (Item_sum***) (sum_funcs+func_count+1);
  return(sum_funcs == 0);
}

/**
  Initialize 'sum_funcs' array with all Item_sum objects.

  @param field_list        All items
  @param send_fields       Items in select list
  @param before_group_by   Set to 1 if this is called before GROUP BY handling
  @param recompute         Set to true if sum_funcs must be recomputed

  @retval
    0  ok
  @retval
    1  error
*/
bool Join::make_sum_func_list(List<Item> &field_list, 
                              List<Item> &send_fields,
                              bool before_group_by, 
                              bool recompute)
{
  List<Item>::iterator it(field_list.begin());
  Item_sum **func;
  Item *item;

  if (*sum_funcs && !recompute)
    return false; /* We have already initialized sum_funcs. */

  func= sum_funcs;
  while ((item=it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
        (!((Item_sum*) item)->depended_from() ||
         ((Item_sum *)item)->depended_from() == select_lex))
      *func++= (Item_sum*) item;
  }
  if (before_group_by && rollup.getState() == Rollup::STATE_INITED)
  {
    rollup.setState(Rollup::STATE_READY);
    if (rollup_make_fields(field_list, send_fields, &func))
      return true;     // Should never happen
  }
  else if (rollup.getState() == Rollup::STATE_NONE)
  {
    for (uint32_t i=0 ; i <= send_group_parts ;i++)
      sum_funcs_end[i]= func;
  }
  else if (rollup.getState() == Rollup::STATE_READY)
    return false;                         // Don't put end marker
  *func=0;          // End marker
  return false;
}

/** Allocate memory needed for other rollup functions. */
bool Join::rollup_init()
{

  tmp_table_param.quick_group= 0; // Can't create groups in tmp table
  rollup.setState(Rollup::STATE_INITED);

  /*
    Create pointers to the different sum function groups
    These are updated by rollup_make_fields()
  */
  tmp_table_param.group_parts= send_group_parts;

  rollup.setNullItems((Item_null_result**) session->mem.alloc((sizeof(Item*) + sizeof(Item**) + sizeof(List<Item>) + ref_pointer_array_size) * send_group_parts));
  rollup.setFields((List<Item>*) (rollup.getNullItems() + send_group_parts));
  rollup.setRefPointerArrays((Item***) (rollup.getFields() + send_group_parts));
  Item** ref_array= (Item**) (rollup.getRefPointerArrays()+send_group_parts);

  /*
    Prepare space for field list for the different levels
    These will be filled up in rollup_make_fields()
  */
  for (uint32_t i= 0 ; i < send_group_parts ; i++)
  {
    rollup.getNullItems()[i]= new (session->mem) Item_null_result();
    List<Item> *rollup_fields= &rollup.getFields()[i];
    rollup_fields->clear();
    rollup.getRefPointerArrays()[i]= ref_array;
    ref_array+= all_fields.size();
  }

  for (uint32_t i= 0 ; i < send_group_parts; i++)
  {
    for (uint32_t j= 0 ; j < fields_list.size() ; j++)
    {
      rollup.getFields()[i].push_back(rollup.getNullItems()[i]);
    }
  }

  List<Item>::iterator it(all_fields.begin());
  while (Item* item= it++)
  {
    Order *group_tmp;
    bool found_in_group= 0;

    for (group_tmp= group_list; group_tmp; group_tmp= group_tmp->next)
    {
      if (*group_tmp->item == item)
      {
        item->maybe_null= 1;
        found_in_group= 1;
        if (item->const_item())
        {
          /*
            For ROLLUP queries each constant item referenced in GROUP BY list
            is wrapped up into an Item_func object yielding the same value
            as the constant item. The objects of the wrapper class are never
            considered as constant items and besides they inherit all
            properties of the Item_result_field class.
            This wrapping allows us to ensure writing constant items
            into temporary tables whenever the result of the ROLLUP
            operation has to be written into a temporary table, e.g. when
            ROLLUP is used together with DISTINCT in the SELECT list.
            Usually when creating temporary tables for a intermidiate
            result we do not include fields for constant expressions.
          */
          Item* new_item= new Item_func_rollup_const(item);
          new_item->fix_fields(session, NULL);
          *it.ref()= new_item;
          for (Order *tmp= group_tmp; tmp; tmp= tmp->next)
          {
            if (*tmp->item == item)
              *tmp->item= new_item;
          }
        }
      }
    }
    if (item->type() == Item::FUNC_ITEM && !found_in_group)
    {
      bool changed= false;
      if (change_group_ref(session, (Item_func *) item, group_list, &changed))
        return 1;
      /*
        We have to prevent creation of a field in a temporary table for
        an expression that contains GROUP BY attributes.
        Marking the expression item as 'with_sum_func' will ensure this.
      */
      if (changed)
        item->with_sum_func= 1;
    }
  }
  return 0;
}

/**
  Fill up rollup structures with pointers to fields to use.

  Creates copies of item_sum items for each sum level.

  @param fields_arg   List of all fields (hidden and real ones)
  @param sel_fields   Pointer to selected fields
  @param func     Store here a pointer to all fields

  @retval
    0 if ok;
    In this case func is pointing to next not used element.
  @retval
    1    on error
*/
bool Join::rollup_make_fields(List<Item> &fields_arg, List<Item> &sel_fields, Item_sum ***func)
{
  List<Item>::iterator it(fields_arg.begin());
  Item *first_field= &sel_fields.front();
  uint32_t level;

  /*
    Create field lists for the different levels

    The idea here is to have a separate field list for each rollup level to
    avoid all runtime checks of which columns should be NULL.

    The list is stored in reverse order to get sum function in such an order
    in func that it makes it easy to reset them with init_sum_functions()

    Assuming:  SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP

    rollup.fields[0] will contain list where a,b,c is NULL
    rollup.fields[1] will contain list where b,c is NULL
    ...
    rollup.ref_pointer_array[#] points to fields for rollup.fields[#]
    ...
    sum_funcs_end[0] points to all sum functions
    sum_funcs_end[1] points to all sum functions, except grand totals
    ...
  */

  for (level=0 ; level < send_group_parts ; level++)
  {
    uint32_t i;
    uint32_t pos= send_group_parts - level -1;
    bool real_fields= 0;
    Item *item;
    List<Item>::iterator new_it(rollup.getFields()[pos].begin());
    Item **ref_array_start= rollup.getRefPointerArrays()[pos];
    Order *start_group;

    /* Point to first hidden field */
    Item **ref_array= ref_array_start + fields_arg.size()-1;

    /* Remember where the sum functions ends for the previous level */
    sum_funcs_end[pos+1]= *func;

    /* Find the start of the group for this level */
    for (i= 0, start_group= group_list ;i++ < pos ;start_group= start_group->next)
    {}

    it= fields_arg.begin();
    while ((item= it++))
    {
      if (item == first_field)
      {
        real_fields= 1;       // End of hidden fields
        ref_array= ref_array_start;
      }

      if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
          (!((Item_sum*) item)->depended_from() ||
           ((Item_sum *)item)->depended_from() == select_lex))

      {
        /*
          This is a top level summary function that must be replaced with
          a sum function that is reset for this level.

          NOTE: This code creates an object which is not that nice in a
          sub select.  Fortunately it's not common to have rollup in
          sub selects.
        */
        item= item->copy_or_same(session);
        ((Item_sum*) item)->make_unique();
        *(*func)= (Item_sum*) item;
        (*func)++;
      }
      else
      {
        /* Check if this is something that is part of this group by */
        Order *group_tmp;
        for (group_tmp= start_group, i= pos ;
                  group_tmp ; group_tmp= group_tmp->next, i++)
        {
                if (*group_tmp->item == item)
          {
            /*
              This is an element that is used by the GROUP BY and should be
              set to NULL in this level
            */
                  Item_null_result *null_item= new (session->mem) Item_null_result();
            item->maybe_null= 1;    // Value will be null sometimes
                  null_item->result_field= item->get_tmp_table_field();
                  item= null_item;
            break;
          }
        }
      }
      *ref_array= item;
      if (real_fields)
      {
  (void) new_it++;      // Point to next item
  new_it.replace(item);     // Replace previous
  ref_array++;
      }
      else
  ref_array--;
    }
  }
  sum_funcs_end[0]= *func;      // Point to last function
  return 0;
}

/**
  Send all rollup levels higher than the current one to the client.

  @b SAMPLE
    @code
      SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx    Level we are on:
                        - 0 = Total sum level
                        - 1 = First group changed  (a)
                        - 2 = Second group changed (a,b)

  @retval
    0   ok
  @retval
    1   If send_data_failed()
*/
int Join::rollup_send_data(uint32_t idx)
{
  for (uint32_t i= send_group_parts ; i-- > idx ; )
  {
    /* Get reference pointers to sum functions in place */
    memcpy(ref_pointer_array, rollup.getRefPointerArrays()[i], ref_pointer_array_size);

    if ((!having || having->val_int()))
    {
      if (send_records < unit->select_limit_cnt && do_send_rows && result->send_data(rollup.getFields()[i]))
      {
        return 1;
      }
      send_records++;
    }
  }
  /* Restore ref_pointer_array */
  set_items_ref_array(current_ref_pointer_array);

  return 0;
}

/**
  Write all rollup levels higher than the current one to a temp table.

  @b SAMPLE
    @code
      SELECT a, b, SUM(c) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx                 Level we are on:
                               - 0 = Total sum level
                               - 1 = First group changed  (a)
                               - 2 = Second group changed (a,b)
  @param table               reference to temp table

  @retval
    0   ok
  @retval
    1   if write_data_failed()
*/
int Join::rollup_write_data(uint32_t idx, Table *table_arg)
{
  for (uint32_t i= send_group_parts ; i-- > idx ; )
  {
    /* Get reference pointers to sum functions in place */
    memcpy(ref_pointer_array, rollup.getRefPointerArrays()[i],
           ref_pointer_array_size);
    if ((!having || having->val_int()))
    {
      int write_error;
      Item *item;
      List<Item>::iterator it(rollup.getFields()[i].begin());
      while ((item= it++))
      {
        if (item->type() == Item::NULL_ITEM && item->is_result_field())
          item->save_in_result_field(1);
      }
      copy_sum_funcs(sum_funcs_end[i+1], sum_funcs_end[i]);
      if ((write_error= table_arg->cursor->insertRecord(table_arg->getInsertRecord())))
      {
        my_error(ER_USE_SQL_BIG_RESULT, MYF(0));
        return 1;
      }
    }
  }
  /* Restore ref_pointer_array */
  set_items_ref_array(current_ref_pointer_array);

  return 0;
}

/**
  clear results if there are not rows found for group
  (end_send_group/end_write_group)
*/
void Join::clear()
{
  clear_tables(this);
  copy_fields(&tmp_table_param);

  if (sum_funcs)
  {
    Item_sum *func, **func_ptr= sum_funcs;
    while ((func= *(func_ptr++)))
      func->clear();
  }
}

/**
  change select_result object of Join.

  @param res    new select_result object

  @retval
    false   OK
  @retval
    true    error
*/
bool Join::change_result(select_result *res)
{
  result= res;
  if (result->prepare(fields_list, select_lex->master_unit()))
  {
    return true;
  }
  return false;
}

/**
  Cache constant expressions in WHERE, HAVING, ON conditions.
*/

void Join::cache_const_exprs()
{
  bool cache_flag= false;
  bool *analyzer_arg= &cache_flag;

  /* No need in cache if all tables are constant. */
  if (const_tables == tables)
    return;

  if (conds)
    conds->compile(&Item::cache_const_expr_analyzer, (unsigned char **)&analyzer_arg,
                  &Item::cache_const_expr_transformer, (unsigned char *)&cache_flag);
  cache_flag= false;
  if (having)
    having->compile(&Item::cache_const_expr_analyzer, (unsigned char **)&analyzer_arg,
                    &Item::cache_const_expr_transformer, (unsigned char *)&cache_flag);

  for (JoinTable *tab= join_tab + const_tables; tab < join_tab + tables ; tab++)
  {
    if (*tab->on_expr_ref)
    {
      cache_flag= false;
      (*tab->on_expr_ref)->compile(&Item::cache_const_expr_analyzer,
                                 (unsigned char **)&analyzer_arg,
                                 &Item::cache_const_expr_transformer,
                                 (unsigned char *)&cache_flag);
    }
  }
}

/**
  @brief
  
  Process one record of the nested loop join.

  @details 

  This function will evaluate parts of WHERE/ON clauses that are
  applicable to the partial record on hand and in case of success
  submit this record to the next level of the nested loop.
*/
enum_nested_loop_state evaluate_join_record(Join *join, JoinTable *join_tab, int error)
{
  bool not_used_in_distinct= join_tab->not_used_in_distinct;
  ha_rows found_records= join->found_records;
  COND *select_cond= join_tab->select_cond;

  if (error > 0 || (join->session->is_error()))     // Fatal error
    return NESTED_LOOP_ERROR;
  if (error < 0)
    return NESTED_LOOP_NO_MORE_ROWS;
  if (join->session->getKilled())			// Aborted by user
  {
    join->session->send_kill_message();
    return NESTED_LOOP_KILLED;
  }
  if (!select_cond || select_cond->val_int())
  {
    /*
      There is no select condition or the attached pushed down
      condition is true => a match is found.
    */
    bool found= 1;
    while (join_tab->first_unmatched && found)
    {
      /*
        The while condition is always false if join_tab is not
        the last inner join table of an outer join operation.
      */
      JoinTable *first_unmatched= join_tab->first_unmatched;
      /*
        Mark that a match for current outer table is found.
        This activates push down conditional predicates attached
        to the all inner tables of the outer join.
      */
      first_unmatched->found= 1;
      for (JoinTable *tab= first_unmatched; tab <= join_tab; tab++)
      {
        if (tab->table->reginfo.not_exists_optimize)
          return NESTED_LOOP_NO_MORE_ROWS;
        /* Check all predicates that has just been activated. */
        /*
          Actually all predicates non-guarded by first_unmatched->found
          will be re-evaluated again. It could be fixed, but, probably,
          it's not worth doing now.
        */
        if (tab->select_cond && !tab->select_cond->val_int())
        {
          /* The condition attached to table tab is false */
          if (tab == join_tab)
            found= 0;
          else
          {
            /*
              Set a return point if rejected predicate is attached
              not to the last table of the current nest level.
            */
            join->return_tab= tab;
            return NESTED_LOOP_OK;
          }
        }
      }
      /*
        Check whether join_tab is not the last inner table
        for another embedding outer join.
      */
      if ((first_unmatched= first_unmatched->first_upper) &&
          first_unmatched->last_inner != join_tab)
        first_unmatched= 0;
      join_tab->first_unmatched= first_unmatched;
    }

    JoinTable *return_tab= join->return_tab;
    join_tab->found_match= true;

    /*
      It was not just a return to lower loop level when one
      of the newly activated predicates is evaluated as false
      (See above join->return_tab= tab).
    */
    join->examined_rows++;
    join->session->row_count++;

    if (found)
    {
      enum enum_nested_loop_state rc;
      /* A match from join_tab is found for the current partial join. */
      rc= (*join_tab->next_select)(join, join_tab+1, 0);
      if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
        return rc;
      if (return_tab < join->return_tab)
        join->return_tab= return_tab;

      if (join->return_tab < join_tab)
        return NESTED_LOOP_OK;
      /*
        Test if this was a SELECT DISTINCT query on a table that
        was not in the field list;  In this case we can abort if
        we found a row, as no new rows can be added to the result.
      */
      if (not_used_in_distinct && found_records != join->found_records)
        return NESTED_LOOP_NO_MORE_ROWS;
    }
    else
      join_tab->read_record.cursor->unlock_row();
  }
  else
  {
    /*
      The condition pushed down to the table join_tab rejects all rows
      with the beginning coinciding with the current partial join.
    */
    join->examined_rows++;
    join->session->row_count++;
    join_tab->read_record.cursor->unlock_row();
  }
  return NESTED_LOOP_OK;
}

/**
  @details
    Construct a NULL complimented partial join record and feed it to the next
    level of the nested loop. This function is used in case we have
    an OUTER join and no matching record was found.
*/
enum_nested_loop_state evaluate_null_complemented_join_record(Join *join, JoinTable *join_tab)
{
  /*
    The table join_tab is the first inner table of a outer join operation
    and no matches has been found for the current outer row.
  */
  JoinTable *last_inner_tab= join_tab->last_inner;
  /* Cache variables for faster loop */
  COND *select_cond;
  for ( ; join_tab <= last_inner_tab ; join_tab++)
  {
    /* Change the the values of guard predicate variables. */
    join_tab->found= 1;
    join_tab->not_null_compl= 0;
    /* The outer row is complemented by nulls for each inner tables */
    join_tab->table->restoreRecordAsDefault();  // Make empty record
    join_tab->table->mark_as_null_row();       // For group by without error
    select_cond= join_tab->select_cond;
    /* Check all attached conditions for inner table rows. */
    if (select_cond && !select_cond->val_int())
      return NESTED_LOOP_OK;
  }
  join_tab--;
  /*
    The row complemented by nulls might be the first row
    of embedding outer joins.
    If so, perform the same actions as in the code
    for the first regular outer join row above.
  */
  for ( ; ; )
  {
    JoinTable *first_unmatched= join_tab->first_unmatched;
    if ((first_unmatched= first_unmatched->first_upper) && first_unmatched->last_inner != join_tab)
      first_unmatched= 0;
    join_tab->first_unmatched= first_unmatched;
    if (! first_unmatched)
      break;
    first_unmatched->found= 1;
    for (JoinTable *tab= first_unmatched; tab <= join_tab; tab++)
    {
      if (tab->select_cond && !tab->select_cond->val_int())
      {
        join->return_tab= tab;
        return NESTED_LOOP_OK;
      }
    }
  }
  /*
    The row complemented by nulls satisfies all conditions
    attached to inner tables.
    Send the row complemented by nulls to be joined with the
    remaining tables.
  */
  return (*join_tab->next_select)(join, join_tab+1, 0);
}

enum_nested_loop_state flush_cached_records(Join *join, JoinTable *join_tab, bool skip_last)
{
  enum_nested_loop_state rc= NESTED_LOOP_OK;
  int error;
  ReadRecord *info;

  join_tab->table->null_row= 0;
  if (!join_tab->cache.records)
  {
    return NESTED_LOOP_OK;                      /* Nothing to do */
  }

  if (skip_last)
  {
    (void) join_tab->cache.store_record_in_cache(); // Must save this for later
  }


  if (join_tab->use_quick == 2)
  {
    delete join_tab->select->quick;
    join_tab->select->quick= 0;
  }
  /* read through all records */
  if ((error=join_init_read_record(join_tab)))
  {
    join_tab->cache.reset_cache_write();
    return error < 0 ? NESTED_LOOP_NO_MORE_ROWS: NESTED_LOOP_ERROR;
  }

  for (JoinTable *tmp=join->join_tab; tmp != join_tab ; tmp++)
  {
    tmp->status=tmp->table->status;
    tmp->table->status=0;
  }

  info= &join_tab->read_record;
  do
  {
    if (join->session->getKilled())
    {
      join->session->send_kill_message();
      return NESTED_LOOP_KILLED;
    }
    optimizer::SqlSelect *select= join_tab->select;
    if (rc == NESTED_LOOP_OK &&
        (!join_tab->cache.select || !join_tab->cache.select->skip_record()))
    {
      uint32_t i;
      join_tab->cache.reset_cache_read();
      for (i=(join_tab->cache.records- (skip_last ? 1 : 0)) ; i-- > 0 ;)
      {
	      join_tab->readCachedRecord();
	      if (!select || !select->skip_record())
        {
          int res= 0;

          rc= (join_tab->next_select)(join,join_tab+1,0);
          if (rc != NESTED_LOOP_OK && rc != NESTED_LOOP_NO_MORE_ROWS)
          {
            join_tab->cache.reset_cache_write();
            return rc;
          }

          if (res == -1)
            return NESTED_LOOP_ERROR;
        }
      }
    }
  } while (!(error=info->read_record(info)));

  if (skip_last)
    join_tab->readCachedRecord();		// Restore current record
  join_tab->cache.reset_cache_write();
  if (error > 0)				// Fatal error
    return NESTED_LOOP_ERROR;
  for (JoinTable *tmp2=join->join_tab; tmp2 != join_tab ; tmp2++)
    tmp2->table->status=tmp2->status;
  return NESTED_LOOP_OK;
}

/*****************************************************************************
  DESCRIPTION
    Functions that end one nested loop iteration. Different functions
    are used to support GROUP BY clause and to redirect records
    to a table (e.g. in case of SELECT into a temporary table) or to the
    network client.

  RETURN VALUES
    NESTED_LOOP_OK           - the record has been successfully handled
    NESTED_LOOP_ERROR        - a fatal error (like table corruption)
                               was detected
    NESTED_LOOP_KILLED       - thread shutdown was requested while processing
                               the record
    NESTED_LOOP_QUERY_LIMIT  - the record has been successfully handled;
                               additionally, the nested loop produced the
                               number of rows specified in the LIMIT clause
                               for the query
    NESTED_LOOP_CURSOR_LIMIT - the record has been successfully handled;
                               additionally, there is a cursor and the nested
                               loop algorithm produced the number of rows
                               that is specified for current cursor fetch
                               operation.
   All return values except NESTED_LOOP_OK abort the nested loop.
*****************************************************************************/
enum_nested_loop_state end_send(Join *join, JoinTable *, bool end_of_records)
{
  if (! end_of_records)
  {
    int error;
    if (join->having && join->having->val_int() == 0)
      return NESTED_LOOP_OK;               // Didn't match having
    error= 0;
    if (join->do_send_rows)
      error=join->result->send_data(*join->fields);
    if (error)
      return NESTED_LOOP_ERROR;
    if (++join->send_records >= join->unit->select_limit_cnt &&	join->do_send_rows)
    {
      if (join->select_options & OPTION_FOUND_ROWS)
      {
        JoinTable *jt=join->join_tab;
        if ((join->tables == 1) && !join->tmp_table && !join->sort_and_group
            && !join->send_group_parts && !join->having && !jt->select_cond &&
            !(jt->select && jt->select->quick) &&
            (jt->table->cursor->getEngine()->check_flag(HTON_BIT_STATS_RECORDS_IS_EXACT)) &&
                  (jt->ref.key < 0))
        {
          /* Join over all rows in table;  Return number of found rows */
          Table *table= jt->table;

          join->select_options^= OPTION_FOUND_ROWS;
          if (table->sort.record_pointers ||
              (table->sort.io_cache && my_b_inited(table->sort.io_cache)))
          {
            /* Using filesort */
            join->send_records= table->sort.found_records;
          }
          else
          {
            table->cursor->info(HA_STATUS_VARIABLE);
            join->send_records= table->cursor->stats.records;
          }
        }
        else
        {
          join->do_send_rows= 0;
          if (join->unit->fake_select_lex)
            join->unit->fake_select_lex->select_limit= 0;
          return NESTED_LOOP_OK;
        }
      }
      return NESTED_LOOP_QUERY_LIMIT;      // Abort nicely
    }
    else if (join->send_records >= join->fetch_limit)
    {
      /*
        There is a server side cursor and all rows for
        this fetch request are sent.
      */
      return NESTED_LOOP_CURSOR_LIMIT;
    }
  }

  return NESTED_LOOP_OK;
}

enum_nested_loop_state end_write(Join *join, JoinTable *, bool end_of_records)
{
  Table *table= join->tmp_table;

  if (join->session->getKilled())			// Aborted by user
  {
    join->session->send_kill_message();
    return NESTED_LOOP_KILLED;
  }
  if (!end_of_records)
  {
    copy_fields(&join->tmp_table_param);
    if (copy_funcs(join->tmp_table_param.items_to_copy, join->session))
      return NESTED_LOOP_ERROR;
    if (!join->having || join->having->val_int())
    {
      int error;
      join->found_records++;
      if ((error=table->cursor->insertRecord(table->getInsertRecord())))
      {
        if (!table->cursor->is_fatal_error(error, HA_CHECK_DUP))
        {
          return NESTED_LOOP_OK;
        }

        my_error(ER_USE_SQL_BIG_RESULT, MYF(0));
        return NESTED_LOOP_ERROR;        // Table is_full error
      }
      if (++join->send_records >= join->tmp_table_param.end_write_records && join->do_send_rows)
      {
        if (!(join->select_options & OPTION_FOUND_ROWS))
          return NESTED_LOOP_QUERY_LIMIT;
        join->do_send_rows= 0;
        join->unit->select_limit_cnt= HA_POS_ERROR;
        return NESTED_LOOP_OK;
      }
    }
  }

  return NESTED_LOOP_OK;
}

/** Group by searching after group record and updating it if possible. */
enum_nested_loop_state end_update(Join *join, JoinTable *, bool end_of_records)
{
  Table *table= join->tmp_table;
  Order *group;
  int	error;

  if (end_of_records)
    return NESTED_LOOP_OK;
  if (join->session->getKilled())			// Aborted by user
  {
    join->session->send_kill_message();
    return NESTED_LOOP_KILLED;
  }

  join->found_records++;
  copy_fields(&join->tmp_table_param);		// Groups are copied twice.
  /* Make a key of group index */
  for (group=table->group ; group ; group=group->next)
  {
    Item *item= *group->item;
    item->save_org_in_field(group->field);
    /* Store in the used key if the field was 0 */
    if (item->maybe_null)
      group->buff[-1]= (char) group->field->is_null();
  }
  if (!table->cursor->index_read_map(table->getUpdateRecord(),
                                   join->tmp_table_param.group_buff,
                                   HA_WHOLE_KEY,
                                   HA_READ_KEY_EXACT))
  {						/* Update old record */
    table->restoreRecord();
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error= table->cursor->updateRecord(table->getUpdateRecord(),
                                          table->getInsertRecord())))
    {
      table->print_error(error,MYF(0));
      return NESTED_LOOP_ERROR;
    }
    return NESTED_LOOP_OK;
  }

  /*
    Copy null bits from group key to table
    We can't copy all data as the key may have different format
    as the row data (for example as with VARCHAR keys)
  */
  KeyPartInfo *key_part;
  for (group=table->group,key_part=table->key_info[0].key_part;
       group ;
       group=group->next,key_part++)
  {
    if (key_part->null_bit)
      memcpy(table->getInsertRecord()+key_part->offset, group->buff, 1);
  }
  init_tmptable_sum_functions(join->sum_funcs);
  if (copy_funcs(join->tmp_table_param.items_to_copy, join->session))
    return NESTED_LOOP_ERROR;
  if ((error=table->cursor->insertRecord(table->getInsertRecord())))
  {
    my_error(ER_USE_SQL_BIG_RESULT, MYF(0));
    return NESTED_LOOP_ERROR;        // Table is_full error
  }
  join->send_records++;
  return NESTED_LOOP_OK;
}

/** Like end_update, but this is done with unique constraints instead of keys.  */
enum_nested_loop_state end_unique_update(Join *join, JoinTable *, bool end_of_records)
{
  Table *table= join->tmp_table;
  int	error;

  if (end_of_records)
    return NESTED_LOOP_OK;
  if (join->session->getKilled())			// Aborted by user
  {
    join->session->send_kill_message();
    return NESTED_LOOP_KILLED;
  }

  init_tmptable_sum_functions(join->sum_funcs);
  copy_fields(&join->tmp_table_param);		// Groups are copied twice.
  if (copy_funcs(join->tmp_table_param.items_to_copy, join->session))
    return NESTED_LOOP_ERROR;

  if (!(error= table->cursor->insertRecord(table->getInsertRecord())))
    join->send_records++;			// New group
  else
  {
    if ((int) table->get_dup_key(error) < 0)
    {
      table->print_error(error,MYF(0));
      return NESTED_LOOP_ERROR;
    }
    if (table->cursor->rnd_pos(table->getUpdateRecord(),table->cursor->dup_ref))
    {
      table->print_error(error,MYF(0));
      return NESTED_LOOP_ERROR;
    }
    table->restoreRecord();
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error= table->cursor->updateRecord(table->getUpdateRecord(),
                                          table->getInsertRecord())))
    {
      table->print_error(error,MYF(0));
      return NESTED_LOOP_ERROR;
    }
  }
  return NESTED_LOOP_OK;
}

/**
  allocate group fields or take prepared (cached).

  @param main_join   join of current select
  @param curr_join   current join (join of current select or temporary copy
                     of it)

  @retval
    0   ok
  @retval
    1   failed
*/
static bool make_group_fields(Join *main_join, Join *curr_join)
{
  if (main_join->group_fields_cache.size())
  {
    curr_join->group_fields= main_join->group_fields_cache;
    curr_join->sort_and_group= 1;
  }
  else
  {
    if (alloc_group_fields(curr_join, curr_join->group_list))
      return 1;
    main_join->group_fields_cache= curr_join->group_fields;
  }
  return (0);
}

/**
  calc how big buffer we need for comparing group entries.
*/
static void calc_group_buffer(Join *join, Order *group)
{
  uint32_t key_length=0, parts=0, null_parts=0;

  if (group)
    join->group= 1;
  for (; group ; group=group->next)
  {
    Item *group_item= *group->item;
    Field *field= group_item->get_tmp_table_field();
    if (field)
    {
      enum_field_types type;
      if ((type= field->type()) == DRIZZLE_TYPE_BLOB)
        key_length+=MAX_BLOB_WIDTH;   // Can't be used as a key
      else if (type == DRIZZLE_TYPE_VARCHAR)
        key_length+= field->field_length + HA_KEY_BLOB_LENGTH;
      else
        key_length+= field->pack_length();
    }
    else
    {
      switch (group_item->result_type()) {
      case REAL_RESULT:
        key_length+= sizeof(double);
        break;

      case INT_RESULT:
        key_length+= sizeof(int64_t);
        break;

      case DECIMAL_RESULT:
        key_length+= class_decimal_get_binary_size(group_item->max_length -
                                                (group_item->decimals ? 1 : 0),
                                                group_item->decimals);
        break;

      case STRING_RESULT:
        {
          enum enum_field_types type= group_item->field_type();
          /*
            As items represented as DATE/TIME fields in the group buffer
            have STRING_RESULT result type, we increase the length
            by 8 as maximum pack length of such fields.
          */
          if (field::isDateTime(type))
          {
            key_length+= 8;
          }
          else
          {
            /*
              Group strings are taken as varstrings and require an length field.
              A field is not yet created by create_tmp_field()
              and the sizes should match up.
            */
            key_length+= group_item->max_length + HA_KEY_BLOB_LENGTH;
          }

          break;
        }

      case ROW_RESULT:
        /* This case should never be choosen */
        assert(0);
        my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      }
    }

    parts++;

    if (group_item->maybe_null)
      null_parts++;
  }

  join->tmp_table_param.group_length=key_length+null_parts;
  join->tmp_table_param.group_parts=parts;
  join->tmp_table_param.group_null_parts=null_parts;
}

/**
  Get a list of buffers for saveing last group.

  Groups are saved in reverse order for easyer check loop.
*/
static bool alloc_group_fields(Join *join, Order *group)
{
  if (group)
  {
    for (; group ; group=group->next)
    {
      Cached_item *tmp= new_Cached_item(join->session, *group->item);
      if (!tmp)
        return true;
			join->group_fields.push_front(tmp);
    }
  }
  join->sort_and_group=1;     /* Mark for do_select */
  return false;
}

static uint32_t cache_record_length(Join *join,uint32_t idx)
{
  uint32_t length=0;
  JoinTable **pos,**end;
  Session *session=join->session;

  for (pos=join->best_ref+join->const_tables,end=join->best_ref+idx ;
       pos != end ;
       pos++)
  {
    JoinTable *join_tab= *pos;
    if (!join_tab->used_fieldlength)    /* Not calced yet */
      calc_used_field_length(session, join_tab);
    length+=join_tab->used_fieldlength;
  }
  return length;
}

/*
  Get the number of different row combinations for subset of partial join

  SYNOPSIS
    prev_record_reads()
      join       The join structure
      idx        Number of tables in the partial join order (i.e. the
                 partial join order is in join->positions[0..idx-1])
      found_ref  Bitmap of tables for which we need to find # of distinct
                 row combinations.

  DESCRIPTION
    Given a partial join order (in join->positions[0..idx-1]) and a subset of
    tables within that join order (specified in found_ref), find out how many
    distinct row combinations of subset tables will be in the result of the
    partial join order.

    This is used as follows: Suppose we have a table accessed with a ref-based
    method. The ref access depends on current rows of tables in found_ref.
    We want to count # of different ref accesses. We assume two ref accesses
    will be different if at least one of access parameters is different.
    Example: consider a query

    SELECT * FROM t1, t2, t3 WHERE t1.key=c1 AND t2.key=c2 AND t3.key=t1.field

    and a join order:
      t1,  ref access on t1.key=c1
      t2,  ref access on t2.key=c2
      t3,  ref access on t3.key=t1.field

    For t1: n_ref_scans = 1, n_distinct_ref_scans = 1
    For t2: n_ref_scans = records_read(t1), n_distinct_ref_scans=1
    For t3: n_ref_scans = records_read(t1)*records_read(t2)
            n_distinct_ref_scans = #records_read(t1)

    The reason for having this function (at least the latest version of it)
    is that we need to account for buffering in join execution.

    An edge-case example: if we have a non-first table in join accessed via
    ref(const) or ref(param) where there is a small number of different
    values of param, then the access will likely hit the disk cache and will
    not require any disk seeks.

    The proper solution would be to assume an LRU disk cache of some size,
    calculate probability of cache hits, etc. For now we just count
    identical ref accesses as one.

  RETURN
    Expected number of row combinations
*/
static double prev_record_reads(Join *join, uint32_t idx, table_map found_ref)
{
  double found=1.0;
  optimizer::Position *pos_end= join->getSpecificPosInPartialPlan(-1);
  for (optimizer::Position *pos= join->getSpecificPosInPartialPlan(idx - 1); 
       pos != pos_end; 
       pos--)
  {
    if (pos->examinePosition(found_ref))
    {
      found_ref|= pos->getRefDependMap();
      /*
        For the case of "t1 LEFT Join t2 ON ..." where t2 is a const table
        with no matching row we will get position[t2].records_read==0.
        Actually the size of output is one null-complemented row, therefore
        we will use value of 1 whenever we get records_read==0.

        Note
        - the above case can't occur if inner part of outer join has more
          than one table: table with no matches will not be marked as const.

        - Ideally we should add 1 to records_read for every possible null-
          complemented row. We're not doing it because: 1. it will require
          non-trivial code and add overhead. 2. The value of records_read
          is an inprecise estimate and adding 1 (or, in the worst case,
          #max_nested_outer_joins=64-1) will not make it any more precise.
      */
      if (pos->getFanout() > DBL_EPSILON)
        found*= pos->getFanout();
    }
  }
  return found;
}

/**
  Set up join struct according to best position.
*/
static bool get_best_combination(Join *join)
{
  uint32_t i,tablenr;
  table_map used_tables;
  JoinTable *join_tab,*j;
  optimizer::KeyUse *keyuse;
  uint32_t table_count;
  Session *session=join->session;
  optimizer::Position cur_pos;

  table_count=join->tables;
  join->join_tab=join_tab= new (session->mem) JoinTable[table_count];
  join->full_join=0;

  used_tables= OUTER_REF_TABLE_BIT;   // Outer row is already read
  for (j=join_tab, tablenr=0 ; tablenr < table_count ; tablenr++,j++)
  {
    Table *form;
    cur_pos= join->getPosFromOptimalPlan(tablenr);
    *j= *cur_pos.getJoinTable();
    form=join->table[tablenr]=j->table;
    used_tables|= form->map;
    form->reginfo.join_tab=j;
    if (!*j->on_expr_ref)
      form->reginfo.not_exists_optimize=0;  // Only with LEFT Join
    if (j->type == AM_CONST)
      continue;         // Handled in make_join_stat..

    j->ref.key = -1;
    j->ref.key_parts=0;

    if (j->type == AM_SYSTEM)
      continue;
    if (j->keys.none() || ! (keyuse= cur_pos.getKeyUse()))
    {
      j->type= AM_ALL;
      if (tablenr != join->const_tables)
        join->full_join=1;
    }
    else if (create_ref_for_key(join, j, keyuse, used_tables))
      return true;                        // Something went wrong
  }

  for (i=0 ; i < table_count ; i++)
    join->map2table[join->join_tab[i].table->tablenr]=join->join_tab+i;
  update_depend_map(join);
  return 0;
}

/** Save const tables first as used tables. */
static void set_position(Join *join,
                         uint32_t idx,
                         JoinTable *table,
                         optimizer::KeyUse *key)
{
  optimizer::Position tmp_pos(1.0, /* This is a const table */
                              0.0,
                              table,
                              key,
                              0);
  join->setPosInPartialPlan(idx, tmp_pos);

  /* Move the const table as down as possible in best_ref */
  JoinTable **pos=join->best_ref+idx+1;
  JoinTable *next=join->best_ref[idx];
  for (;next != table ; pos++)
  {
    JoinTable *tmp=pos[0];
    pos[0]=next;
    next=tmp;
  }
  join->best_ref[idx]=table;
}

/**
  Selects and invokes a search strategy for an optimal query plan.

  The function checks user-configurable parameters that control the search
  strategy for an optimal plan, selects the search method and then invokes
  it. Each specific optimization procedure stores the final optimal plan in
  the array 'join->best_positions', and the cost of the plan in
  'join->best_read'.

  @param join         pointer to the structure providing all context info for
                      the query
  @param join_tables  set of the tables in the query

  @retval
    false       ok
  @retval
    true        Fatal error
*/
static bool choose_plan(Join *join, table_map join_tables)
{
  uint32_t search_depth= join->session->variables.optimizer_search_depth;
  uint32_t prune_level=  join->session->variables.optimizer_prune_level;
  bool straight_join= test(join->select_options & SELECT_STRAIGHT_JOIN);

  join->cur_embedding_map.reset();
  reset_nj_counters(join->join_list);
  /*
    if (SELECT_STRAIGHT_JOIN option is set)
      reorder tables so dependent tables come after tables they depend
      on, otherwise keep tables in the order they were specified in the query
    else
      Apply heuristic: pre-sort all access plans with respect to the number of
      records accessed.
  */
  internal::my_qsort(join->best_ref + join->const_tables,
                     join->tables - join->const_tables, sizeof(JoinTable*),
                     straight_join ? join_tab_cmp_straight : join_tab_cmp);
  if (straight_join)
  {
    optimize_straight_join(join, join_tables);
  }
  else
  {
    if (search_depth == 0)
      /* Automatically determine a reasonable value for 'search_depth' */
      search_depth= determine_search_depth(join);
    if (greedy_search(join, join_tables, search_depth, prune_level))
      return true;
  }

  /*
    Store the cost of this query into a user variable
    Don't update last_query_cost for statements that are not "flat joins" :
    i.e. they have subqueries, unions or call stored procedures.
    TODO: calculate a correct cost for a query with subqueries and UNIONs.
  */
  if (join->session->lex().is_single_level_stmt())
    join->session->status_var.last_query_cost= join->best_read;
  return false;
}

/**
  Find the best access path for an extension of a partial execution
  plan and add this path to the plan.

  The function finds the best access path to table 's' from the passed
  partial plan where an access path is the general term for any means to
  access the data in 's'. An access path may use either an index or a scan,
  whichever is cheaper. The input partial plan is passed via the array
  'join->positions' of length 'idx'. The chosen access method for 's' and its
  cost are stored in 'join->positions[idx]'.

  @param join             pointer to the structure providing all context info
                          for the query
  @param s                the table to be joined by the function
  @param session              thread for the connection that submitted the query
  @param remaining_tables set of tables not included into the partial plan yet
  @param idx              the length of the partial plan
  @param record_count     estimate for the number of records returned by the
                          partial plan
  @param read_time        the cost of the partial plan

  @return
    None
*/
static void best_access_path(Join *join,
                             JoinTable *s,
                             Session *session,
                             table_map remaining_tables,
                             uint32_t idx,
                             double record_count,
                             double)
{
  optimizer::KeyUse *best_key= NULL;
  uint32_t best_max_key_part= 0;
  bool found_constraint= 0;
  double best= DBL_MAX;
  double best_time= DBL_MAX;
  double records= DBL_MAX;
  table_map best_ref_depends_map= 0;
  double tmp;
  ha_rows rec;

  if (s->keyuse)
  {                                            /* Use key if possible */
    Table *table= s->table;
    optimizer::KeyUse *keyuse= NULL;
    optimizer::KeyUse *start_key= NULL;
    double best_records= DBL_MAX;
    uint32_t max_key_part=0;

    /* Test how we can use keys */
    rec= s->records/MATCHING_ROWS_IN_OTHER_TABLE;  // Assumed records/key
    for (keyuse= s->keyuse; keyuse->getTable() == table; )
    {
      key_part_map found_part= 0;
      table_map found_ref= 0;
      uint32_t key= keyuse->getKey();
      KeyInfo *keyinfo= table->key_info + key;
      /* Bitmap of keyparts where the ref access is over 'keypart=const': */
      key_part_map const_part= 0;
      /* The or-null keypart in ref-or-null access: */
      key_part_map ref_or_null_part= 0;

      /* Calculate how many key segments of the current key we can use */
      start_key= keyuse;

      do /* For each keypart */
      {
        uint32_t keypart= keyuse->getKeypart();
        table_map best_part_found_ref= 0;
        double best_prev_record_reads= DBL_MAX;

        do /* For each way to access the keypart */
        {

          /*
            if 1. expression doesn't refer to forward tables
               2. we won't get two ref-or-null's
          */
          if (! (remaining_tables & keyuse->getUsedTables()) &&
              ! (ref_or_null_part && (keyuse->getOptimizeFlags() &
                                      KEY_OPTIMIZE_REF_OR_NULL)))
          {
            found_part|= keyuse->getKeypartMap();
            if (! (keyuse->getUsedTables() & ~join->const_table_map))
              const_part|= keyuse->getKeypartMap();

            double tmp2= prev_record_reads(join, idx, (found_ref |
                                                       keyuse->getUsedTables()));
            if (tmp2 < best_prev_record_reads)
            {
              best_part_found_ref= keyuse->getUsedTables() & ~join->const_table_map;
              best_prev_record_reads= tmp2;
            }
            if (rec > keyuse->getTableRows())
              rec= keyuse->getTableRows();
      /*
        If there is one 'key_column IS NULL' expression, we can
        use this ref_or_null optimisation of this field
      */
            if (keyuse->getOptimizeFlags() & KEY_OPTIMIZE_REF_OR_NULL)
              ref_or_null_part|= keyuse->getKeypartMap();
          }

          keyuse++;
        } while (keyuse->getTable() == table && keyuse->getKey() == key &&
                 keyuse->getKeypart() == keypart);
        found_ref|= best_part_found_ref;
      } while (keyuse->getTable() == table && keyuse->getKey() == key);

      /*
        Assume that that each key matches a proportional part of table.
      */
      if (!found_part)
        continue;                               // Nothing usable found

      if (rec < MATCHING_ROWS_IN_OTHER_TABLE)
        rec= MATCHING_ROWS_IN_OTHER_TABLE;      // Fix for small tables

      {
        found_constraint= 1;

        /*
          Check if we found full key
        */
        if (found_part == PREV_BITS(uint,keyinfo->key_parts) &&
            !ref_or_null_part)
        {                                         /* use eq key */
          max_key_part= UINT32_MAX;
          if ((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) == HA_NOSAME)
          {
            tmp = prev_record_reads(join, idx, found_ref);
            records=1.0;
          }
          else
          {
            if (!found_ref)
            {                                     /* We found a const key */
              /*
                ReuseRangeEstimateForRef-1:
                We get here if we've found a ref(const) (c_i are constants):
                  "(keypart1=c1) AND ... AND (keypartN=cN)"   [ref_const_cond]

                If range optimizer was able to construct a "range"
                access on this index, then its condition "quick_cond" was
                eqivalent to ref_const_cond (*), and we can re-use E(#rows)
                from the range optimizer.

                Proof of (*): By properties of range and ref optimizers
                quick_cond will be equal or tighther than ref_const_cond.
                ref_const_cond already covers "smallest" possible interval -
                a singlepoint interval over all keyparts. Therefore,
                quick_cond is equivalent to ref_const_cond (if it was an
                empty interval we wouldn't have got here).
              */
              if (table->quick_keys.test(key))
                records= (double) table->quick_rows[key];
              else
              {
                /* quick_range couldn't use key! */
                records= (double) s->records/rec;
              }
            }
            else
            {
              if (!(records=keyinfo->rec_per_key[keyinfo->key_parts-1]))
              {                                   /* Prefer longer keys */
                records=
                  ((double) s->records / (double) rec *
                   (1.0 +
                    ((double) (table->getShare()->max_key_length-keyinfo->key_length) /
                     (double) table->getShare()->max_key_length)));
                if (records < 2.0)
                  records=2.0;               /* Can't be as good as a unique */
              }
              /*
                ReuseRangeEstimateForRef-2:  We get here if we could not reuse
                E(#rows) from range optimizer. Make another try:

                If range optimizer produced E(#rows) for a prefix of the ref
                access we're considering, and that E(#rows) is lower then our
                current estimate, make an adjustment. The criteria of when we
                can make an adjustment is a special case of the criteria used
                in ReuseRangeEstimateForRef-3.
              */
              if (table->quick_keys.test(key) &&
                  const_part & (1 << table->quick_key_parts[key]) &&
                  table->quick_n_ranges[key] == 1 &&
                  records > (double) table->quick_rows[key])
              {
                records= (double) table->quick_rows[key];
              }
            }
            /* Limit the number of matched rows */
            tmp= records;
            set_if_smaller(tmp, (double) session->variables.max_seeks_for_key);
            if (table->covering_keys.test(key))
            {
              /* we can use only index tree */
              tmp= record_count * table->cursor->index_only_read_time(key, tmp);
            }
            else
              tmp= record_count * min(tmp,s->worst_seeks);
          }
        }
        else
        {
          /*
            Use as much key-parts as possible and a uniq key is better
            than a not unique key
            Set tmp to (previous record count) * (records / combination)
          */
          if ((found_part & 1) &&
              (!(table->index_flags(key) & HA_ONLY_WHOLE_INDEX) ||
               found_part == PREV_BITS(uint, keyinfo->key_parts)))
          {
            max_key_part= max_part_bit(found_part);
            /*
              ReuseRangeEstimateForRef-3:
              We're now considering a ref[or_null] access via
              (t.keypart1=e1 AND ... AND t.keypartK=eK) [ OR
              (same-as-above but with one cond replaced
               with "t.keypart_i IS NULL")]  (**)

              Try re-using E(#rows) from "range" optimizer:
              We can do so if "range" optimizer used the same intervals as
              in (**). The intervals used by range optimizer may be not
              available at this point (as "range" access might have choosen to
              create quick select over another index), so we can't compare
              them to (**). We'll make indirect judgements instead.
              The sufficient conditions for re-use are:
              (C1) All e_i in (**) are constants, i.e. found_ref==false. (if
                   this is not satisfied we have no way to know which ranges
                   will be actually scanned by 'ref' until we execute the
                   join)
              (C2) max #key parts in 'range' access == K == max_key_part (this
                   is apparently a necessary requirement)

              We also have a property that "range optimizer produces equal or
              tighter set of scan intervals than ref(const) optimizer". Each
              of the intervals in (**) are "tightest possible" intervals when
              one limits itself to using keyparts 1..K (which we do in #2).
              From here it follows that range access used either one, or
              both of the (I1) and (I2) intervals:

               (t.keypart1=c1 AND ... AND t.keypartK=eK)  (I1)
               (same-as-above but with one cond replaced
                with "t.keypart_i IS NULL")               (I2)

              The remaining part is to exclude the situation where range
              optimizer used one interval while we're considering
              ref-or-null and looking for estimate for two intervals. This
              is done by last limitation:

              (C3) "range optimizer used (have ref_or_null?2:1) intervals"
            */
            if (table->quick_keys.test(key) && !found_ref &&          //(C1)
                table->quick_key_parts[key] == max_key_part &&          //(C2)
                table->quick_n_ranges[key] == 1+((ref_or_null_part)?1:0)) //(C3)
            {
              tmp= records= (double) table->quick_rows[key];
            }
            else
            {
              /* Check if we have statistic about the distribution */
              if ((records= keyinfo->rec_per_key[max_key_part-1]))
              {
                /*
                  Fix for the case where the index statistics is too
                  optimistic: If
                  (1) We're considering ref(const) and there is quick select
                      on the same index,
                  (2) and that quick select uses more keyparts (i.e. it will
                      scan equal/smaller interval then this ref(const))
                  (3) and E(#rows) for quick select is higher then our
                      estimate,
                  Then
                    We'll use E(#rows) from quick select.

                  Q: Why do we choose to use 'ref'? Won't quick select be
                  cheaper in some cases ?
                  TODO: figure this out and adjust the plan choice if needed.
                */
                if (!found_ref && table->quick_keys.test(key) &&    // (1)
                    table->quick_key_parts[key] > max_key_part &&     // (2)
                    records < (double)table->quick_rows[key])         // (3)
                  records= (double)table->quick_rows[key];

                tmp= records;
              }
              else
              {
                /*
                  Assume that the first key part matches 1% of the cursor
                  and that the whole key matches 10 (duplicates) or 1
                  (unique) records.
                  Assume also that more key matches proportionally more
                  records
                  This gives the formula:
                  records = (x * (b-a) + a*c-b)/(c-1)

                  b = records matched by whole key
                  a = records matched by first key part (1% of all records?)
                  c = number of key parts in key
                  x = used key parts (1 <= x <= c)
                */
                double rec_per_key;
                if (!(rec_per_key=(double)
                      keyinfo->rec_per_key[keyinfo->key_parts-1]))
                  rec_per_key=(double) s->records/rec+1;

                if (!s->records)
                  tmp = 0;
                else if (rec_per_key/(double) s->records >= 0.01)
                  tmp = rec_per_key;
                else
                {
                  double a=s->records*0.01;
                  if (keyinfo->key_parts > 1)
                    tmp= (max_key_part * (rec_per_key - a) +
                          a*keyinfo->key_parts - rec_per_key)/
                         (keyinfo->key_parts-1);
                  else
                    tmp= a;
                  set_if_bigger(tmp,1.0);
                }
                records = (uint32_t) tmp;
              }

              if (ref_or_null_part)
              {
                /* We need to do two key searches to find key */
                tmp *= 2.0;
                records *= 2.0;
              }

              /*
                ReuseRangeEstimateForRef-4:  We get here if we could not reuse
                E(#rows) from range optimizer. Make another try:

                If range optimizer produced E(#rows) for a prefix of the ref
                access we're considering, and that E(#rows) is lower then our
                current estimate, make the adjustment.

                The decision whether we can re-use the estimate from the range
                optimizer is the same as in ReuseRangeEstimateForRef-3,
                applied to first table->quick_key_parts[key] key parts.
              */
              if (table->quick_keys.test(key) &&
                  table->quick_key_parts[key] <= max_key_part &&
                  const_part & (1 << table->quick_key_parts[key]) &&
                  table->quick_n_ranges[key] == 1 + ((ref_or_null_part &
                                                     const_part) ? 1 : 0) &&
                  records > (double) table->quick_rows[key])
              {
                tmp= records= (double) table->quick_rows[key];
              }
            }

            /* Limit the number of matched rows */
            set_if_smaller(tmp, (double) session->variables.max_seeks_for_key);
            if (table->covering_keys.test(key))
            {
              /* we can use only index tree */
              tmp= record_count * table->cursor->index_only_read_time(key, tmp);
            }
            else
              tmp= record_count * min(tmp,s->worst_seeks);
          }
          else
            tmp= best_time;                    // Do nothing
        }

      }
      if (tmp < best_time - records/(double) TIME_FOR_COMPARE)
      {
        best_time= tmp + records/(double) TIME_FOR_COMPARE;
        best= tmp;
        best_records= records;
        best_key= start_key;
        best_max_key_part= max_key_part;
        best_ref_depends_map= found_ref;
      }
    }
    records= best_records;
  }

  /*
    Don't test table scan if it can't be better.
    Prefer key lookup if we would use the same key for scanning.

    Don't do a table scan on InnoDB tables, if we can read the used
    parts of the row from any of the used index.
    This is because table scans uses index and we would not win
    anything by using a table scan.

    A word for word translation of the below if-statement in sergefp's
    understanding: we check if we should use table scan if:
    (1) The found 'ref' access produces more records than a table scan
        (or index scan, or quick select), or 'ref' is more expensive than
        any of them.
    (2) This doesn't hold: the best way to perform table scan is to to perform
        'range' access using index IDX, and the best way to perform 'ref'
        access is to use the same index IDX, with the same or more key parts.
        (note: it is not clear how this rule is/should be extended to
        index_merge quick selects)
    (3) See above note about InnoDB.
    (4) NOT ("FORCE INDEX(...)" is used for table and there is 'ref' access
             path, but there is no quick select)
        If the condition in the above brackets holds, then the only possible
        "table scan" access method is ALL/index (there is no quick select).
        Since we have a 'ref' access path, and FORCE INDEX instructs us to
        choose it over ALL/index, there is no need to consider a full table
        scan.
  */
  if ((records >= s->found_records || best > s->read_time) &&            // (1)
      ! (s->quick && best_key && s->quick->index == best_key->getKey() &&      // (2)
        best_max_key_part >= s->table->quick_key_parts[best_key->getKey()]) &&// (2)
      ! ((s->table->cursor->getEngine()->check_flag(HTON_BIT_TABLE_SCAN_ON_INDEX)) &&   // (3)
        ! s->table->covering_keys.none() && best_key && !s->quick) && // (3)
      ! (s->table->force_index && best_key && !s->quick))                 // (4)
  {                                             // Check full join
    ha_rows rnd_records= s->found_records;
    /*
      If there is a filtering condition on the table (i.e. ref analyzer found
      at least one "table.keyXpartY= exprZ", where exprZ refers only to tables
      preceding this table in the join order we're now considering), then
      assume that 25% of the rows will be filtered out by this condition.

      This heuristic is supposed to force tables used in exprZ to be before
      this table in join order.
    */
    if (found_constraint)
      rnd_records-= rnd_records/4;

    /*
      If applicable, get a more accurate estimate. Don't use the two
      heuristics at once.
    */
    if (s->table->quick_condition_rows != s->found_records)
      rnd_records= s->table->quick_condition_rows;

    /*
      Range optimizer never proposes a RANGE if it isn't better
      than FULL: so if RANGE is present, it's always preferred to FULL.
      Here we estimate its cost.
    */
    if (s->quick)
    {
      /*
        For each record we:
        - read record range through 'quick'
        - skip rows which does not satisfy WHERE constraints
        TODO:
        We take into account possible use of join cache for ALL/index
        access (see first else-branch below), but we don't take it into
        account here for range/index_merge access. Find out why this is so.
      */
      tmp= record_count *
        (s->quick->read_time +
         (s->found_records - rnd_records)/(double) TIME_FOR_COMPARE);
    }
    else
    {
      /* Estimate cost of reading table. */
      tmp= s->table->cursor->scan_time();
      if (s->table->map & join->outer_join)     // Can't use join cache
      {
        /*
          For each record we have to:
          - read the whole table record
          - skip rows which does not satisfy join condition
        */
        tmp= record_count *
          (tmp +
           (s->records - rnd_records)/(double) TIME_FOR_COMPARE);
      }
      else
      {
        /* We read the table as many times as join buffer becomes full. */
        tmp*= (1.0 + floor((double) cache_record_length(join,idx) *
                           record_count /
                           (double) session->variables.join_buff_size));
        /*
            We don't make full cartesian product between rows in the scanned
           table and existing records because we skip all rows from the
           scanned table, which does not satisfy join condition when
           we read the table (see flush_cached_records for details). Here we
           take into account cost to read and skip these records.
        */
        tmp+= (s->records - rnd_records)/(double) TIME_FOR_COMPARE;
      }
    }

    /*
      We estimate the cost of evaluating WHERE clause for found records
      as record_count * rnd_records / TIME_FOR_COMPARE. This cost plus
      tmp give us total cost of using Table SCAN
    */
    if (best == DBL_MAX ||
        (tmp  + record_count/(double) TIME_FOR_COMPARE*rnd_records <
         best + record_count/(double) TIME_FOR_COMPARE*records))
    {
      /*
        If the table has a range (s->quick is set) make_join_select()
        will ensure that this will be used
      */
      best= tmp;
      records= rnd_records;
      best_key= 0;
      /* range/index_merge/ALL/index access method are "independent", so: */
      best_ref_depends_map= 0;
    }
  }

  /* Update the cost information for the current partial plan */
  optimizer::Position tmp_pos(records,
                              best,
                              s,
                              best_key,
                              best_ref_depends_map);
  join->setPosInPartialPlan(idx, tmp_pos);

  if (!best_key &&
      idx == join->const_tables &&
      s->table == join->sort_by_table &&
      join->unit->select_limit_cnt >= records)
    join->sort_by_table= (Table*) 1;  // Must use temporary table

  return;
}

/**
  Select the best ways to access the tables in a query without reordering them.

    Find the best access paths for each query table and compute their costs
    according to their order in the array 'join->best_ref' (thus without
    reordering the join tables). The function calls sequentially
    'best_access_path' for each table in the query to select the best table
    access method. The final optimal plan is stored in the array
    'join->best_positions', and the corresponding cost in 'join->best_read'.

  @param join          pointer to the structure providing all context info for
                       the query
  @param join_tables   set of the tables in the query

  @note
    This function can be applied to:
    - queries with STRAIGHT_JOIN
    - internally to compute the cost of an arbitrary QEP
  @par
    Thus 'optimize_straight_join' can be used at any stage of the query
    optimization process to finalize a QEP as it is.
*/
static void optimize_straight_join(Join *join, table_map join_tables)
{
  JoinTable *s;
  optimizer::Position partial_pos;
  uint32_t idx= join->const_tables;
  double    record_count= 1.0;
  double    read_time=    0.0;

  for (JoinTable **pos= join->best_ref + idx ; (s= *pos) ; pos++)
  {
    /* Find the best access method from 's' to the current partial plan */
    best_access_path(join, s, join->session, join_tables, idx,
                     record_count, read_time);
    /* compute the cost of the new plan extended with 's' */
    partial_pos= join->getPosFromPartialPlan(idx);
    record_count*= partial_pos.getFanout();
    read_time+=    partial_pos.getCost();
    join_tables&= ~(s->table->map);
    ++idx;
  }

  read_time+= record_count / (double) TIME_FOR_COMPARE;
  partial_pos= join->getPosFromPartialPlan(join->const_tables);
  if (join->sort_by_table &&
      partial_pos.hasTableForSorting(join->sort_by_table))
    read_time+= record_count;  // We have to make a temp table
  join->copyPartialPlanIntoOptimalPlan(idx);
  join->best_read= read_time;
}

/**
  Find a good, possibly optimal, query execution plan (QEP) by a greedy search.

    The search procedure uses a hybrid greedy/exhaustive search with controlled
    exhaustiveness. The search is performed in N = card(remaining_tables)
    steps. Each step evaluates how promising is each of the unoptimized tables,
    selects the most promising table, and extends the current partial QEP with
    that table.  Currenly the most 'promising' table is the one with least
    expensive extension.\

    There are two extreme cases:
    -# When (card(remaining_tables) < search_depth), the estimate finds the
    best complete continuation of the partial QEP. This continuation can be
    used directly as a result of the search.
    -# When (search_depth == 1) the 'best_extension_by_limited_search'
    consideres the extension of the current QEP with each of the remaining
    unoptimized tables.

    All other cases are in-between these two extremes. Thus the parameter
    'search_depth' controlls the exhaustiveness of the search. The higher the
    value, the longer the optimizaton time and possibly the better the
    resulting plan. The lower the value, the fewer alternative plans are
    estimated, but the more likely to get a bad QEP.

    All intermediate and final results of the procedure are stored in 'join':
    - join->positions     : modified for every partial QEP that is explored
    - join->best_positions: modified for the current best complete QEP
    - join->best_read     : modified for the current best complete QEP
    - join->best_ref      : might be partially reordered

    The final optimal plan is stored in 'join->best_positions', and its
    corresponding cost in 'join->best_read'.

  @note
    The following pseudocode describes the algorithm of 'greedy_search':

    @code
    procedure greedy_search
    input: remaining_tables
    output: pplan;
    {
      pplan = <>;
      do {
        (t, a) = best_extension(pplan, remaining_tables);
        pplan = concat(pplan, (t, a));
        remaining_tables = remaining_tables - t;
      } while (remaining_tables != {})
      return pplan;
    }

  @endcode
    where 'best_extension' is a placeholder for a procedure that selects the
    most "promising" of all tables in 'remaining_tables'.
    Currently this estimate is performed by calling
    'best_extension_by_limited_search' to evaluate all extensions of the
    current QEP of size 'search_depth', thus the complexity of 'greedy_search'
    mainly depends on that of 'best_extension_by_limited_search'.

  @par
    If 'best_extension()' == 'best_extension_by_limited_search()', then the
    worst-case complexity of this algorithm is <=
    O(N*N^search_depth/search_depth). When serch_depth >= N, then the
    complexity of greedy_search is O(N!).

  @par
    In the future, 'greedy_search' might be extended to support other
    implementations of 'best_extension', e.g. some simpler quadratic procedure.

  @param join             pointer to the structure providing all context info
                          for the query
  @param remaining_tables set of tables not included into the partial plan yet
  @param search_depth     controlls the exhaustiveness of the search
  @param prune_level      the pruning heuristics that should be applied during
                          search

  @retval
    false       ok
  @retval
    true        Fatal error
*/
static bool greedy_search(Join      *join,
              table_map remaining_tables,
              uint32_t      search_depth,
              uint32_t      prune_level)
{
  double    record_count= 1.0;
  double    read_time=    0.0;
  uint32_t      idx= join->const_tables; // index into 'join->best_ref'
  uint32_t      best_idx;
  uint32_t      size_remain;    // cardinality of remaining_tables
  optimizer::Position best_pos;
  JoinTable  *best_table; // the next plan node to be added to the curr QEP

  /* number of tables that remain to be optimized */
  size_remain= internal::my_count_bits(remaining_tables);

  do {
    /* Find the extension of the current QEP with the lowest cost */
    join->best_read= DBL_MAX;
    if (best_extension_by_limited_search(join, remaining_tables, idx, record_count,
                                         read_time, search_depth, prune_level))
      return true;

    if (size_remain <= search_depth)
    {
      /*
        'join->best_positions' contains a complete optimal extension of the
        current partial QEP.
      */
      return false;
    }

    /* select the first table in the optimal extension as most promising */
    best_pos= join->getPosFromOptimalPlan(idx);
    best_table= best_pos.getJoinTable();
    /*
      Each subsequent loop of 'best_extension_by_limited_search' uses
      'join->positions' for cost estimates, therefore we have to update its
      value.
    */
    join->setPosInPartialPlan(idx, best_pos);

    /*
      We need to make best_extension_by_limited_search aware of the fact
      that it's not starting from top level, but from a rather specific
      position in the list of nested joins.
    */
    check_interleaving_with_nj (best_table);
    
      

    /* find the position of 'best_table' in 'join->best_ref' */
    best_idx= idx;
    JoinTable *pos= join->best_ref[best_idx];
    while (pos && best_table != pos)
      pos= join->best_ref[++best_idx];
    assert((pos != NULL)); // should always find 'best_table'
    /* move 'best_table' at the first free position in the array of joins */
    std::swap(join->best_ref[idx], join->best_ref[best_idx]);

    /* compute the cost of the new plan extended with 'best_table' */
    optimizer::Position partial_pos= join->getPosFromPartialPlan(idx);
    record_count*= partial_pos.getFanout();
    read_time+=    partial_pos.getCost();

    remaining_tables&= ~(best_table->table->map);
    --size_remain;
    ++idx;
  } while (true);
}


/**
  Find a good, possibly optimal, query execution plan (QEP) by a possibly
  exhaustive search.

    The procedure searches for the optimal ordering of the query tables in set
    'remaining_tables' of size N, and the corresponding optimal access paths to
    each table. The choice of a table order and an access path for each table
    constitutes a query execution plan (QEP) that fully specifies how to
    execute the query.

    The maximal size of the found plan is controlled by the parameter
    'search_depth'. When search_depth == N, the resulting plan is complete and
    can be used directly as a QEP. If search_depth < N, the found plan consists
    of only some of the query tables. Such "partial" optimal plans are useful
    only as input to query optimization procedures, and cannot be used directly
    to execute a query.

    The algorithm begins with an empty partial plan stored in 'join->positions'
    and a set of N tables - 'remaining_tables'. Each step of the algorithm
    evaluates the cost of the partial plan extended by all access plans for
    each of the relations in 'remaining_tables', expands the current partial
    plan with the access plan that results in lowest cost of the expanded
    partial plan, and removes the corresponding relation from
    'remaining_tables'. The algorithm continues until it either constructs a
    complete optimal plan, or constructs an optimal plartial plan with size =
    search_depth.

    The final optimal plan is stored in 'join->best_positions'. The
    corresponding cost of the optimal plan is in 'join->best_read'.

  @note
    The procedure uses a recursive depth-first search where the depth of the
    recursion (and thus the exhaustiveness of the search) is controlled by the
    parameter 'search_depth'.

  @note
    The pseudocode below describes the algorithm of
    'best_extension_by_limited_search'. The worst-case complexity of this
    algorithm is O(N*N^search_depth/search_depth). When serch_depth >= N, then
    the complexity of greedy_search is O(N!).

    @code
    procedure best_extension_by_limited_search(
      pplan in,             // in, partial plan of tables-joined-so-far
      pplan_cost,           // in, cost of pplan
      remaining_tables,     // in, set of tables not referenced in pplan
      best_plan_so_far,     // in/out, best plan found so far
      best_plan_so_far_cost,// in/out, cost of best_plan_so_far
      search_depth)         // in, maximum size of the plans being considered
    {
      for each table T from remaining_tables
      {
        // Calculate the cost of using table T as above
        cost = complex-series-of-calculations;

        // Add the cost to the cost so far.
        pplan_cost+= cost;

        if (pplan_cost >= best_plan_so_far_cost)
          // pplan_cost already too great, stop search
          continue;

        pplan= expand pplan by best_access_method;
        remaining_tables= remaining_tables - table T;
        if (remaining_tables is not an empty set
            and
            search_depth > 1)
        {
          best_extension_by_limited_search(pplan, pplan_cost,
                                           remaining_tables,
                                           best_plan_so_far,
                                           best_plan_so_far_cost,
                                           search_depth - 1);
        }
        else
        {
          best_plan_so_far_cost= pplan_cost;
          best_plan_so_far= pplan;
        }
      }
    }
    @endcode

  @note
    When 'best_extension_by_limited_search' is called for the first time,
    'join->best_read' must be set to the largest possible value (e.g. DBL_MAX).
    The actual implementation provides a way to optionally use pruning
    heuristic (controlled by the parameter 'prune_level') to reduce the search
    space by skipping some partial plans.

  @note
    The parameter 'search_depth' provides control over the recursion
    depth, and thus the size of the resulting optimal plan.

  @param join             pointer to the structure providing all context info
                          for the query
  @param remaining_tables set of tables not included into the partial plan yet
  @param idx              length of the partial QEP in 'join->positions';
                          since a depth-first search is used, also corresponds
                          to the current depth of the search tree;
                          also an index in the array 'join->best_ref';
  @param record_count     estimate for the number of records returned by the
                          best partial plan
  @param read_time        the cost of the best partial plan
  @param search_depth     maximum depth of the recursion and thus size of the
                          found optimal plan
                          (0 < search_depth <= join->tables+1).
  @param prune_level      pruning heuristics that should be applied during
                          optimization
                          (values: 0 = EXHAUSTIVE, 1 = PRUNE_BY_TIME_OR_ROWS)

  @retval
    false       ok
  @retval
    true        Fatal error
*/
static bool best_extension_by_limited_search(Join *join,
                                             table_map remaining_tables,
                                             uint32_t idx,
                                             double record_count,
                                             double read_time,
                                             uint32_t search_depth,
                                             uint32_t prune_level)
{
  Session *session= join->session;
  if (session->getKilled())  // Abort
    return true;

  /*
     'join' is a partial plan with lower cost than the best plan so far,
     so continue expanding it further with the tables in 'remaining_tables'.
  */
  JoinTable *s;
  double best_record_count= DBL_MAX;
  double best_read_time=    DBL_MAX;
  optimizer::Position partial_pos;

  for (JoinTable **pos= join->best_ref + idx ; (s= *pos) ; pos++)
  {
    table_map real_table_bit= s->table->map;
    if ((remaining_tables & real_table_bit) &&
        ! (remaining_tables & s->dependent) &&
        (! idx || ! check_interleaving_with_nj(s)))
    {
      double current_record_count, current_read_time;

      /*
        psergey-insideout-todo:
          when best_access_path() detects it could do an InsideOut scan or
          some other scan, have it return an insideout scan and a flag that
          requests to "fork" this loop iteration. (Q: how does that behave
          when the depth is insufficient??)
      */
      /* Find the best access method from 's' to the current partial plan */
      best_access_path(join, s, session, remaining_tables, idx,
                       record_count, read_time);
      /* Compute the cost of extending the plan with 's' */
      partial_pos= join->getPosFromPartialPlan(idx);
      current_record_count= record_count * partial_pos.getFanout();
      current_read_time=    read_time + partial_pos.getCost();

      /* Expand only partial plans with lower cost than the best QEP so far */
      if ((current_read_time +
           current_record_count / (double) TIME_FOR_COMPARE) >= join->best_read)
      {
        restore_prev_nj_state(s);
        continue;
      }

      /*
        Prune some less promising partial plans. This heuristic may miss
        the optimal QEPs, thus it results in a non-exhaustive search.
      */
      if (prune_level == 1)
      {
        if (best_record_count > current_record_count ||
            best_read_time > current_read_time ||
            (idx == join->const_tables && s->table == join->sort_by_table)) // 's' is the first table in the QEP
        {
          if (best_record_count >= current_record_count &&
              best_read_time >= current_read_time &&
              /* TODO: What is the reasoning behind this condition? */
              (! (s->key_dependent & remaining_tables) ||
               partial_pos.isConstTable()))
          {
            best_record_count= current_record_count;
            best_read_time=    current_read_time;
          }
        }
        else
        {
          restore_prev_nj_state(s);
          continue;
        }
      }

      if ( (search_depth > 1) && (remaining_tables & ~real_table_bit) )
      { /* Recursively expand the current partial plan */
        std::swap(join->best_ref[idx], *pos);
        if (best_extension_by_limited_search(join,
                                             remaining_tables & ~real_table_bit,
                                             idx + 1,
                                             current_record_count,
                                             current_read_time,
                                             search_depth - 1,
                                             prune_level))
          return true;
        std::swap(join->best_ref[idx], *pos);
      }
      else
      { /*
          'join' is either the best partial QEP with 'search_depth' relations,
          or the best complete QEP so far, whichever is smaller.
        */
        partial_pos= join->getPosFromPartialPlan(join->const_tables);
        current_read_time+= current_record_count / (double) TIME_FOR_COMPARE;
        if (join->sort_by_table &&
            partial_pos.hasTableForSorting(join->sort_by_table))
          /* We have to make a temp table */
          current_read_time+= current_record_count;
        if ((search_depth == 1) || (current_read_time < join->best_read))
        {
          join->copyPartialPlanIntoOptimalPlan(idx + 1);
          join->best_read= current_read_time - 0.001;
        }
      }
      restore_prev_nj_state(s);
    }
  }
  return false;
}

/**
  Heuristic procedure to automatically guess a reasonable degree of
  exhaustiveness for the greedy search procedure.

  The procedure estimates the optimization time and selects a search depth
  big enough to result in a near-optimal QEP, that doesn't take too long to
  find. If the number of tables in the query exceeds some constant, then
  search_depth is set to this constant.

  @param join   pointer to the structure providing all context info for
                the query

  @note
    This is an extremely simplistic implementation that serves as a stub for a
    more advanced analysis of the join. Ideally the search depth should be
    determined by learning from previous query optimizations, because it will
    depend on the CPU power (and other factors).

  @todo
    this value should be determined dynamically, based on statistics:
    uint32_t max_tables_for_exhaustive_opt= 7;

  @todo
    this value could be determined by some mapping of the form:
    depth : table_count -> [max_tables_for_exhaustive_opt..MAX_EXHAUSTIVE]

  @return
    A positive integer that specifies the search depth (and thus the
    exhaustiveness) of the depth-first search algorithm used by
    'greedy_search'.
*/
static uint32_t determine_search_depth(Join *join)
{
  uint32_t table_count=  join->tables - join->const_tables;
  uint32_t search_depth;
  /* TODO: this value should be determined dynamically, based on statistics: */
  uint32_t max_tables_for_exhaustive_opt= 7;

  if (table_count <= max_tables_for_exhaustive_opt)
    search_depth= table_count+1; // use exhaustive for small number of tables
  else
    /*
      TODO: this value could be determined by some mapping of the form:
      depth : table_count -> [max_tables_for_exhaustive_opt..MAX_EXHAUSTIVE]
    */
    search_depth= max_tables_for_exhaustive_opt; // use greedy search

  return search_depth;
}

static void make_simple_join(Join *join,Table *tmp_table)
{
  /*
    Reuse Table * and JoinTable if already allocated by a previous call
    to this function through Join::exec (may happen for sub-queries).
  */
  if (!join->table_reexec)
  {
    join->table_reexec= new (join->session->mem) Table*;
    if (join->tmp_join)
      join->tmp_join->table_reexec= join->table_reexec;
  }
  if (!join->join_tab_reexec)
  {
    join->join_tab_reexec= new (join->session->mem) JoinTable;
    if (join->tmp_join)
      join->tmp_join->join_tab_reexec= join->join_tab_reexec;
  }
  Table** tableptr= join->table_reexec;
  JoinTable* join_tab= join->join_tab_reexec;

  join->join_tab=join_tab;
  join->table=tableptr; tableptr[0]=tmp_table;
  join->tables=1;
  join->const_tables=0;
  join->const_table_map=0;
  join->tmp_table_param.field_count= join->tmp_table_param.sum_func_count=
    join->tmp_table_param.func_count=0;
  join->tmp_table_param.copy_field=join->tmp_table_param.copy_field_end=0;
  join->first_record=join->sort_and_group=0;
  join->send_records=(ha_rows) 0;
  join->group=0;
  join->row_limit=join->unit->select_limit_cnt;
  join->do_send_rows = (join->row_limit) ? 1 : 0;

  join_tab->table=tmp_table;
  join_tab->select=0;
  join_tab->select_cond=0;
  join_tab->quick=0;
  join_tab->type= AM_ALL;			/* Map through all records */
  join_tab->keys.set();                     /* test everything in quick */
  join_tab->info=0;
  join_tab->on_expr_ref=0;
  join_tab->last_inner= 0;
  join_tab->first_unmatched= 0;
  join_tab->ref.key = -1;
  join_tab->not_used_in_distinct=0;
  join_tab->read_first_record= join_init_read_record;
  join_tab->join=join;
  join_tab->ref.key_parts= 0;
  join_tab->read_record.init();
  tmp_table->status=0;
  tmp_table->null_row=0;
}

/**
  Fill in outer join related info for the execution plan structure.

    For each outer join operation left after simplification of the
    original query the function set up the following pointers in the linear
    structure join->join_tab representing the selected execution plan.
    The first inner table t0 for the operation is set to refer to the last
    inner table tk through the field t0->last_inner.
    Any inner table ti for the operation are set to refer to the first
    inner table ti->first_inner.
    The first inner table t0 for the operation is set to refer to the
    first inner table of the embedding outer join operation, if there is any,
    through the field t0->first_upper.
    The on expression for the outer join operation is attached to the
    corresponding first inner table through the field t0->on_expr_ref.
    Here ti are structures of the JoinTable type.

  EXAMPLE. For the query:
  @code
        SELECT * FROM t1
                      LEFT JOIN
                      (t2, t3 LEFT JOIN t4 ON t3.a=t4.a)
                      ON (t1.a=t2.a AND t1.b=t3.b)
          WHERE t1.c > 5,
  @endcode

    given the execution plan with the table order t1,t2,t3,t4
    is selected, the following references will be set;
    t4->last_inner=[t4], t4->first_inner=[t4], t4->first_upper=[t2]
    t2->last_inner=[t4], t2->first_inner=t3->first_inner=[t2],
    on expression (t1.a=t2.a AND t1.b=t3.b) will be attached to
    *t2->on_expr_ref, while t3.a=t4.a will be attached to *t4->on_expr_ref.

  @param join   reference to the info fully describing the query

  @note
    The function assumes that the simplification procedure has been
    already applied to the join query (see simplify_joins).
    This function can be called only after the execution plan
    has been chosen.
*/
static void make_outerjoin_info(Join *join)
{
  for (uint32_t i=join->const_tables ; i < join->tables ; i++)
  {
    JoinTable *tab=join->join_tab+i;
    Table *table=tab->table;
    TableList *tbl= table->pos_in_table_list;
    TableList *embedding= tbl->getEmbedding();

    if (tbl->outer_join)
    {
      /*
        Table tab is the only one inner table for outer join.
        (Like table t4 for the table reference t3 LEFT JOIN t4 ON t3.a=t4.a
        is in the query above.)
      */
      tab->last_inner= tab->first_inner= tab;
      tab->on_expr_ref= &tbl->on_expr;
      tab->cond_equal= tbl->cond_equal;
      if (embedding)
        tab->first_upper= embedding->getNestedJoin()->first_nested;
    }
    for ( ; embedding ; embedding= embedding->getEmbedding())
    {
      /* Ignore sj-nests: */
      if (!embedding->on_expr)
        continue;
      NestedJoin *nested_join= embedding->getNestedJoin();
      if (!nested_join->counter_)
      {
        /*
          Table tab is the first inner table for nested_join.
          Save reference to it in the nested join structure.
        */
        nested_join->first_nested= tab;
        tab->on_expr_ref= &embedding->on_expr;
        tab->cond_equal= tbl->cond_equal;
        if (embedding->getEmbedding())
          tab->first_upper= embedding->getEmbedding()->getNestedJoin()->first_nested;
      }
      if (!tab->first_inner)
        tab->first_inner= nested_join->first_nested;
      if (++nested_join->counter_ < nested_join->join_list.size())
        break;
      /* Table tab is the last inner table for nested join. */
      nested_join->first_nested->last_inner= tab;
    }
  }
  return;
}

static bool make_join_select(Join *join,
                             optimizer::SqlSelect *select,
                             COND *cond)
{
  Session *session= join->session;
  optimizer::Position cur_pos;
  if (select)
  {
    add_not_null_conds(join);
    table_map used_tables;
    if (cond)                /* Because of QUICK_GROUP_MIN_MAX_SELECT */
    {                        /* there may be a select without a cond. */
      if (join->tables > 1)
        cond->update_used_tables();		// Tablenr may have changed
      if (join->const_tables == join->tables &&
          session->lex().current_select->master_unit() ==
          &session->lex().unit)		// not upper level SELECT
        join->const_table_map|=RAND_TABLE_BIT;
      {						// Check const tables
        COND *const_cond=
          make_cond_for_table(cond,
              join->const_table_map,
              0, 1);
        for (JoinTable *tab= join->join_tab+join->const_tables;
            tab < join->join_tab+join->tables ; tab++)
        {
          if (*tab->on_expr_ref)
          {
            JoinTable *cond_tab= tab->first_inner;
            COND *tmp= make_cond_for_table(*tab->on_expr_ref,
                join->const_table_map,
                (  table_map) 0, 0);
            if (!tmp)
              continue;
            tmp= new Item_func_trig_cond(tmp, &cond_tab->not_null_compl);
            if (! tmp)
              return 1;
            tmp->quick_fix_field();
            cond_tab->select_cond= !cond_tab->select_cond ? tmp :
              new Item_cond_and(cond_tab->select_cond,
                  tmp);
            if (! cond_tab->select_cond)
              return 1;
            cond_tab->select_cond->quick_fix_field();
          }
        }
        if (const_cond && ! const_cond->val_int())
        {
          return 1;	 // Impossible const condition
        }
      }
    }
    used_tables=((select->const_tables=join->const_table_map) |
        OUTER_REF_TABLE_BIT | RAND_TABLE_BIT);
    for (uint32_t i=join->const_tables ; i < join->tables ; i++)
    {
      JoinTable *tab=join->join_tab+i;
      /*
         first_inner is the X in queries like:
         SELECT * FROM t1 LEFT OUTER JOIN (t2 JOIN t3) ON X
       */
      JoinTable *first_inner_tab= tab->first_inner;
      table_map current_map= tab->table->map;
      bool use_quick_range=0;
      COND *tmp;

      /*
         Following force including random expression in last table condition.
         It solve problem with select like SELECT * FROM t1 WHERE rand() > 0.5
       */
      if (i == join->tables-1)
        current_map|= OUTER_REF_TABLE_BIT | RAND_TABLE_BIT;
      used_tables|=current_map;

      if (tab->type == AM_REF && tab->quick &&
          (uint32_t) tab->ref.key == tab->quick->index &&
          tab->ref.key_length < tab->quick->max_used_key_length)
      {
        /* Range uses longer key;  Use this instead of ref on key */
        tab->type= AM_ALL;
        use_quick_range= 1;
        tab->use_quick= 1;
        tab->ref.key= -1;
        tab->ref.key_parts= 0;		// Don't use ref key.
        cur_pos= join->getPosFromOptimalPlan(i);
        cur_pos.setFanout(tab->quick->records);
        /*
           We will use join cache here : prevent sorting of the first
           table only and sort at the end.
         */
        if (i != join->const_tables && join->tables > join->const_tables + 1)
          join->full_join= 1;
      }

      if (join->full_join and not session->lex().current_select->is_cross and not cond)
      {
        my_error(ER_CARTESIAN_JOIN_ATTEMPTED, MYF(0));
        return 1;
      }

      tmp= NULL;
      if (cond)
        tmp= make_cond_for_table(cond,used_tables,current_map, 0);
      if (cond && !tmp && tab->quick)
      {						// Outer join
        if (tab->type != AM_ALL)
        {
          /*
             Don't use the quick method
             We come here in the case where we have 'key=constant' and
             the test is removed by make_cond_for_table()
           */
          delete tab->quick;
          tab->quick= 0;
        }
        else
        {
          /*
             Hack to handle the case where we only refer to a table
             in the ON part of an OUTER JOIN. In this case we want the code
             below to check if we should use 'quick' instead.
           */
          tmp= new Item_int((int64_t) 1,1);	// Always true
        }

      }
      if (tmp || !cond || tab->type == AM_REF || tab->type == AM_REF_OR_NULL ||
          tab->type == AM_EQ_REF)
      {
        optimizer::SqlSelect *sel= tab->select= ((optimizer::SqlSelect*)session->mem.memdup(select, sizeof(*select)));
        /*
           If tab is an inner table of an outer join operation,
           add a match guard to the pushed down predicate.
           The guard will turn the predicate on only after
           the first match for outer tables is encountered.
         */
        if (cond && tmp)
        {
          /*
             Because of QUICK_GROUP_MIN_MAX_SELECT there may be a select without
             a cond, so neutralize the hack above.
           */
          if (! (tmp= add_found_match_trig_cond(first_inner_tab, tmp, 0)))
            return 1;
          tab->select_cond=sel->cond=tmp;
        }
        else
          tab->select_cond= sel->cond= NULL;

        sel->head=tab->table;
        if (tab->quick)
        {
          /* Use quick key read if it's a constant and it's not used
             with key reading */
          if (tab->needed_reg.none() && tab->type != AM_EQ_REF
              && (tab->type != AM_REF || (uint32_t) tab->ref.key == tab->quick->index))
          {
            sel->quick=tab->quick;		// Use value from get_quick_...
            sel->quick_keys.reset();
            sel->needed_reg.reset();
          }
          else
          {
            delete tab->quick;
          }
          tab->quick= 0;
        }
        uint32_t ref_key= static_cast<uint32_t>(sel->head->reginfo.join_tab->ref.key + 1);
        if (i == join->const_tables && ref_key)
        {
          if (tab->const_keys.any() &&
              tab->table->reginfo.impossible_range)
            return 1;
        }
        else if (tab->type == AM_ALL && ! use_quick_range)
        {
          if (tab->const_keys.any() &&
              tab->table->reginfo.impossible_range)
            return 1;				// Impossible range
          /*
             We plan to scan all rows.
             Check again if we should use an index.
             We could have used an column from a previous table in
             the index if we are using limit and this is the first table
           */

          cur_pos= join->getPosFromOptimalPlan(i);
          if ((cond && (! ((tab->keys & tab->const_keys) == tab->keys) && i > 0)) ||
              (! tab->const_keys.none() && (i == join->const_tables) &&
              (join->unit->select_limit_cnt < cur_pos.getFanout()) && ((join->select_options & OPTION_FOUND_ROWS) == false)))
          {
            /* Join with outer join condition */
            COND *orig_cond= sel->cond;
            sel->cond= and_conds(sel->cond, *tab->on_expr_ref);

            /*
               We can't call sel->cond->fix_fields,
               as it will break tab->on_expr if it's AND condition
               (fix_fields currently removes extra AND/OR levels).
               Yet attributes of the just built condition are not needed.
               Thus we call sel->cond->quick_fix_field for safety.
             */
            if (sel->cond && ! sel->cond->fixed)
              sel->cond->quick_fix_field();

            if (sel->test_quick_select(session, tab->keys,
                  used_tables & ~ current_map,
                  (join->select_options &
                   OPTION_FOUND_ROWS ?
                   HA_POS_ERROR :
                   join->unit->select_limit_cnt), 0,
                  false) < 0)
            {
              /*
                 Before reporting "Impossible WHERE" for the whole query
                 we have to check isn't it only "impossible ON" instead
               */
              sel->cond=orig_cond;
              if (! *tab->on_expr_ref ||
                  sel->test_quick_select(session, tab->keys,
                    used_tables & ~ current_map,
                    (join->select_options &
                     OPTION_FOUND_ROWS ?
                     HA_POS_ERROR :
                     join->unit->select_limit_cnt),0,
                    false) < 0)
                return 1;			// Impossible WHERE
            }
            else
              sel->cond=orig_cond;

            /* Fix for EXPLAIN */
            if (sel->quick)
            {
              cur_pos= join->getPosFromOptimalPlan(i);
              cur_pos.setFanout(static_cast<double>(sel->quick->records));
            }
          }
          else
          {
            sel->needed_reg= tab->needed_reg;
            sel->quick_keys.reset();
          }
          if (!((tab->checked_keys & sel->quick_keys) == sel->quick_keys) ||
              !((tab->checked_keys & sel->needed_reg) == sel->needed_reg))
          {
            tab->keys= sel->quick_keys;
            tab->keys|= sel->needed_reg;
            tab->use_quick= (!sel->needed_reg.none() &&
                (select->quick_keys.none() ||
                 (select->quick &&
                  (select->quick->records >= 100L)))) ?
              2 : 1;
            sel->read_tables= used_tables & ~current_map;
          }
          if (i != join->const_tables && tab->use_quick != 2)
          {					/* Read with cache */
            if (cond &&
                (tmp=make_cond_for_table(cond,
                                         join->const_table_map |
                                         current_map,
                                         current_map, 0)))
            {
              tab->cache.select= (optimizer::SqlSelect*)session->mem.memdup(sel, sizeof(optimizer::SqlSelect));
              tab->cache.select->cond= tmp;
              tab->cache.select->read_tables= join->const_table_map;
            }
          }
        }
      }

      /*
         Push down conditions from all on expressions.
         Each of these conditions are guarded by a variable
         that turns if off just before null complemented row for
         outer joins is formed. Thus, the condition from an
         'on expression' are guaranteed not to be checked for
         the null complemented row.
       */

      /* First push down constant conditions from on expressions */
      for (JoinTable *join_tab= join->join_tab+join->const_tables;
          join_tab < join->join_tab+join->tables ; join_tab++)
      {
        if (*join_tab->on_expr_ref)
        {
          JoinTable *cond_tab= join_tab->first_inner;
          tmp= make_cond_for_table(*join_tab->on_expr_ref,
              join->const_table_map,
              (table_map) 0, 0);
          if (!tmp)
            continue;
          tmp= new Item_func_trig_cond(tmp, &cond_tab->not_null_compl);
          if (! tmp)
            return 1;
          tmp->quick_fix_field();
          cond_tab->select_cond= !cond_tab->select_cond ? tmp :
            new Item_cond_and(cond_tab->select_cond,tmp);
          if (! cond_tab->select_cond)
            return 1;
          cond_tab->select_cond->quick_fix_field();
        }
      }

      /* Push down non-constant conditions from on expressions */
      JoinTable *last_tab= tab;
      while (first_inner_tab && first_inner_tab->last_inner == last_tab)
      {
        /*
           Table tab is the last inner table of an outer join.
           An on expression is always attached to it.
         */
        COND *on_expr= *first_inner_tab->on_expr_ref;

        table_map used_tables2= (join->const_table_map |
            OUTER_REF_TABLE_BIT | RAND_TABLE_BIT);
        for (tab= join->join_tab+join->const_tables; tab <= last_tab ; tab++)
        {
          current_map= tab->table->map;
          used_tables2|= current_map;
          COND *tmp_cond= make_cond_for_table(on_expr, used_tables2,
              current_map, 0);
          if (tmp_cond)
          {
            JoinTable *cond_tab= tab < first_inner_tab ? first_inner_tab : tab;
            /*
               First add the guards for match variables of
               all embedding outer join operations.
             */
            if (!(tmp_cond= add_found_match_trig_cond(cond_tab->first_inner,
                                                      tmp_cond,
                                                      first_inner_tab)))
              return 1;
            /*
               Now add the guard turning the predicate off for
               the null complemented row.
             */
            tmp_cond= new Item_func_trig_cond(tmp_cond,
                &first_inner_tab->
                not_null_compl);
            if (tmp_cond)
              tmp_cond->quick_fix_field();
            /* Add the predicate to other pushed down predicates */
            cond_tab->select_cond= !cond_tab->select_cond ? tmp_cond :
              new Item_cond_and(cond_tab->select_cond,
                                tmp_cond);
            if (! cond_tab->select_cond)
              return 1;
            cond_tab->select_cond->quick_fix_field();
          }
        }
        first_inner_tab= first_inner_tab->first_upper;
      }
    }
  }
  return 0;
}

/*
  Plan refinement stage: do various set ups for the executioner

  SYNOPSIS
    make_join_readinfo()
      join           Join being processed
      options        Join's options (checking for SELECT_DESCRIBE,
                     SELECT_NO_JOIN_CACHE)
      no_jbuf_after  Don't use join buffering after table with this number.

  DESCRIPTION
    Plan refinement stage: do various set ups for the executioner
      - set up use of join buffering
      - push index conditions
      - increment counters
      - etc

  RETURN
    false - OK
    true  - Out of memory
*/
static void make_join_readinfo(Join& join)
{
  bool sorted= true;

  for (uint32_t i= join.const_tables; i < join.tables; i++)
  {
    JoinTable *tab=join.join_tab+i;
    Table *table=tab->table;
    tab->read_record.table= table;
    tab->read_record.cursor= table->cursor;
    tab->next_select=sub_select;		/* normal select */
    /*
      TODO: don't always instruct first table's ref/range access method to
      produce sorted output.
    */
    tab->sorted= sorted;
    sorted= false; // only first must be sorted

    if (tab->insideout_match_tab)
    {
      tab->insideout_buf= join.session->mem.alloc(tab->table->key_info[tab->index].key_length);
    }

    optimizer::AccessMethodFactory::create(tab->type)->getStats(*table, *tab);
  }

  join.join_tab[join.tables-1].next_select= NULL; /* Set by do_select */
}

/** Update the dependency map for the tables. */
static void update_depend_map(Join *join)
{
  JoinTable *join_tab=join->join_tab, *end=join_tab+join->tables;

  for (; join_tab != end ; join_tab++)
  {
    table_reference_st *ref= &join_tab->ref;
    table_map depend_map= 0;
    Item **item=ref->items;
    uint32_t i;
    for (i=0 ; i < ref->key_parts ; i++,item++)
      depend_map|=(*item)->used_tables();
    ref->depend_map=depend_map & ~OUTER_REF_TABLE_BIT;
    depend_map&= ~OUTER_REF_TABLE_BIT;
    for (JoinTable **tab=join->map2table; depend_map; tab++,depend_map>>=1 )
    {
      if (depend_map & 1)
        ref->depend_map|=(*tab)->ref.depend_map;
    }
  }
}

/** Update the dependency map for the sort order. */
static void update_depend_map(Join *join, Order *order)
{
  for (; order ; order=order->next)
  {
    table_map depend_map;
    order->item[0]->update_used_tables();
    order->depend_map=depend_map=order->item[0]->used_tables();
    // Not item_sum(), RAND() and no reference to table outside of sub select
    if (!(order->depend_map & (OUTER_REF_TABLE_BIT | RAND_TABLE_BIT))
        && !order->item[0]->with_sum_func)
    {
      for (JoinTable **tab=join->map2table; depend_map; tab++, depend_map>>=1)
      {
        if (depend_map & 1)
          order->depend_map|=(*tab)->ref.depend_map;
      }
    }
  }
}

/**
  Remove all constants and check if order_st only contains simple
  expressions.

  simple_order is set to 1 if sort_order only uses fields from head table
  and the head table is not a LEFT JOIN table.

  @param join			Join handler
  @param first_order		List of SORT or GROUP order
  @param cond			WHERE statement
  @param change_list		Set to 1 if we should remove things from list.
                               If this is not set, then only simple_order is
                               calculated.
  @param simple_order		Set to 1 if we are only using simple expressions

  @return
    Returns new sort order
*/
static Order *remove_constants(Join *join,Order *first_order, COND *cond, bool change_list, bool *simple_order)
{
  if (join->tables == join->const_tables)
    return change_list ? 0 : first_order;		// No need to sort

  Order *order,**prev_ptr;
  table_map first_table= join->join_tab[join->const_tables].table->map;
  table_map not_const_tables= ~join->const_table_map;
  table_map ref;

  prev_ptr= &first_order;
  *simple_order= *join->join_tab[join->const_tables].on_expr_ref ? 0 : 1;

  /* NOTE: A variable of not_const_tables ^ first_table; breaks gcc 2.7 */

  update_depend_map(join, first_order);
  for (order=first_order; order ; order=order->next)
  {
    table_map order_tables=order->item[0]->used_tables();
    if (order->item[0]->with_sum_func)
      *simple_order=0;				// Must do a temp table to sort
    else if (!(order_tables & not_const_tables))
    {
      if (order->item[0]->with_subselect)
        order->item[0]->val_str(&order->item[0]->str_value);
      continue;					// skip const item
    }
    else
    {
      if (order_tables & (RAND_TABLE_BIT | OUTER_REF_TABLE_BIT))
        *simple_order=0;
      else
      {
        Item *comp_item=0;
        if (cond && const_expression_in_where(cond,order->item[0], &comp_item))
        {
          continue;
        }
        if ((ref=order_tables & (not_const_tables ^ first_table)))
        {
          if (!(order_tables & first_table) &&
                    only_eq_ref_tables(join,first_order, ref))
          {
            continue;
          }
          *simple_order=0;			// Must do a temp table to sort
        }
      }
    }
    if (change_list)
      *prev_ptr= order;				// use this entry
    prev_ptr= &order->next;
  }
  if (change_list)
    *prev_ptr=0;
  if (prev_ptr == &first_order)			// Nothing to sort/group
    *simple_order=1;
  return(first_order);
}

static void return_zero_rows(Join *join, select_result *result, TableList *tables, List<Item> &fields, bool send_row, uint64_t select_options, const char *info, Item *having)
{
  if (select_options & SELECT_DESCRIBE)
  {
    optimizer::ExplainPlan planner(join, false, false, false, info);
    planner.printPlan();
    return;
  }

  join->join_free();

  if (send_row)
  {
    for (TableList *table= tables; table; table= table->next_leaf)
      table->table->mark_as_null_row();		// All fields are NULL
    if (having && having->val_int() == 0)
      send_row=0;
  }
  result->send_fields(fields);
  if (send_row)
  {
    List<Item>::iterator it(fields.begin());
    while (Item* item= it++)
      item->no_rows_in_result();
    result->send_data(fields);
  }
  result->send_eof();				// Should be safe
  /* Update results for FOUND_ROWS */
  join->session->limit_found_rows= join->session->examined_row_count= 0;
}

/**
  Simplify joins replacing outer joins by inner joins whenever it's
  possible.

    The function, during a retrieval of join_list,  eliminates those
    outer joins that can be converted into inner join, possibly nested.
    It also moves the on expressions for the converted outer joins
    and from inner joins to conds.
    The function also calculates some attributes for nested joins:
    - used_tables
    - not_null_tables
    - dep_tables.
    - on_expr_dep_tables
    The first two attributes are used to test whether an outer join can
    be substituted for an inner join. The third attribute represents the
    relation 'to be dependent on' for tables. If table t2 is dependent
    on table t1, then in any evaluated execution plan table access to
    table t2 must precede access to table t2. This relation is used also
    to check whether the query contains  invalid cross-references.
    The forth attribute is an auxiliary one and is used to calculate
    dep_tables.
    As the attribute dep_tables qualifies possibles orders of tables in the
    execution plan, the dependencies required by the straight join
    modifiers are reflected in this attribute as well.
    The function also removes all braces that can be removed from the join
    expression without changing its meaning.

  @note
    An outer join can be replaced by an inner join if the where condition
    or the on expression for an embedding nested join contains a conjunctive
    predicate rejecting null values for some attribute of the inner tables.

    E.g. in the query:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    @endcode
    the predicate t2.b < 5 rejects nulls.
    The query is converted first to:
    @code
      SELECT * FROM t1 INNER JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    @endcode
    then to the equivalent form:
    @code
      SELECT * FROM t1, t2 ON t2.a=t1.a WHERE t2.b < 5 AND t2.a=t1.a
    @endcode


    Similarly the following query:
    @code
      SELECT * from t1 LEFT JOIN (t2, t3) ON t2.a=t1.a t3.b=t1.b
        WHERE t2.c < 5
    @endcode
    is converted to:
    @code
      SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a t3.b=t1.b

    @endcode

    One conversion might trigger another:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a
                       LEFT JOIN t3 ON t3.b=t2.b
        WHERE t3 IS NOT NULL =>
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b =>
      SELECT * FROM t1, t2, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b AND t2.a=t1.a
  @endcode

    The function removes all unnecessary braces from the expression
    produced by the conversions.
    E.g.
    @code
      SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b
    @endcode
    finally is converted to:
    @code
      SELECT * FROM t1, t2, t3 WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b

    @endcode


    It also will remove braces from the following queries:
    @code
      SELECT * from (t1 LEFT JOIN t2 ON t2.a=t1.a) LEFT JOIN t3 ON t3.b=t2.b
      SELECT * from (t1, (t2,t3)) WHERE t1.a=t2.a AND t2.b=t3.b.
    @endcode

    The benefit of this simplification procedure is that it might return
    a query for which the optimizer can evaluate execution plan with more
    join orders. With a left join operation the optimizer does not
    consider any plan where one of the inner tables is before some of outer
    tables.

  IMPLEMENTATION
    The function is implemented by a recursive procedure.  On the recursive
    ascent all attributes are calculated, all outer joins that can be
    converted are replaced and then all unnecessary braces are removed.
    As join list contains join tables in the reverse order sequential
    elimination of outer joins does not require extra recursive calls.

  SEMI-JOIN NOTES
    Remove all semi-joins that have are within another semi-join (i.e. have
    an "ancestor" semi-join nest)

  EXAMPLES
    Here is an example of a join query with invalid cross references:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t3.a LEFT JOIN t3 ON t3.b=t1.b
    @endcode

  @param join        reference to the query info
  @param join_list   list representation of the join to be converted
  @param conds       conditions to add on expressions for converted joins
  @param top         true <=> conds is the where condition

  @return
    - The new condition, if success
    - 0, otherwise
*/
static COND *simplify_joins(Join *join, List<TableList> *join_list, COND *conds, bool top)
{
  NestedJoin *nested_join;
  TableList *prev_table= 0;
  List<TableList>::iterator li(join_list->begin());

  /*
    Try to simplify join operations from join_list.
    The most outer join operation is checked for conversion first.
  */
  while (TableList* table= li++)
  {
    table_map used_tables;
    table_map not_null_tables= (table_map) 0;

    if ((nested_join= table->getNestedJoin()))
    {
      /*
         If the element of join_list is a nested join apply
         the procedure to its nested join list first.
      */
      if (table->on_expr)
      {
        Item *expr= table->on_expr;
        /*
           If an on expression E is attached to the table,
           check all null rejected predicates in this expression.
           If such a predicate over an attribute belonging to
           an inner table of an embedded outer join is found,
           the outer join is converted to an inner join and
           the corresponding on expression is added to E.
	      */
        expr= simplify_joins(join, &nested_join->join_list, expr, false);

        if (!table->prep_on_expr || expr != table->on_expr)
        {
          assert(expr);

          table->on_expr= expr;
          table->prep_on_expr= expr->copy_andor_structure(join->session);
        }
      }
      nested_join->used_tables= (table_map) 0;
      nested_join->not_null_tables=(table_map) 0;
      conds= simplify_joins(join, &nested_join->join_list, conds, top);
      used_tables= nested_join->used_tables;
      not_null_tables= nested_join->not_null_tables;
    }
    else
    {
      if (!table->prep_on_expr)
        table->prep_on_expr= table->on_expr;
      used_tables= table->table->map;
      if (conds)
        not_null_tables= conds->not_null_tables();
    }

    if (table->getEmbedding())
    {
      table->getEmbedding()->getNestedJoin()->used_tables|= used_tables;
      table->getEmbedding()->getNestedJoin()->not_null_tables|= not_null_tables;
    }

    if (!table->outer_join || (used_tables & not_null_tables))
    {
      /*
        For some of the inner tables there are conjunctive predicates
        that reject nulls => the outer join can be replaced by an inner join.
      */
      table->outer_join= 0;
      if (table->on_expr)
      {
        /* Add ON expression to the WHERE or upper-level ON condition. */
        if (conds)
        {
          conds= and_conds(conds, table->on_expr);
          conds->top_level_item();
          /* conds is always a new item as both cond and on_expr existed */
          assert(!conds->fixed);
          conds->fix_fields(join->session, &conds);
        }
        else
          conds= table->on_expr;
        table->prep_on_expr= table->on_expr= 0;
      }
    }

    if (!top)
      continue;

    /*
      Only inner tables of non-convertible outer joins
      remain with on_expr.
    */
    if (table->on_expr)
    {
      table->setDepTables(table->getDepTables() | table->on_expr->used_tables());
      if (table->getEmbedding())
      {
        table->setDepTables(table->getDepTables() & ~table->getEmbedding()->getNestedJoin()->used_tables);
        /*
           Embedding table depends on tables used
           in embedded on expressions.
        */
        table->getEmbedding()->setOnExprDepTables(table->getEmbedding()->getOnExprDepTables() & table->on_expr->used_tables());
      }
      else
        table->setDepTables(table->getDepTables() & ~table->table->map);
    }

    if (prev_table)
    {
      //If this is straight join, set prev table to be dependent on all tables
      //from this nested join, so that correct join order is selected.
      if ((test(join->select_options & SELECT_STRAIGHT_JOIN)) ||
          prev_table->straight)
        prev_table->setDepTables(prev_table->getDepTables() | used_tables);
      if (prev_table->on_expr)
      {
        prev_table->setDepTables(prev_table->getDepTables() | table->getOnExprDepTables());
        table_map prev_used_tables= prev_table->getNestedJoin() ?
	                            prev_table->getNestedJoin()->used_tables :
	                            prev_table->table->map;
        /*
          If on expression contains only references to inner tables
          we still make the inner tables dependent on the outer tables.
          It would be enough to set dependency only on one outer table
          for them. Yet this is really a rare case.
	      */
        if (!(prev_table->on_expr->used_tables() & ~prev_used_tables))
          prev_table->setDepTables(prev_table->getDepTables() | used_tables);
      }
    }
    prev_table= table;
  }

  /*
    Flatten nested joins that can be flattened.
    no ON expression and not a semi-join => can be flattened.
  */
  li= join_list->begin();
  while (TableList* table= li++)
  {
    nested_join= table->getNestedJoin();
    if (nested_join && !table->on_expr)
    {
      List<TableList>::iterator it(nested_join->join_list.begin());
      while (TableList* tbl= it++)
      {
        tbl->setEmbedding(table->getEmbedding());
        tbl->setJoinList(table->getJoinList());
      }
      li.replace(nested_join->join_list);
    }
  }
  return(conds);
}

static int remove_duplicates(Join *join, Table *entry,List<Item> &fields, Item *having)
{
  entry->reginfo.lock_type=TL_WRITE;

  /* Calculate how many saved fields there is in list */
  uint32_t field_count= 0;
  List<Item>::iterator it(fields.begin());
  while (Item* item=it++)
  {
    if (item->get_tmp_table_field() && ! item->const_item())
      field_count++;
  }

  if (!field_count && !(join->select_options & OPTION_FOUND_ROWS) && !having)
  {                    // only const items with no OPTION_FOUND_ROWS
    join->unit->select_limit_cnt= 1;		// Only send first row
    return 0;
  }
  Field **first_field=entry->getFields() + entry->getShare()->sizeFields() - field_count;
  uint32_t offset= field_count ? entry->getField(entry->getShare()->sizeFields() - field_count)->offset(entry->getInsertRecord()) : 0;
  uint32_t reclength= entry->getShare()->getRecordLength() - offset;

  entry->free_io_cache();				// Safety
  entry->cursor->info(HA_STATUS_VARIABLE);
  int error;
  if (entry->getShare()->db_type() == heap_engine ||
      (!entry->getShare()->blob_fields &&
       ((ALIGN_SIZE(reclength) + HASH_OVERHEAD) * entry->cursor->stats.records < join->session->variables.sortbuff_size)))
  {
    error= remove_dup_with_hash_index(join->session, entry, field_count, first_field, reclength, having);
  }
  else
  {
    error= remove_dup_with_compare(join->session, entry, first_field, offset, having);
  }
  free_blobs(first_field);
  return error;
}

/**
  Function to setup clauses without sum functions.
*/
static int setup_without_group(Session *session, 
                               Item **ref_pointer_array,
                               TableList *tables,
                               TableList *,
                               List<Item> &fields,
                               List<Item> &all_fields,
                               COND **conds,
                               Order *order,
                               Order *group,
                               bool *hidden_group_fields)
{
  int res;
  nesting_map save_allow_sum_func=session->lex().allow_sum_func;

  session->lex().allow_sum_func&= ~(1 << session->lex().current_select->nest_level);
  res= session->setup_conds(tables, conds);

  session->lex().allow_sum_func|= 1 << session->lex().current_select->nest_level;
  res= res || setup_order(session, ref_pointer_array, tables, fields, all_fields, order);
  session->lex().allow_sum_func&= ~(1 << session->lex().current_select->nest_level);
  res= res || setup_group(session, ref_pointer_array, tables, fields, all_fields, group, hidden_group_fields);
  session->lex().allow_sum_func= save_allow_sum_func;
  return res;
}

/**
  Calculate the best possible join and initialize the join structure.

  @retval
    0	ok
  @retval
    1	Fatal error
*/
static bool make_join_statistics(Join *join, TableList *tables, COND *conds, DYNAMIC_ARRAY *keyuse_array)
{
  int error;
  Table *table;
  uint32_t i;
  uint32_t table_count;
  uint32_t const_count;
  uint32_t key;
  table_map found_const_table_map;
  table_map all_table_map;
  table_map found_ref;
  table_map refs;
  key_map const_ref;
  key_map eq_part;
  Table **table_vector= NULL;
  JoinTable *stat= NULL;
  JoinTable *stat_end= NULL;
  JoinTable *s= NULL;
  JoinTable **stat_ref= NULL;
  optimizer::KeyUse *keyuse= NULL;
  optimizer::KeyUse *start_keyuse= NULL;
  table_map outer_join= 0;
  vector<optimizer::SargableParam> sargables;
  JoinTable *stat_vector[MAX_TABLES+1];
  optimizer::Position *partial_pos;

  table_count= join->tables;
  stat= (JoinTable*) join->session->mem.calloc(sizeof(JoinTable)*table_count);
  stat_ref= new (join->session->mem) JoinTable*[MAX_TABLES];
  table_vector= new (join->session->mem) Table*[2 * table_count];

  join->best_ref=stat_vector;

  stat_end=stat+table_count;
  found_const_table_map= all_table_map=0;
  const_count=0;

  for (s= stat, i= 0;
       tables;
       s++, tables= tables->next_leaf, i++)
  {
    TableList *embedding= tables->getEmbedding();
    stat_vector[i]=s;
    s->keys.reset();
    s->const_keys.reset();
    s->checked_keys.reset();
    s->needed_reg.reset();
    table_vector[i]=s->table=table=tables->table;
    table->pos_in_table_list= tables;
    assert(table->cursor);
    error= table->cursor->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    if (error)
    {
        table->print_error(error, MYF(0));
        return 1;
    }
    table->quick_keys.reset();
    table->reginfo.join_tab=s;
    table->reginfo.not_exists_optimize=0;
    memset(table->const_key_parts, 0, sizeof(key_part_map)*table->getShare()->sizeKeys());
    all_table_map|= table->map;
    s->join=join;
    s->info=0;					// For describe

    s->dependent= tables->getDepTables();
    s->key_dependent= 0;
    table->quick_condition_rows= table->cursor->stats.records;

    s->on_expr_ref= &tables->on_expr;
    if (*s->on_expr_ref)
    {
      /* s is the only inner table of an outer join */
      if (!table->cursor->stats.records && !embedding)
      {						// Empty table
        s->dependent= 0;                        // Ignore LEFT JOIN depend.
        set_position(join, const_count++, s, NULL);
        continue;
      }
      outer_join|= table->map;
      s->embedding_map.reset();
      for (;embedding; embedding= embedding->getEmbedding())
        s->embedding_map|= embedding->getNestedJoin()->nj_map;
      continue;
    }
    if (embedding && !(false && ! embedding->getEmbedding()))
    {
      /* s belongs to a nested join, maybe to several embedded joins */
      s->embedding_map.reset();
      do
      {
        NestedJoin *nested_join= embedding->getNestedJoin();
        s->embedding_map|= nested_join->nj_map;
        s->dependent|= embedding->getDepTables();
        embedding= embedding->getEmbedding();
        outer_join|= nested_join->used_tables;
      }
      while (embedding);
      continue;
    }
    if ((table->cursor->stats.records <= 1) && !s->dependent &&
	      (table->cursor->getEngine()->check_flag(HTON_BIT_STATS_RECORDS_IS_EXACT)) &&
        !join->no_const_tables)
    {
      set_position(join, const_count++, s, NULL);
    }
  }
  stat_vector[i]=0;
  join->outer_join=outer_join;

  if (join->outer_join)
  {
    /*
       Build transitive closure for relation 'to be dependent on'.
       This will speed up the plan search for many cases with outer joins,
       as well as allow us to catch illegal cross references/
       Warshall's algorithm is used to build the transitive closure.
       As we use bitmaps to represent the relation the complexity
       of the algorithm is O((number of tables)^2).
    */
    for (i= 0; i < table_count; i++)
    {
      uint32_t j;
      table= stat[i].table;

      if (!table->reginfo.join_tab->dependent)
        continue;

      for (j= 0, s= stat; j < table_count; j++, s++)
      {
        if (s->dependent & table->map)
        {
          table_map was_dependent= s->dependent;
          s->dependent |= table->reginfo.join_tab->dependent;
          if (i > j && s->dependent != was_dependent)
          {
            i= j= 1;
            break;
          }
        }
      }
    }
    /* Catch illegal cross references for outer joins */
    for (i= 0, s= stat ; i < table_count ; i++, s++)
    {
      if (s->dependent & s->table->map)
      {
        join->tables=0;			// Don't use join->table
        my_message(ER_WRONG_OUTER_JOIN, ER(ER_WRONG_OUTER_JOIN), MYF(0));
        return 1;
      }
      if (outer_join & s->table->map)
        s->table->maybe_null= 1;

      s->key_dependent= s->dependent;
    }
  }

  if (conds || outer_join)
    update_ref_and_keys(join->session, keyuse_array, stat, join->tables, conds, join->cond_equal, ~outer_join, join->select_lex, sargables);

  /* Read tables with 0 or 1 rows (system tables) */
  join->const_table_map= 0;

  optimizer::Position *p_pos= join->getFirstPosInPartialPlan();
  optimizer::Position *p_end= join->getSpecificPosInPartialPlan(const_count);
  while (p_pos < p_end)
  {
    int tmp;
    s= p_pos->getJoinTable();
    s->type= AM_SYSTEM;
    join->const_table_map|=s->table->map;
    if ((tmp= s->joinReadConstTable(p_pos)))
    {
      if (tmp > 0)
        return 1;			// Fatal error
    }
    else
      found_const_table_map|= s->table->map;
    p_pos++;
  }

  /* loop until no more const tables are found */
  int ref_changed;
  do
  {
  more_const_tables_found:
    ref_changed = 0;
    found_ref=0;

    /*
      We only have to loop from stat_vector + const_count as
      set_position() will move all const_tables first in stat_vector
    */

    for (JoinTable **pos= stat_vector+const_count; (s= *pos); pos++)
    {
      table= s->table;

      /*
        If equi-join condition by a key is null rejecting and after a
        substitution of a const table the key value happens to be null
        then we can state that there are no matches for this equi-join.
      */
      if ((keyuse= s->keyuse) && *s->on_expr_ref && s->embedding_map.none())
      {
        /*
          When performing an outer join operation if there are no matching rows
          for the single row of the outer table all the inner tables are to be
          null complemented and thus considered as constant tables.
          Here we apply this consideration to the case of outer join operations
          with a single inner table only because the case with nested tables
          would require a more thorough analysis.
          TODO. Apply single row substitution to null complemented inner tables
          for nested outer join operations.
        */
        while (keyuse->getTable() == table)
        {
          if (! (keyuse->getVal()->used_tables() & ~join->const_table_map) &&
              keyuse->getVal()->is_null() && keyuse->isNullRejected())
          {
            s->type= AM_CONST;
            table->mark_as_null_row();
            found_const_table_map|= table->map;
            join->const_table_map|= table->map;
            set_position(join, const_count++, s, NULL);
            goto more_const_tables_found;
           }
          keyuse++;
        }
      }

      if (s->dependent)				// If dependent on some table
      {
        // All dep. must be constants
        if (s->dependent & ~(found_const_table_map))
          continue;
        if (table->cursor->stats.records <= 1L &&
            (table->cursor->getEngine()->check_flag(HTON_BIT_STATS_RECORDS_IS_EXACT)) &&
                  !table->pos_in_table_list->getEmbedding())
        {					// system table
          int tmp= 0;
          s->type= AM_SYSTEM;
          join->const_table_map|=table->map;
          set_position(join, const_count++, s, NULL);
          partial_pos= join->getSpecificPosInPartialPlan(const_count - 1);
          if ((tmp= s->joinReadConstTable(partial_pos)))
          {
            if (tmp > 0)
              return 1;			// Fatal error
          }
          else
            found_const_table_map|= table->map;
          continue;
        }
      }
      /* check if table can be read by key or table only uses const refs */
      if ((keyuse=s->keyuse))
      {
        s->type= AM_REF;
        while (keyuse->getTable() == table)
        {
          start_keyuse= keyuse;
          key= keyuse->getKey();
          s->keys.set(key);               // QQ: remove this ?

          refs= 0;
          const_ref.reset();
          eq_part.reset();
          do
          {
            if (keyuse->getVal()->type() != Item::NULL_ITEM && 
                ! keyuse->getOptimizeFlags())
            {
              if (! ((~found_const_table_map) & keyuse->getUsedTables()))
                const_ref.set(keyuse->getKeypart());
              else
                refs|= keyuse->getUsedTables();
              eq_part.set(keyuse->getKeypart());
            }
            keyuse++;
          } while (keyuse->getTable() == table && keyuse->getKey() == key);

          if (is_keymap_prefix(eq_part, table->key_info[key].key_parts) &&
              ! table->pos_in_table_list->getEmbedding())
          {
            if ((table->key_info[key].flags & (HA_NOSAME)) == HA_NOSAME)
            {
              if (const_ref == eq_part)
              {					// Found everything for ref.
                int tmp;
                ref_changed = 1;
                s->type= AM_CONST;
                join->const_table_map|= table->map;
                set_position(join, const_count++, s, start_keyuse);
                if (create_ref_for_key(join, s, start_keyuse, found_const_table_map))
                  return 1;
                partial_pos= join->getSpecificPosInPartialPlan(const_count - 1);
                if ((tmp=s->joinReadConstTable(partial_pos)))
                {
                  if (tmp > 0)
                    return 1;			// Fatal error
                }
                else
                  found_const_table_map|= table->map;
                break;
              }
              else
                found_ref|= refs;      // Table is const if all refs are const
            }
            else if (const_ref == eq_part)
              s->const_keys.set(key);
          }
        }
      }
    }
  } while (join->const_table_map & found_ref && ref_changed);

  /*
    Update info on indexes that can be used for search lookups as
    reading const tables may has added new sargable predicates.
  */
  if (const_count && ! sargables.empty())
  {
    BOOST_FOREACH(vector<optimizer::SargableParam>::reference iter, sargables)
    {
      Field& field= *iter.getField();
      JoinTable *join_tab= field.getTable()->reginfo.join_tab;
      key_map possible_keys= field.key_start & field.getTable()->keys_in_use_for_query;
      bool is_const= true;
      for (uint32_t j= 0; j < iter.getNumValues(); j++)
        is_const&= iter.isConstItem(j);
      if (is_const)
        join_tab[0].const_keys|= possible_keys;
    }
  }

  /* Calc how many (possible) matched records in each table */

  for (s=stat ; s < stat_end ; s++)
  {
    if (s->type == AM_SYSTEM || s->type == AM_CONST)
    {
      /* Only one matching row */
      s->found_records=s->records=s->read_time=1; s->worst_seeks=1.0;
      continue;
    }
    /* Approximate found rows and time to read them */
    s->found_records=s->records=s->table->cursor->stats.records;
    s->read_time=(ha_rows) s->table->cursor->scan_time();

    /*
      Set a max range of how many seeks we can expect when using keys
      This is can't be to high as otherwise we are likely to use
      table scan.
    */
    s->worst_seeks= min((double) s->found_records / 10,
                        (double) s->read_time*3);
    if (s->worst_seeks < 2.0)			// Fix for small tables
      s->worst_seeks=2.0;

    /*
      Add to stat->const_keys those indexes for which all group fields or
      all select distinct fields participate in one index.
    */
    add_group_and_distinct_keys(join, s);

    if (s->const_keys.any() &&
        !s->table->pos_in_table_list->getEmbedding())
    {
      ha_rows records;
      optimizer::SqlSelect *select= NULL;
      select= optimizer::make_select(s->table, found_const_table_map, found_const_table_map, *s->on_expr_ref ? *s->on_expr_ref : conds, 1, &error);
      if (! select)
        return 1;
      records= get_quick_record_count(join->session, select, s->table, &s->const_keys, join->row_limit);
      s->quick=select->quick;
      s->needed_reg=select->needed_reg;
      select->quick=0;

      if (records == 0 && s->table->reginfo.impossible_range)
      {
        /*
          Impossible WHERE or ON expression
          In case of ON, we mark that the we match one empty NULL row.
          In case of WHERE, don't set found_const_table_map to get the
          caller to abort with a zero row result.
        */
        join->const_table_map|= s->table->map;
        set_position(join, const_count++, s, NULL);
        s->type= AM_CONST;
        if (*s->on_expr_ref)
        {
          /* Generate empty row */
          s->info= "Impossible ON condition";
          found_const_table_map|= s->table->map;
          s->type= AM_CONST;
          s->table->mark_as_null_row();		// All fields are NULL
        }
      }
      if (records != HA_POS_ERROR)
      {
        s->found_records=records;
        s->read_time= (ha_rows) (s->quick ? s->quick->read_time : 0.0);
      }
      delete select;
    }
  }

  join->join_tab=stat;
  join->map2table=stat_ref;
  join->table= join->all_tables=table_vector;
  join->const_tables=const_count;
  join->found_const_table_map=found_const_table_map;

  /* Find an optimal join order of the non-constant tables. */
  if (join->const_tables != join->tables)
  {
    optimize_keyuse(join, keyuse_array);
    // @note c_str() is not likely to be valid here if dtrace expects it to
    // exist for any period of time.
    DRIZZLE_QUERY_OPT_CHOOSE_PLAN_START(join->session->getQueryString()->c_str(), join->session->thread_id);
    bool res= choose_plan(join, all_table_map & ~join->const_table_map);
    DRIZZLE_QUERY_OPT_CHOOSE_PLAN_DONE(res ? 1 : 0);
    if (res)
      return true;
  }
  else
  {
    join->copyPartialPlanIntoOptimalPlan(join->const_tables);
    join->best_read= 1.0;
  }
  /* Generate an execution plan from the found optimal join order. */
  return join->session->getKilled() || get_best_combination(join);
}

/**
  Assign each nested join structure a bit in the nested join bitset.

    Assign each nested join structure (except "confluent" ones - those that
    embed only one element) a bit in the nested join bitset.

  @param join          Join being processed
  @param join_list     List of tables
  @param first_unused  Number of first unused bit in the nest joing bitset before the
                       call

  @note
    This function is called after simplify_joins(), when there are no
    redundant nested joins, #non_confluent_nested_joins <= #tables_in_join so
    we will not run out of bits in the nested join bitset.

  @return
    First unused bit in the nest join bitset after the call.
*/
static uint32_t build_bitmap_for_nested_joins(List<TableList> *join_list, uint32_t first_unused)
{
  List<TableList>::iterator li(join_list->begin());
  while (TableList* table= li++)
  {
    if (NestedJoin* nested_join= table->getNestedJoin())
    {
      /*
        It is guaranteed by simplify_joins() function that a nested join
        that has only one child is either
         - a single-table view (the child is the underlying table), or
         - a single-table semi-join nest

        We don't assign bits to such sj-nests because
        1. it is redundant (a "sequence" of one table cannot be interleaved
            with anything)
        2. we could run out of bits in the nested join bitset otherwise.
      */
      if (nested_join->join_list.size() != 1)
      {
        /* Don't assign bits to sj-nests */
        if (table->on_expr)
          nested_join->nj_map.set(first_unused++);
        first_unused= build_bitmap_for_nested_joins(&nested_join->join_list,
                                                    first_unused);
      }
    }
  }
  return(first_unused);
}


/**
  Return table number if there is only one table in sort order
  and group and order is compatible, else return 0.
*/
static Table *get_sort_by_table(Order *a, Order *b,TableList *tables)
{
  table_map map= 0;

  if (!a)
    a= b;					// Only one need to be given
  else if (!b)
    b= a;

  for (; a && b; a=a->next,b=b->next)
  {
    if (!(*a->item)->eq(*b->item,1))
      return NULL;
    map|= a->item[0]->used_tables();
  }
  if (!map || (map & (RAND_TABLE_BIT | OUTER_REF_TABLE_BIT)))
    return NULL;

  for (; !(map & tables->table->map); tables= tables->next_leaf) {};
  if (map != tables->table->map)
    return NULL;				// More than one table
  return tables->table;
}

/**
  Set NestedJoin::counter=0 in all nested joins in passed list.

    Recursively set NestedJoin::counter=0 for all nested joins contained in
    the passed join_list.

  @param join_list  List of nested joins to process. It may also contain base
                    tables which will be ignored.
*/
static void reset_nj_counters(List<TableList> *join_list)
{
  List<TableList>::iterator li(join_list->begin());
  while (TableList* table= li++)
  {
    NestedJoin *nested_join;
    if ((nested_join= table->getNestedJoin()))
    {
      nested_join->counter_= 0;
      reset_nj_counters(&nested_join->join_list);
    }
  }
}

/**
  Return 1 if second is a subpart of first argument.

  If first parts has different direction, change it to second part
  (group is sorted like order)
*/
static bool test_if_subpart(Order *a, Order *b)
{
  for (; a && b; a=a->next,b=b->next)
  {
    if ((*a->item)->eq(*b->item,1))
      a->asc=b->asc;
    else
      return 0;
  }
  return test(!b);
}

/**
  Nested joins perspective: Remove the last table from the join order.

  The algorithm is the reciprocal of check_interleaving_with_nj(), hence
  parent join nest nodes are updated only when the last table in its child
  node is removed. The ASCII graphic below will clarify.

  %A table nesting such as <tt> t1 x [ ( t2 x t3 ) x ( t4 x t5 ) ] </tt>is
  represented by the below join nest tree.

  @verbatim
                     NJ1
                  _/ /  \
                _/  /    NJ2
              _/   /     / \ 
             /    /     /   \
   t1 x [ (t2 x t3) x (t4 x t5) ]
  @endverbatim

  At the point in time when check_interleaving_with_nj() adds the table t5 to
  the query execution plan, QEP, it also directs the node named NJ2 to mark
  the table as covered. NJ2 does so by incrementing its @c counter
  member. Since all of NJ2's tables are now covered by the QEP, the algorithm
  proceeds up the tree to NJ1, incrementing its counter as well. All join
  nests are now completely covered by the QEP.

  restore_prev_nj_state() does the above in reverse. As seen above, the node
  NJ1 contains the nodes t2, t3, and NJ2. Its counter being equal to 3 means
  that the plan covers t2, t3, and NJ2, @e and that the sub-plan (t4 x t5)
  completely covers NJ2. The removal of t5 from the partial plan will first
  decrement NJ2's counter to 1. It will then detect that NJ2 went from being
  completely to partially covered, and hence the algorithm must continue
  upwards to NJ1 and decrement its counter to 2. %A subsequent removal of t4
  will however not influence NJ1 since it did not un-cover the last table in
  NJ2.

  SYNOPSIS
    restore_prev_nj_state()
      last  join table to remove, it is assumed to be the last in current 
            partial join order.
     
  DESCRIPTION

    Remove the last table from the partial join order and update the nested
    joins counters and join->cur_embedding_map. It is ok to call this 
    function for the first table in join order (for which 
    check_interleaving_with_nj has not been called)

  @param last  join table to remove, it is assumed to be the last in current
               partial join order.
*/

static void restore_prev_nj_state(JoinTable *last)
{
  TableList *last_emb= last->table->pos_in_table_list->getEmbedding();
  Join *join= last->join;
  for (;last_emb != NULL; last_emb= last_emb->getEmbedding())
  {
    NestedJoin *nest= last_emb->getNestedJoin();
    
    bool was_fully_covered= nest->is_fully_covered();
    
    if (--nest->counter_ == 0)
      join->cur_embedding_map&= ~nest->nj_map;
    
    if (!was_fully_covered)
      break;
    
    join->cur_embedding_map|= nest->nj_map;
  }
}

/**
  Create a condition for a const reference and add this to the
  currenct select for the table.
*/
static bool add_ref_to_table_cond(Session *session, JoinTable *join_tab)
{
  if (!join_tab->ref.key_parts)
    return false;

  Item_cond_and *cond=new Item_cond_and();
  Table *table=join_tab->table;

  for (uint32_t i=0 ; i < join_tab->ref.key_parts ; i++)
  {
    Field *field=table->getField(table->key_info[join_tab->ref.key].key_part[i].fieldnr - 1);
    Item *value=join_tab->ref.items[i];
    cond->add(new Item_func_equal(new Item_field(field), value));
  }
  if (session->is_fatal_error)
    return true;

  if (!cond->fixed)
    cond->fix_fields(session, (Item**)&cond);
  int error = 0;
  if (join_tab->select)
  {
    cond->add(join_tab->select->cond);
    join_tab->select_cond=join_tab->select->cond=cond;
  }
  else if ((join_tab->select= optimizer::make_select(join_tab->table, 0, 0, cond, 0, &error)))
    join_tab->select_cond=cond;

  return error;
}

static void free_blobs(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      ((Field_blob *) (*ptr))->free();
  }
}

/**
  @} (end of group Query_Optimizer)
*/

} /* namespace drizzled */
