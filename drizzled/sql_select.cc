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

/**
  @file

  @brief
  select_query and join optimization

  @defgroup Query_Optimizer  Query Optimizer
  @{
*/
#include <config.h>

#include <string>
#include <iostream>
#include <algorithm>
#include <vector>

#include <drizzled/sql_select.h> /* include join.h */

#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/util/test.h>
#include <drizzled/name_resolution_context_state.h>
#include <drizzled/nested_join.h>
#include <drizzled/probes.h>
#include <drizzled/show.h>
#include <drizzled/item/cache.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/copy_string.h>
#include <drizzled/item/uint.h>
#include <drizzled/cached_item.h>
#include <drizzled/sql_base.h>
#include <drizzled/field/blob.h>
#include <drizzled/check_stack_overrun.h>
#include <drizzled/lock.h>
#include <drizzled/item/outer_ref.h>
#include <drizzled/index_hint.h>
#include <drizzled/records.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/drizzled.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/sql_union.h>
#include <drizzled/optimizer/key_field.h>
#include <drizzled/optimizer/position.h>
#include <drizzled/optimizer/sargable_param.h>
#include <drizzled/optimizer/key_use.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/optimizer/quick_range_select.h>
#include <drizzled/optimizer/quick_ror_intersect_select.h>
#include <drizzled/filesort.h>
#include <drizzled/sql_lex.h>
#include <drizzled/session.h>
#include <drizzled/sort_field.h>
#include <drizzled/select_result.h>
#include <drizzled/key.h>
#include <drizzled/my_hash.h>

using namespace std;

namespace drizzled {

static int sort_keyuse(optimizer::KeyUse *a, optimizer::KeyUse *b);
static COND *build_equal_items(Session *session, COND *cond,
                               COND_EQUAL *inherited,
                               List<TableList> *join_list,
                               COND_EQUAL **cond_equal_ref);

static Item* part_of_refkey(Table *form,Field *field);
static bool cmp_buffer_with_ref(JoinTable *tab);
static void change_cond_ref_to_const(Session *session,
                                     list<COND_CMP>& save_list,
                                     Item *and_father,
                                     Item *cond,
                                     Item *field,
                                     Item *value);
static void copy_blobs(Field **ptr);

static bool eval_const_cond(COND *cond)
{
    return ((Item_func*) cond)->val_int() ? true : false;
}

/*
  This is used to mark equalities that were made from i-th IN-equality.
  We limit semi-join InsideOut optimization to handling max 64 inequalities,
  The following variable occupies 64 addresses.
*/
const char *subq_sj_cond_name=
  "0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef-sj-cond";

static void copy_blobs(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      ((Field_blob *) (*ptr))->copy();
  }
}

/**
  This handles SELECT with and without UNION.
*/
bool handle_select(Session *session, LEX *lex, select_result *result,
                   uint64_t setup_tables_done_option)
{
  bool res;
  Select_Lex *select_lex= &lex->select_lex;
  DRIZZLE_SELECT_START(session->getQueryString()->c_str());

  if (select_lex->master_unit()->is_union() ||
      select_lex->master_unit()->fake_select_lex)
  {
    res= drizzle_union(session, lex, result, &lex->unit,
		       setup_tables_done_option);
  }
  else
  {
    Select_Lex_Unit *unit= &lex->unit;
    unit->set_limit(unit->global_parameters);
    session->session_marker= 0;
    /*
      'options' of select_query will be set in JOIN, as far as JOIN for
      every PS/SP execution new, we will not need reset this flag if
      setup_tables_done_option changed for next rexecution
    */
    res= select_query(session,
                      &select_lex->ref_pointer_array,
		      (TableList*) select_lex->table_list.first,
		      select_lex->with_wild,
                      select_lex->item_list,
		      select_lex->where,
		      select_lex->order_list.size() +
		      select_lex->group_list.size(),
		      (Order*) select_lex->order_list.first,
		      (Order*) select_lex->group_list.first,
		      select_lex->having,
		      select_lex->options | session->options |
                      setup_tables_done_option,
		      result, unit, select_lex);
  }
  res|= session->is_error();
  if (unlikely(res))
    result->abort();

  DRIZZLE_SELECT_DONE(res, session->limit_found_rows);
  return res;
}

/*
  Fix fields referenced from inner selects.

  SYNOPSIS
    fix_inner_refs()
    session               Thread handle
    all_fields        List of all fields used in select
    select            Current select
    ref_pointer_array Array of references to Items used in current select

  DESCRIPTION
    The function serves 3 purposes - adds fields referenced from inner
    selects to the current select list, resolves which class to use
    to access referenced item (Item_ref of Item_direct_ref) and fixes
    references (Item_ref objects) to these fields.

    If a field isn't already in the select list and the ref_pointer_array
    is provided then it is added to the all_fields list and the pointer to
    it is saved in the ref_pointer_array.

    The class to access the outer field is determined by the following rules:
    1. If the outer field isn't used under an aggregate function
      then the Item_ref class should be used.
    2. If the outer field is used under an aggregate function and this
      function is aggregated in the select where the outer field was
      resolved or in some more inner select then the Item_direct_ref
      class should be used.
    The resolution is done here and not at the fix_fields() stage as
    it can be done only after sum functions are fixed and pulled up to
    selects where they are have to be aggregated.
    When the class is chosen it substitutes the original field in the
    Item_outer_ref object.

    After this we proceed with fixing references (Item_outer_ref objects) to
    this field from inner subqueries.

  RETURN
    true  an error occured
    false ok
*/
bool fix_inner_refs(Session *session,
                    List<Item> &all_fields,
                    Select_Lex *select,
                    Item **ref_pointer_array)
{
  Item_outer_ref *ref;
  bool res= false;
  bool direct_ref= false;

  List<Item_outer_ref>::iterator ref_it(select->inner_refs_list.begin());
  while ((ref= ref_it++))
  {
    Item *item= ref->outer_ref;
    Item **item_ref= ref->ref;
    Item_ref *new_ref;
    /*
      @todo this field item already might be present in the select list.
      In this case instead of adding new field item we could use an
      existing one. The change will lead to less operations for copying fields,
      smaller temporary tables and less data passed through filesort.
    */
    if (ref_pointer_array && !ref->found_in_select_list)
    {
      int el= all_fields.size();
      ref_pointer_array[el]= item;
      /* Add the field item to the select list of the current select. */
      all_fields.push_front(item);
      /*
        If it's needed reset each Item_ref item that refers this field with
        a new reference taken from ref_pointer_array.
      */
      item_ref= ref_pointer_array + el;
    }

    if (ref->in_sum_func)
    {
      Item_sum *sum_func;
      if (ref->in_sum_func->nest_level > select->nest_level)
        direct_ref= true;
      else
      {
        for (sum_func= ref->in_sum_func; sum_func &&
             sum_func->aggr_level >= select->nest_level;
             sum_func= sum_func->in_sum_func)
        {
          if (sum_func->aggr_level == select->nest_level)
          {
            direct_ref= true;
            break;
          }
        }
      }
    }
    new_ref= direct_ref ?
              new Item_direct_ref(ref->context, item_ref, ref->table_name,
                          ref->field_name, ref->alias_name_used) :
              new Item_ref(ref->context, item_ref, ref->table_name,
                          ref->field_name, ref->alias_name_used);
    ref->outer_ref= new_ref;
    ref->ref= &ref->outer_ref;

    if (!ref->fixed && ref->fix_fields(session, 0))
      return true;
    session->used_tables|= item->used_tables();
  }
  return res;
}

/*****************************************************************************
  Check fields, find best join, do the select and output fields.
  select_query assumes that all tables are already opened
*****************************************************************************/

/*
  Index lookup-based subquery: save some flags for EXPLAIN output

  SYNOPSIS
    save_index_subquery_explain_info()
      join_tab  Subquery's join tab (there is only one as index lookup is
                only used for subqueries that are single-table SELECTs)
      where     Subquery's WHERE clause

  DESCRIPTION
    For index lookup-based subquery (i.e. one executed with
    subselect_uniquesubquery_engine or subselect_indexsubquery_engine),
    check its EXPLAIN output row should contain
      "Using index" (TAB_INFO_FULL_SCAN_ON_NULL)
      "Using Where" (TAB_INFO_USING_WHERE)
      "Full scan on NULL key" (TAB_INFO_FULL_SCAN_ON_NULL)
    and set appropriate flags in join_tab->packed_info.
*/
void save_index_subquery_explain_info(JoinTable *join_tab, Item* where)
{
  join_tab->packed_info= TAB_INFO_HAVE_VALUE;
  if (join_tab->table->covering_keys.test(join_tab->ref.key))
    join_tab->packed_info |= TAB_INFO_USING_INDEX;
  if (where)
    join_tab->packed_info |= TAB_INFO_USING_WHERE;
  for (uint32_t i = 0; i < join_tab->ref.key_parts; i++)
  {
    if (join_tab->ref.cond_guards[i])
    {
      join_tab->packed_info |= TAB_INFO_FULL_SCAN_ON_NULL;
      break;
    }
  }
}

/**
  An entry point to single-unit select (a select without UNION).

  @param session                  thread Cursor
  @param rref_pointer_array   a reference to ref_pointer_array of
                              the top-level select_lex for this query
  @param tables               list of all tables used in this query.
                              The tables have been pre-opened.
  @param wild_num             number of wildcards used in the top level
                              select of this query.
                              For example statement
                              SELECT *, t1.*, catalog.t2.* FROM t0, t1, t2;
                              has 3 wildcards.
  @param fields               list of items in SELECT list of the top-level
                              select
                              e.g. SELECT a, b, c FROM t1 will have Item_field
                              for a, b and c in this list.
  @param conds                top level item of an expression representing
                              WHERE clause of the top level select
  @param og_num               total number of ORDER BY and GROUP BY clauses
                              arguments
  @param order                linked list of ORDER BY agruments
  @param group                linked list of GROUP BY arguments
  @param having               top level item of HAVING expression
  @param select_options       select options (BIG_RESULT, etc)
  @param result               an instance of result set handling class.
                              This object is responsible for send result
                              set rows to the client or inserting them
                              into a table.
  @param select_lex           the only Select_Lex of this query
  @param unit                 top-level UNIT of this query
                              UNIT is an artificial object created by the
                              parser for every SELECT clause.
                              e.g.
                              SELECT * FROM t1 WHERE a1 IN (SELECT * FROM t2)
                              has 2 unions.

  @retval
    false  success
  @retval
    true   an error
*/
bool select_query(Session *session,
                  Item ***rref_pointer_array,
                  TableList *tables,
                  uint32_t wild_num,
                  List<Item> &fields,
                  COND *conds,
                  uint32_t og_num,
                  Order *order,
                  Order *group,
                  Item *having,
                  uint64_t select_options,
                  select_result *result,
                  Select_Lex_Unit *unit,
                  Select_Lex *select_lex)
{
  bool err;
  bool free_join= 1;

  select_lex->context.resolve_in_select_list= true;
  Join *join;
  if (select_lex->join != 0)
  {
    join= select_lex->join;
    /*
      is it single SELECT in derived table, called in derived table
      creation
    */
    if (select_lex->linkage != DERIVED_TABLE_TYPE ||
        (select_options & SELECT_DESCRIBE))
    {
      if (select_lex->linkage != GLOBAL_OPTIONS_TYPE)
      {
        //here is EXPLAIN of subselect or derived table
        if (join->change_result(result))
        {
          return true;
        }
      }
      else
      {
        if ((err= join->prepare(rref_pointer_array, tables, wild_num,
                               conds, og_num, order, group, having, select_lex, unit)))
        {
          goto err;
        }
      }
    }
    free_join= 0;
    join->select_options= select_options;
  }
  else
  {
    join= new Join(session, fields, select_options, result);
    session->set_proc_info("init");
    session->used_tables=0;                         // Updated by setup_fields
    if ((err= join->prepare(rref_pointer_array, tables, wild_num, conds, og_num, order, group, having, select_lex, unit)))
      goto err;
  }

  err= join->optimize();
  if (err)
  {
    goto err; // 1
  }

  if (session->lex().describe & DESCRIBE_EXTENDED)
  {
    join->conds_history= join->conds;
    join->having_history= (join->having?join->having:join->tmp_having);
  }

  if (session->is_error())
    goto err;

  join->exec();

  if (session->lex().describe & DESCRIBE_EXTENDED)
  {
    select_lex->where= join->conds_history;
    select_lex->having= join->having_history;
  }

err:
  if (free_join)
  {
    session->set_proc_info("end");
    err|= select_lex->cleanup();
    return(err || session->is_error());
  }
  return(join->error);
}

inline Item *and_items(Item* cond, Item *item)
{
  return (cond? (new Item_cond_and(cond, item)) : item);
}

/*****************************************************************************
  Create JoinTableS, make a guess about the table types,
  Approximate how many records will be used in each table
*****************************************************************************/
ha_rows get_quick_record_count(Session *session, optimizer::SqlSelect *select, Table *table, const key_map *keys,ha_rows limit)
{
  int error;
  if (check_stack_overrun(session, STACK_MIN_SIZE, NULL))
    return 0;                           // Fatal error flag is set
  if (select)
  {
    select->head=table;
    table->reginfo.impossible_range=0;
    if ((error= select->test_quick_select(session, *(key_map *)keys,(table_map) 0,
                                          limit, 0, false)) == 1)
      return(select->quick->records);
    if (error == -1)
    {
      table->reginfo.impossible_range=1;
      return 0;
    }
  }
  return(HA_POS_ERROR);			/* This shouldn't happend */
}

/*****************************************************************************
  Check with keys are used and with tables references with tables
  Updates in stat:
	  keys	     Bitmap of all used keys
	  const_keys Bitmap of all keys with may be used with quick_select
	  keyuse     Pointer to possible keys
*****************************************************************************/


/**
  Add all keys with uses 'field' for some keypart.

  If field->and_level != and_level then only mark key_part as const_part.
*/
uint32_t max_part_bit(key_part_map bits)
{
  uint32_t found;
  for (found=0; bits & 1 ; found++,bits>>=1) ;
  return found;
}

static int sort_keyuse(optimizer::KeyUse *a, optimizer::KeyUse *b)
{
  int res;
  if (a->getTable()->tablenr != b->getTable()->tablenr)
    return static_cast<int>((a->getTable()->tablenr - b->getTable()->tablenr));
  if (a->getKey() != b->getKey())
    return static_cast<int>((a->getKey() - b->getKey()));
  if (a->getKeypart() != b->getKeypart())
    return static_cast<int>((a->getKeypart() - b->getKeypart()));
  // Place const values before other ones
  if ((res= test((a->getUsedTables() & ~OUTER_REF_TABLE_BIT)) -
       test((b->getUsedTables() & ~OUTER_REF_TABLE_BIT))))
    return res;
  /* Place rows that are not 'OPTIMIZE_REF_OR_NULL' first */
  return static_cast<int>(((a->getOptimizeFlags() & KEY_OPTIMIZE_REF_OR_NULL) -
		          (b->getOptimizeFlags() & KEY_OPTIMIZE_REF_OR_NULL)));
}


/**
  Update keyuse array with all possible keys we can use to fetch rows.

  @param       session
  @param[out]  keyuse         Put here ordered array of KeyUse structures
  @param       join_tab       Array in tablenr_order
  @param       tables         Number of tables in join
  @param       cond           WHERE condition (note that the function analyzes
                              join_tab[i]->on_expr too)
  @param       normal_tables  Tables not inner w.r.t some outer join (ones
                              for which we can make ref access based the WHERE
                              clause)
  @param       select_lex     current SELECT
  @param[out]  sargables      std::vector of found sargable candidates

   @retval
     0  OK
   @retval
     1  Out of memory.
*/
void update_ref_and_keys(Session *session,
                         DYNAMIC_ARRAY *keyuse,
                         JoinTable *join_tab,
                         uint32_t tables,
                         COND *cond,
                         COND_EQUAL *,
                         table_map normal_tables,
                         Select_Lex *select_lex,
                         vector<optimizer::SargableParam> &sargables)
{
  uint32_t m= max(select_lex->max_equal_elems,(uint32_t)1);

  /*
    All predicates that are used to fill arrays of KeyField
    and SargableParam classes have at most 2 arguments
    except BETWEEN predicates that have 3 arguments and
    IN predicates.
    This any predicate if it's not BETWEEN/IN can be used
    directly to fill at most 2 array elements, either of KeyField
    or SargableParam type. For a BETWEEN predicate 3 elements
    can be filled as this predicate is considered as
    saragable with respect to each of its argument.
    An IN predicate can require at most 1 element as currently
    it is considered as sargable only for its first argument.
    Multiple equality can add  elements that are filled after
    substitution of field arguments by equal fields. There
    can be not more than select_lex->max_equal_elems such
    substitutions.
  */
  optimizer::KeyField* key_fields= new (session->mem) optimizer::KeyField[((session->lex().current_select->cond_count+1)*2 + session->lex().current_select->between_count)*m+1];
  uint and_level= 0;
  optimizer::KeyField* end, *field;
  field= end= key_fields;

  my_init_dynamic_array(keyuse, sizeof(optimizer::KeyUse), 20, 64);
  if (cond)
  {
    add_key_fields(join_tab->join, &end, &and_level, cond, normal_tables,
                   sargables);
    for (; field != end; field++)
    {
      add_key_part(keyuse, field);
      /* Mark that we can optimize LEFT JOIN */
      if (field->getValue()->type() == Item::NULL_ITEM &&
	  ! field->getField()->real_maybe_null())
      {
	field->getField()->getTable()->reginfo.not_exists_optimize= 1;
      }
    }
  }
  for (uint32_t i= 0; i < tables; i++)
  {
    /*
      Block the creation of keys for inner tables of outer joins.
      Here only the outer joins that can not be converted to
      inner joins are left and all nests that can be eliminated
      are flattened.
      In the future when we introduce conditional accesses
      for inner tables in outer joins these keys will be taken
      into account as well.
    */
    if (*join_tab[i].on_expr_ref)
      add_key_fields(join_tab->join, &end, &and_level,
                     *join_tab[i].on_expr_ref,
                     join_tab[i].table->map, sargables);
  }

  /* Process ON conditions for the nested joins */
  {
    List<TableList>::iterator li(join_tab->join->join_list->begin());
    TableList *table;
    while ((table= li++))
    {
      if (table->getNestedJoin())
        add_key_fields_for_nj(join_tab->join, table, &end, &and_level,
                              sargables);
    }
  }

  /* fill keyuse with found key parts */
  for ( ; field != end ; field++)
    add_key_part(keyuse,field);

  /*
    Sort the array of possible keys and remove the following key parts:
    - ref if there is a keypart which is a ref and a const.
      (e.g. if there is a key(a,b) and the clause is a=3 and b=7 and b=t2.d,
      then we skip the key part corresponding to b=t2.d)
    - keyparts without previous keyparts
      (e.g. if there is a key(a,b,c) but only b < 5 (or a=2 and c < 3) is
      used in the query, we drop the partial key parts from consideration).
  */
  if (keyuse->size())
  {
    optimizer::KeyUse key_end,*prev,*save_pos,*use;

    internal::my_qsort(keyuse->buffer,keyuse->size(),sizeof(optimizer::KeyUse),
                       (qsort_cmp) sort_keyuse);

    memset(&key_end, 0, sizeof(key_end)); /* Add for easy testing */
    keyuse->push_back(&key_end);

    use= save_pos= (optimizer::KeyUse*)keyuse->buffer;
    prev= &key_end;
    uint found_eq_constant= 0;
    {
      uint32_t i;

      for (i= 0; i < keyuse->size()-1; i++, use++)
      {
        if (! use->getUsedTables() && use->getOptimizeFlags() != KEY_OPTIMIZE_REF_OR_NULL)
          use->getTable()->const_key_parts[use->getKey()]|= use->getKeypartMap();
        if (use->getKey() == prev->getKey() && use->getTable() == prev->getTable())
        {
          if (prev->getKeypart() + 1 < use->getKeypart() ||
              ((prev->getKeypart() == use->getKeypart()) && found_eq_constant))
            continue;				/* remove */
        }
        else if (use->getKeypart() != 0)		// First found must be 0
          continue;

#ifdef HAVE_VALGRIND
        /* Valgrind complains about overlapped memcpy when save_pos==use. */
        if (save_pos != use)
#endif
          *save_pos= *use;
        prev=use;
        found_eq_constant= ! use->getUsedTables();
        /* Save ptr to first use */
        if (! use->getTable()->reginfo.join_tab->keyuse)
          use->getTable()->reginfo.join_tab->keyuse= save_pos;
        use->getTable()->reginfo.join_tab->checked_keys.set(use->getKey());
        save_pos++;
      }
      i= (uint32_t) (save_pos - (optimizer::KeyUse*) keyuse->buffer);
      reinterpret_cast<optimizer::KeyUse*>(keyuse->buffer)[i] = key_end;
      keyuse->set_size(i);
    }
  }
}

/**
  Update some values in keyuse for faster choose_plan() loop.
*/
void optimize_keyuse(Join *join, DYNAMIC_ARRAY *keyuse_array)
{
  optimizer::KeyUse* keyuse= (optimizer::KeyUse*)keyuse_array->buffer;
  for (optimizer::KeyUse* end= keyuse+ keyuse_array->size() ; keyuse < end ; keyuse++)
  {
    table_map map;
    /*
      If we find a ref, assume this table matches a proportional
      part of this table.
      For example 100 records matching a table with 5000 records
      gives 5000/100 = 50 records per key
      Constant tables are ignored.
      To avoid bad matches, we don't make ref_table_rows less than 100.
    */
    keyuse->setTableRows(~(ha_rows) 0); // If no ref
    if (keyuse->getUsedTables() & (map= (keyuse->getUsedTables() & ~join->const_table_map & ~OUTER_REF_TABLE_BIT)))
    {
      uint32_t tablenr;
      for (tablenr=0 ; ! (map & 1) ; map>>=1, tablenr++) ;
      if (map == 1)			// Only one table
      {
        Table *tmp_table=join->all_tables[tablenr];
        keyuse->setTableRows(max(tmp_table->cursor->stats.records, (ha_rows)100));
      }
    }
    /*
      Outer reference (external field) is constant for single executing
      of subquery
    */
    if (keyuse->getUsedTables() == OUTER_REF_TABLE_BIT)
      keyuse->setTableRows(1);
  }
}


/**
  Discover the indexes that can be used for GROUP BY or DISTINCT queries.

  If the query has a GROUP BY clause, find all indexes that contain all
  GROUP BY fields, and add those indexes to join->const_keys.

  If the query has a DISTINCT clause, find all indexes that contain all
  SELECT fields, and add those indexes to join->const_keys.
  This allows later on such queries to be processed by a
  QUICK_GROUP_MIN_MAX_SELECT.

  @param join
  @param join_tab

  @return
    None
*/
void add_group_and_distinct_keys(Join *join, JoinTable *join_tab)
{
  List<Item_field> indexed_fields;
  List<Item_field>::iterator indexed_fields_it(indexed_fields.begin());
  Order      *cur_group;
  Item_field *cur_item;
  key_map possible_keys(0);

  if (join->group_list)
  { /* Collect all query fields referenced in the GROUP clause. */
    for (cur_group= join->group_list; cur_group; cur_group= cur_group->next)
      (*cur_group->item)->walk(&Item::collect_item_field_processor, 0,
                               (unsigned char*) &indexed_fields);
  }
  else if (join->select_distinct)
  { /* Collect all query fields referenced in the SELECT clause. */
    List<Item> &select_items= join->fields_list;
    List<Item>::iterator select_items_it(select_items.begin());
    Item *item;
    while ((item= select_items_it++))
      item->walk(&Item::collect_item_field_processor, 0,
                 (unsigned char*) &indexed_fields);
  }
  else
    return;

  if (indexed_fields.size() == 0)
    return;

  /* Intersect the keys of all group fields. */
  cur_item= indexed_fields_it++;
  possible_keys|= cur_item->field->part_of_key;
  while ((cur_item= indexed_fields_it++))
  {
    possible_keys&= cur_item->field->part_of_key;
  }

  if (possible_keys.any())
    join_tab->const_keys|= possible_keys;
}

/**
  Compare two JoinTable objects based on the number of accessed records.

  @param ptr1 pointer to first JoinTable object
  @param ptr2 pointer to second JoinTable object

  NOTES
    The order relation implemented by join_tab_cmp() is not transitive,
    i.e. it is possible to choose such a, b and c that (a < b) && (b < c)
    but (c < a). This implies that result of a sort using the relation
    implemented by join_tab_cmp() depends on the order in which
    elements are compared, i.e. the result is implementation-specific.
    Example:
      a: dependent = 0x0 table->map = 0x1 found_records = 3 ptr = 0x907e6b0
      b: dependent = 0x0 table->map = 0x2 found_records = 3 ptr = 0x907e838
      c: dependent = 0x6 table->map = 0x10 found_records = 2 ptr = 0x907ecd0

  @retval
    1  if first is bigger
  @retval
    -1  if second is bigger
  @retval
    0  if equal
*/
int join_tab_cmp(const void* ptr1, const void* ptr2)
{
  JoinTable *jt1= *(JoinTable**) ptr1;
  JoinTable *jt2= *(JoinTable**) ptr2;

  if (jt1->dependent & jt2->table->map)
    return 1;
  if (jt2->dependent & jt1->table->map)
    return -1;
  if (jt1->found_records > jt2->found_records)
    return 1;
  if (jt1->found_records < jt2->found_records)
    return -1;
  return jt1 > jt2 ? 1 : (jt1 < jt2 ? -1 : 0);
}

/**
  Same as join_tab_cmp, but for use with SELECT_STRAIGHT_JOIN.
*/
int join_tab_cmp_straight(const void* ptr1, const void* ptr2)
{
  JoinTable *jt1= *(JoinTable**) ptr1;
  JoinTable *jt2= *(JoinTable**) ptr2;

  if (jt1->dependent & jt2->table->map)
    return 1;
  if (jt2->dependent & jt1->table->map)
    return -1;
  return jt1 > jt2 ? 1 : (jt1 < jt2 ? -1 : 0);
}

/**
  Find how much space the prevous read not const tables takes in cache.
*/
void calc_used_field_length(Session *, JoinTable *join_tab)
{
  uint32_t null_fields,blobs,fields,rec_length;
  Field **f_ptr,*field;

  null_fields= blobs= fields= rec_length=0;
  for (f_ptr=join_tab->table->getFields() ; (field= *f_ptr) ; f_ptr++)
  {
    if (field->isReadSet())
    {
      uint32_t flags=field->flags;
      fields++;
      rec_length+=field->pack_length();
      if (flags & BLOB_FLAG)
        blobs++;
      if (!(flags & NOT_NULL_FLAG))
        null_fields++;
    }
  }
  if (null_fields)
    rec_length+=(join_tab->table->getNullFields() + 7)/8;
  if (join_tab->table->maybe_null)
    rec_length+=sizeof(bool);
  if (blobs)
  {
    uint32_t blob_length=(uint32_t) (join_tab->table->cursor->stats.mean_rec_length-
                                     (join_tab->table->getRecordLength()- rec_length));
    rec_length+= max((uint32_t)4,blob_length);
  }
  join_tab->used_fields= fields;
  join_tab->used_fieldlength= rec_length;
  join_tab->used_blobs= blobs;
}

StoredKey *get_store_key(Session *session,
                         optimizer::KeyUse *keyuse,
                         table_map used_tables,
	                 KeyPartInfo *key_part,
                         unsigned char *key_buff,
                         uint32_t maybe_null)
{
  Item_ref *key_use_val= static_cast<Item_ref *>(keyuse->getVal());
  if (! ((~used_tables) & keyuse->getUsedTables())) // if const item
  {
    return new store_key_const_item(session,
				    key_part->field,
				    key_buff + maybe_null,
				    maybe_null ? key_buff : 0,
				    key_part->length,
				    key_use_val);
  }
  else if (key_use_val->type() == Item::FIELD_ITEM ||
           (key_use_val->type() == Item::REF_ITEM &&
            key_use_val->ref_type() == Item_ref::OUTER_REF &&
            (*(Item_ref**)((Item_ref*)key_use_val)->ref)->ref_type() == Item_ref::DIRECT_REF &&
            key_use_val->real_item()->type() == Item::FIELD_ITEM))
  {
    return new store_key_field(session,
			       key_part->field,
			       key_buff + maybe_null,
			       maybe_null ? key_buff : 0,
			       key_part->length,
			       ((Item_field*) key_use_val->real_item())->field,
			       key_use_val->full_name());
  }
  return new store_key_item(session,
			    key_part->field,
			    key_buff + maybe_null,
			    maybe_null ? key_buff : 0,
			    key_part->length,
			    key_use_val);
}

/**
  This function is only called for const items on fields which are keys.

  @return
    returns 1 if there was some conversion made when the field was stored.
*/
bool store_val_in_field(Field *field, Item *item, enum_check_fields check_flag)
{
  bool error;
  Table *table= field->getTable();
  Session *session= table->in_use;
  ha_rows cuted_fields=session->cuted_fields;

  /*
    we should restore old value of count_cuted_fields because
    store_val_in_field can be called from insert_query
    with select_insert, which make count_cuted_fields= 1
   */
  enum_check_fields old_count_cuted_fields= session->count_cuted_fields;
  session->count_cuted_fields= check_flag;
  error= item->save_in_field(field, 1);
  session->count_cuted_fields= old_count_cuted_fields;
  return error || cuted_fields != session->cuted_fields;
}

inline void add_cond_and_fix(Item **e1, Item *e2)
{
  if (*e1)
  {
    Item* res= new Item_cond_and(*e1, e2);
    *e1= res;
    res->quick_fix_field();
  }
  else
    *e1= e2;
}

bool create_ref_for_key(Join *join,
                        JoinTable *j,
                        optimizer::KeyUse *org_keyuse,
                        table_map used_tables)
{
  optimizer::KeyUse *keyuse= org_keyuse;
  Session  *session= join->session;
  uint32_t keyparts;
  uint32_t length;
  uint32_t key;
  Table *table= NULL;
  KeyInfo *keyinfo= NULL;

  /*  Use best key from find_best */
  table= j->table;
  key= keyuse->getKey();
  keyinfo= table->key_info + key;

  {
    keyparts= length= 0;
    uint32_t found_part_ref_or_null= 0;
    /*
      Calculate length for the used key
      Stop if there is a missing key part or when we find second key_part
      with KEY_OPTIMIZE_REF_OR_NULL
    */
    do
    {
      if (! (~used_tables & keyuse->getUsedTables()))
      {
        if (keyparts == keyuse->getKeypart() &&
            ! (found_part_ref_or_null & keyuse->getOptimizeFlags()))
        {
          keyparts++;
          length+= keyinfo->key_part[keyuse->getKeypart()].store_length;
          found_part_ref_or_null|= keyuse->getOptimizeFlags();
        }
      }
      keyuse++;
    } while (keyuse->getTable() == table && keyuse->getKey() == key);
  }

  /* set up fieldref */
  keyinfo=table->key_info+key;
  j->ref.key_parts=keyparts;
  j->ref.key_length=length;
  j->ref.key=(int) key;
  j->ref.key_buff= (unsigned char*) session->mem.calloc(ALIGN_SIZE(length)*2);
  j->ref.key_copy= new (session->mem) StoredKey*[keyparts + 1];
  j->ref.items= new (session->mem) Item*[keyparts];
  j->ref.cond_guards= new (session->mem) bool*[keyparts];
  j->ref.key_buff2=j->ref.key_buff+ALIGN_SIZE(length);
  j->ref.key_err=1;
  j->ref.null_rejecting= 0;
  j->ref.disable_cache= false;
  keyuse=org_keyuse;

  StoredKey **ref_key= j->ref.key_copy;
  unsigned char *key_buff= j->ref.key_buff, *null_ref_key= 0;
  bool keyuse_uses_no_tables= true;
  {
    for (uint32_t i= 0; i < keyparts; keyuse++, i++)
    {
      while (keyuse->getKeypart() != i ||
             ((~used_tables) & keyuse->getUsedTables()))
        keyuse++;       /* Skip other parts */

      uint32_t maybe_null= test(keyinfo->key_part[i].null_bit);
      j->ref.items[i]= keyuse->getVal();    // Save for cond removal
      j->ref.cond_guards[i]= keyuse->getConditionalGuard();
      if (keyuse->isNullRejected())
        j->ref.null_rejecting |= 1 << i;
      keyuse_uses_no_tables= keyuse_uses_no_tables && ! keyuse->getUsedTables();
      if (! keyuse->getUsedTables() &&  !(join->select_options & SELECT_DESCRIBE))
      {         // Compare against constant
        store_key_item tmp(session, keyinfo->key_part[i].field,
                           key_buff + maybe_null,
                           maybe_null ?  key_buff : 0,
                           keyinfo->key_part[i].length, keyuse->getVal());
        if (session->is_fatal_error)
          return true;
        tmp.copy();
      }
      else
        *ref_key++= get_store_key(session,
          keyuse,join->const_table_map,
          &keyinfo->key_part[i],
          key_buff, maybe_null);
      /*
        Remember if we are going to use REF_OR_NULL
        But only if field _really_ can be null i.e. we force AM_REF
        instead of AM_REF_OR_NULL in case if field can't be null
      */
      if ((keyuse->getOptimizeFlags() & KEY_OPTIMIZE_REF_OR_NULL) && maybe_null)
        null_ref_key= key_buff;
      key_buff+=keyinfo->key_part[i].store_length;
    }
  }
  *ref_key= 0;       // end_marker
  if (j->type == AM_CONST)
    j->table->const_table= 1;
  else if (((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) != HA_NOSAME) ||
           keyparts != keyinfo->key_parts || null_ref_key)
  {
    /* Must read with repeat */
    j->type= null_ref_key ? AM_REF_OR_NULL : AM_REF;
    j->ref.null_ref_key= null_ref_key;
  }
  else if (keyuse_uses_no_tables)
  {
    /*
      This happen if we are using a constant expression in the ON part
      of an LEFT JOIN.
      SELECT * FROM a LEFT JOIN b ON b.key=30
      Here we should not mark the table as a 'const' as a field may
      have a 'normal' value or a NULL value.
    */
    j->type= AM_CONST;
  }
  else
    j->type= AM_EQ_REF;
  return 0;
}

/**
  Add to join_tab->select_cond[i] "table.field IS NOT NULL" conditions
  we've inferred from ref/eq_ref access performed.

    This function is a part of "Early NULL-values filtering for ref access"
    optimization.

    Example of this optimization:
    For query SELECT * FROM t1,t2 WHERE t2.key=t1.field @n
    and plan " any-access(t1), ref(t2.key=t1.field) " @n
    add "t1.field IS NOT NULL" to t1's table condition. @n

    Description of the optimization:

      We look through equalities choosen to perform ref/eq_ref access,
      pick equalities that have form "tbl.part_of_key = othertbl.field"
      (where othertbl is a non-const table and othertbl.field may be NULL)
      and add them to conditions on correspoding tables (othertbl in this
      example).

      Exception from that is the case when referred_tab->join != join.
      I.e. don't add NOT NULL constraints from any embedded subquery.
      Consider this query:
      @code
      SELECT A.f2 FROM t1 LEFT JOIN t2 A ON A.f2 = f1
      WHERE A.f3=(SELECT MIN(f3) FROM  t2 C WHERE A.f4 = C.f4) OR A.f3 IS NULL;
      @endocde
      Here condition A.f3 IS NOT NULL is going to be added to the WHERE
      condition of the embedding query.
      Another example:
      SELECT * FROM t10, t11 WHERE (t10.a < 10 OR t10.a IS NULL)
      AND t11.b <=> t10.b AND (t11.a = (SELECT MAX(a) FROM t12
      WHERE t12.b = t10.a ));
      Here condition t10.a IS NOT NULL is going to be added.
      In both cases addition of NOT NULL condition will erroneously reject
      some rows of the result set.
      referred_tab->join != join constraint would disallow such additions.

      This optimization doesn't affect the choices that ref, range, or join
      optimizer make. This was intentional because this was added after 4.1
      was GA.

    Implementation overview
      1. update_ref_and_keys() accumulates info about null-rejecting
         predicates in in KeyField::null_rejecting
      1.1 add_key_part saves these to KeyUse.
      2. create_ref_for_key copies them to table_reference_st.
      3. add_not_null_conds adds "x IS NOT NULL" to join_tab->select_cond of
         appropiate JoinTable members.
*/
void add_not_null_conds(Join *join)
{
  for (uint32_t i= join->const_tables; i < join->tables; i++)
  {
    JoinTable *tab=join->join_tab+i;
    if ((tab->type == AM_REF || tab->type == AM_EQ_REF ||
         tab->type == AM_REF_OR_NULL) &&
        !tab->table->maybe_null)
    {
      for (uint32_t keypart= 0; keypart < tab->ref.key_parts; keypart++)
      {
        if (tab->ref.null_rejecting & (1 << keypart))
        {
          Item *item= tab->ref.items[keypart];
          Item *notnull;
          assert(item->type() == Item::FIELD_ITEM);
          Item_field *not_null_item= (Item_field*)item;
          JoinTable *referred_tab= not_null_item->field->getTable()->reginfo.join_tab;
          /*
            For UPDATE queries such as:
            UPDATE t1 SET t1.f2=(SELECT MAX(t2.f4) FROM t2 WHERE t2.f3=t1.f1);
            not_null_item is the t1.f1, but it's referred_tab is 0.
          */
          if (!referred_tab || referred_tab->join != join)
            continue;
          notnull= new Item_func_isnotnull(not_null_item);
          /*
            We need to do full fix_fields() call here in order to have correct
            notnull->const_item(). This is needed e.g. by test_quick_select
            when it is called from make_join_select after this function is
            called.
          */
          if (notnull->fix_fields(join->session, &notnull))
            return;
          add_cond_and_fix(&referred_tab->select_cond, notnull);
        }
      }
    }
  }
  return;
}

/**
  Build a predicate guarded by match variables for embedding outer joins.
  The function recursively adds guards for predicate cond
  assending from tab to the first inner table  next embedding
  nested outer join and so on until it reaches root_tab
  (root_tab can be 0).

  @param tab       the first inner table for most nested outer join
  @param cond      the predicate to be guarded (must be set)
  @param root_tab  the first inner table to stop

  @return
    -  pointer to the guarded predicate, if success
    -  0, otherwise
*/
COND *add_found_match_trig_cond(JoinTable *tab, COND *cond, JoinTable *root_tab)
{
  COND *tmp;
  assert(cond != 0);
  if (tab == root_tab)
    return cond;
  if ((tmp= add_found_match_trig_cond(tab->first_upper, cond, root_tab)))
    tmp= new Item_func_trig_cond(tmp, &tab->found);
  if (tmp)
  {
    tmp->quick_fix_field();
    tmp->update_used_tables();
  }
  return tmp;
}

/**
  cleanup JoinTable.
*/
void JoinTable::cleanup()
{
  safe_delete(select);
  safe_delete(quick);

  if (cache.buff)
  {
    size_t size= cache.end - cache.buff;
    global_join_buffer.sub(size);
    free(cache.buff);
  }
  cache.buff= 0;
  limit= 0;
  if (table)
  {
    if (table->key_read)
    {
      table->key_read= 0;
      table->cursor->extra(HA_EXTRA_NO_KEYREAD);
    }
    table->cursor->ha_index_or_rnd_end();
    /*
      We need to reset this for next select
      (Tested in part_of_refkey)
    */
    table->reginfo.join_tab= 0;
  }
  read_record.end_read_record();
}

bool only_eq_ref_tables(Join *join,Order *order,table_map tables)
{
  for (JoinTable **tab=join->map2table ; tables ; tab++, tables>>=1)
  {
    if (tables & 1 && !eq_ref_table(join, order, *tab))
      return 0;
  }
  return 1;
}

/**
  Remove the following expressions from ORDER BY and GROUP BY:
  Constant expressions @n
  Expression that only uses tables that are of type EQ_REF and the reference
  is in the order_st list or if all refereed tables are of the above type.

  In the following, the X field can be removed:
  @code
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t1.a,t2.X
  SELECT * FROM t1,t2,t3 WHERE t1.a=t2.a AND t2.b=t3.b ORDER BY t1.a,t3.X
  @endcode

  These can't be optimized:
  @code
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.X,t1.a
  SELECT * FROM t1,t2 WHERE t1.a=t2.a AND t1.b=t2.b ORDER BY t1.a,t2.c
  SELECT * FROM t1,t2 WHERE t1.a=t2.a ORDER BY t2.b,t1.a
  @endcode
*/
bool eq_ref_table(Join *join, Order *start_order, JoinTable *tab)
{
  if (tab->cached_eq_ref_table)			// If cached
    return tab->eq_ref_table;
  tab->cached_eq_ref_table=1;
  /* We can skip const tables only if not an outer table */
  if (tab->type == AM_CONST && !tab->first_inner)
    return (tab->eq_ref_table=1);
  if (tab->type != AM_EQ_REF || tab->table->maybe_null)
    return (tab->eq_ref_table=0);		// We must use this
  Item **ref_item=tab->ref.items;
  Item **end=ref_item+tab->ref.key_parts;
  uint32_t found=0;
  table_map map=tab->table->map;

  for (; ref_item != end ; ref_item++)
  {
    if (! (*ref_item)->const_item())
    {						// Not a const ref
      Order *order;
      for (order=start_order ; order ; order=order->next)
      {
        if ((*ref_item)->eq(order->item[0],0))
          break;
      }
      if (order)
      {
        found++;
        assert(!(order->used & map));
        order->used|=map;
        continue;				// Used in order_st BY
      }
      if (!only_eq_ref_tables(join,start_order, (*ref_item)->used_tables()))
        return (tab->eq_ref_table= 0);
    }
  }
  /* Check that there was no reference to table before sort order */
  for (; found && start_order ; start_order=start_order->next)
  {
    if (start_order->used & map)
    {
      found--;
      continue;
    }
    if (start_order->depend_map & map)
      return (tab->eq_ref_table= 0);
  }
  return tab->eq_ref_table= 1;
}

/**
  Find the multiple equality predicate containing a field.

  The function retrieves the multiple equalities accessed through
  the con_equal structure from current level and up looking for
  an equality containing field. It stops retrieval as soon as the equality
  is found and set up inherited_fl to true if it's found on upper levels.

  @param cond_equal          multiple equalities to search in
  @param field               field to look for
  @param[out] inherited_fl   set up to true if multiple equality is found
                             on upper levels (not on current level of
                             cond_equal)

  @return
    - Item_equal for the found multiple equality predicate if a success;
    - NULL otherwise.
*/
static Item_equal *find_item_equal(COND_EQUAL *cond_equal, Field *field, bool *inherited_fl)
{
  Item_equal *item= 0;
  bool in_upper_level= false;
  while (cond_equal)
  {
    List<Item_equal>::iterator li(cond_equal->current_level.begin());
    while ((item= li++))
    {
      if (item->contains(field))
        goto finish;
    }
    in_upper_level= true;
    cond_equal= cond_equal->upper_levels;
  }
  in_upper_level= false;
finish:
  *inherited_fl= in_upper_level;
  return item;
}

/**
  Check whether an equality can be used to build multiple equalities.

    This function first checks whether the equality (left_item=right_item)
    is a simple equality i.e. the one that equates a field with another field
    or a constant (field=field_item or field=const_item).
    If this is the case the function looks for a multiple equality
    in the lists referenced directly or indirectly by cond_equal inferring
    the given simple equality. If it doesn't find any, it builds a multiple
    equality that covers the predicate, i.e. the predicate can be inferred
    from this multiple equality.
    The built multiple equality could be obtained in such a way:
    create a binary  multiple equality equivalent to the predicate, then
    merge it, if possible, with one of old multiple equalities.
    This guarantees that the set of multiple equalities covering equality
    predicates will be minimal.

  EXAMPLE:
    For the where condition
    @code
      WHERE a=b AND b=c AND
            (b=2 OR f=e)
    @endcode
    the check_equality will be called for the following equality
    predicates a=b, b=c, b=2 and f=e.
    - For a=b it will be called with *cond_equal=(0,[]) and will transform
      *cond_equal into (0,[Item_equal(a,b)]).
    - For b=c it will be called with *cond_equal=(0,[Item_equal(a,b)])
      and will transform *cond_equal into CE=(0,[Item_equal(a,b,c)]).
    - For b=2 it will be called with *cond_equal=(ptr(CE),[])
      and will transform *cond_equal into (ptr(CE),[Item_equal(2,a,b,c)]).
    - For f=e it will be called with *cond_equal=(ptr(CE), [])
      and will transform *cond_equal into (ptr(CE),[Item_equal(f,e)]).

  @note
    Now only fields that have the same type definitions (verified by
    the Field::eq_def method) are placed to the same multiple equalities.
    Because of this some equality predicates are not eliminated and
    can be used in the constant propagation procedure.
    We could weeken the equlity test as soon as at least one of the
    equal fields is to be equal to a constant. It would require a
    more complicated implementation: we would have to store, in
    general case, its own constant for each fields from the multiple
    equality. But at the same time it would allow us to get rid
    of constant propagation completely: it would be done by the call
    to build_equal_items_for_cond.


    The implementation does not follow exactly the above rules to
    build a new multiple equality for the equality predicate.
    If it processes the equality of the form field1=field2, it
    looks for multiple equalities me1 containig field1 and me2 containing
    field2. If only one of them is found the fuction expands it with
    the lacking field. If multiple equalities for both fields are
    found they are merged. If both searches fail a new multiple equality
    containing just field1 and field2 is added to the existing
    multiple equalities.
    If the function processes the predicate of the form field1=const,
    it looks for a multiple equality containing field1. If found, the
    function checks the constant of the multiple equality. If the value
    is unknown, it is setup to const. Otherwise the value is compared with
    const and the evaluation of the equality predicate is performed.
    When expanding/merging equality predicates from the upper levels
    the function first copies them for the current level. It looks
    acceptable, as this happens rarely. The implementation without
    copying would be much more complicated.

  @param left_item   left term of the quality to be checked
  @param right_item  right term of the equality to be checked
  @param item        equality item if the equality originates from a condition
                     predicate, 0 if the equality is the result of row
                     elimination
  @param cond_equal  multiple equalities that must hold together with the
                     equality

  @retval
    true    if the predicate is a simple equality predicate to be used
    for building multiple equalities
  @retval
    false   otherwise
*/
static bool check_simple_equality(Item *left_item,
                                  Item *right_item,
                                  Item *item,
                                  COND_EQUAL *cond_equal)
{
  if (left_item->type() == Item::FIELD_ITEM &&
      right_item->type() == Item::FIELD_ITEM &&
      !((Item_field*)left_item)->depended_from &&
      !((Item_field*)right_item)->depended_from)
  {
    /* The predicate the form field1=field2 is processed */

    Field *left_field= ((Item_field*) left_item)->field;
    Field *right_field= ((Item_field*) right_item)->field;

    if (!left_field->eq_def(right_field))
      return false;

    /* Search for multiple equalities containing field1 and/or field2 */
    bool left_copyfl, right_copyfl;
    Item_equal *left_item_equal=
               find_item_equal(cond_equal, left_field, &left_copyfl);
    Item_equal *right_item_equal=
               find_item_equal(cond_equal, right_field, &right_copyfl);

    /* As (NULL=NULL) != true we can't just remove the predicate f=f */
    if (left_field->eq(right_field)) /* f = f */
      return (!(left_field->maybe_null() && !left_item_equal));

    if (left_item_equal && left_item_equal == right_item_equal)
    {
      /*
        The equality predicate is inference of one of the existing
        multiple equalities, i.e the condition is already covered
        by upper level equalities
      */
       return true;
    }

    bool copy_item_name= test(item && item->name >= subq_sj_cond_name &&
                              item->name < subq_sj_cond_name + 64);
    /* Copy the found multiple equalities at the current level if needed */
    if (left_copyfl)
    {
      /* left_item_equal of an upper level contains left_item */
      left_item_equal= new Item_equal(left_item_equal);
      cond_equal->current_level.push_back(left_item_equal);
      if (copy_item_name)
        left_item_equal->name = item->name;
    }
    if (right_copyfl)
    {
      /* right_item_equal of an upper level contains right_item */
      right_item_equal= new Item_equal(right_item_equal);
      cond_equal->current_level.push_back(right_item_equal);
      if (copy_item_name)
        right_item_equal->name = item->name;
    }

    if (left_item_equal)
    {
      /* left item was found in the current or one of the upper levels */
      if (! right_item_equal)
        left_item_equal->add((Item_field *) right_item);
      else
      {
        /* Merge two multiple equalities forming a new one */
        left_item_equal->merge(right_item_equal);
        /* Remove the merged multiple equality from the list */
        List<Item_equal>::iterator li(cond_equal->current_level.begin());
        while ((li++) != right_item_equal) {};
        li.remove();
      }
    }
    else
    {
      /* left item was not found neither the current nor in upper levels  */
      if (right_item_equal)
      {
        right_item_equal->add((Item_field *) left_item);
        if (copy_item_name)
          right_item_equal->name = item->name;
      }
      else
      {
        /* None of the fields was found in multiple equalities */
        Item_equal *item_equal= new Item_equal((Item_field *) left_item,
                                               (Item_field *) right_item);
        cond_equal->current_level.push_back(item_equal);
        if (copy_item_name)
          item_equal->name = item->name;
      }
    }
    return true;
  }

  {
    /* The predicate of the form field=const/const=field is processed */
    Item *const_item= 0;
    Item_field *field_item= 0;
    if (left_item->type() == Item::FIELD_ITEM &&
        !((Item_field*)left_item)->depended_from &&
        right_item->const_item())
    {
      field_item= (Item_field*) left_item;
      const_item= right_item;
    }
    else if (right_item->type() == Item::FIELD_ITEM &&
             !((Item_field*)right_item)->depended_from &&
             left_item->const_item())
    {
      field_item= (Item_field*) right_item;
      const_item= left_item;
    }

    if (const_item &&
        field_item->result_type() == const_item->result_type())
    {
      bool copyfl;

      if (field_item->result_type() == STRING_RESULT)
      {
        const charset_info_st * const cs= ((Field_str*) field_item->field)->charset();
        if (!item)
        {
          Item_func_eq *eq_item;
          eq_item= new Item_func_eq(left_item, right_item);
          eq_item->set_cmp_func();
          eq_item->quick_fix_field();
          item= eq_item;
        }
        if ((cs != ((Item_func *) item)->compare_collation()) ||
            !cs->coll->propagate(cs, 0, 0))
          return false;
      }

      Item_equal *item_equal = find_item_equal(cond_equal,
                                               field_item->field, &copyfl);
      if (copyfl)
      {
        item_equal= new Item_equal(item_equal);
        cond_equal->current_level.push_back(item_equal);
      }
      if (item_equal)
      {
        /*
          The flag cond_false will be set to 1 after this, if item_equal
          already contains a constant and its value is  not equal to
          the value of const_item.
        */
        item_equal->add(const_item);
      }
      else
      {
        item_equal= new Item_equal(const_item, field_item);
        cond_equal->current_level.push_back(item_equal);
      }
      return true;
    }
  }
  return false;
}

/**
  Convert row equalities into a conjunction of regular equalities.

    The function converts a row equality of the form (E1,...,En)=(E'1,...,E'n)
    into a list of equalities E1=E'1,...,En=E'n. For each of these equalities
    Ei=E'i the function checks whether it is a simple equality or a row
    equality. If it is a simple equality it is used to expand multiple
    equalities of cond_equal. If it is a row equality it converted to a
    sequence of equalities between row elements. If Ei=E'i is neither a
    simple equality nor a row equality the item for this predicate is added
    to eq_list.

  @param session        thread handle
  @param left_row   left term of the row equality to be processed
  @param right_row  right term of the row equality to be processed
  @param cond_equal multiple equalities that must hold together with the
                    predicate
  @param eq_list    results of conversions of row equalities that are not
                    simple enough to form multiple equalities

  @retval
    true    if conversion has succeeded (no fatal error)
  @retval
    false   otherwise
*/
static bool check_row_equality(Session *session,
                               Item *left_row,
                               Item_row *right_row,
                               COND_EQUAL *cond_equal,
                               List<Item>* eq_list)
{
  uint32_t n= left_row->cols();
  for (uint32_t i= 0 ; i < n; i++)
  {
    bool is_converted;
    Item *left_item= left_row->element_index(i);
    Item *right_item= right_row->element_index(i);
    if (left_item->type() == Item::ROW_ITEM &&
        right_item->type() == Item::ROW_ITEM)
    {
      is_converted= check_row_equality(session,
                                       (Item_row *) left_item,
                                       (Item_row *) right_item,
			               cond_equal, eq_list);
      if (!is_converted)
        session->lex().current_select->cond_count++;
    }
    else
    {
      is_converted= check_simple_equality(left_item, right_item, 0, cond_equal);
      session->lex().current_select->cond_count++;
    }

    if (!is_converted)
    {
      Item_func_eq *eq_item;
      eq_item= new Item_func_eq(left_item, right_item);
      eq_item->set_cmp_func();
      eq_item->quick_fix_field();
      eq_list->push_back(eq_item);
    }
  }
  return true;
}

/**
  Eliminate row equalities and form multiple equalities predicates.

    This function checks whether the item is a simple equality
    i.e. the one that equates a field with another field or a constant
    (field=field_item or field=constant_item), or, a row equality.
    For a simple equality the function looks for a multiple equality
    in the lists referenced directly or indirectly by cond_equal inferring
    the given simple equality. If it doesn't find any, it builds/expands
    multiple equality that covers the predicate.
    Row equalities are eliminated substituted for conjunctive regular
    equalities which are treated in the same way as original equality
    predicates.

  @param session        thread handle
  @param item       predicate to process
  @param cond_equal multiple equalities that must hold together with the
                    predicate
  @param eq_list    results of conversions of row equalities that are not
                    simple enough to form multiple equalities

  @retval
    true   if re-writing rules have been applied
  @retval
    false  otherwise, i.e.
           if the predicate is not an equality,
           or, if the equality is neither a simple one nor a row equality,
           or, if the procedure fails by a fatal error.
*/
static bool check_equality(Session *session, Item *item, COND_EQUAL *cond_equal, List<Item> *eq_list)
{
  if (item->type() == Item::FUNC_ITEM &&
         ((Item_func*) item)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item= ((Item_func*) item)->arguments()[0];
    Item *right_item= ((Item_func*) item)->arguments()[1];

    if (left_item->type() == Item::ROW_ITEM &&
        right_item->type() == Item::ROW_ITEM)
    {
      session->lex().current_select->cond_count--;
      return check_row_equality(session,
                                (Item_row *) left_item,
                                (Item_row *) right_item,
                                cond_equal, eq_list);
    }
    else
      return check_simple_equality(left_item, right_item, item, cond_equal);
  }
  return false;
}

/**
  Replace all equality predicates in a condition by multiple equality items.

    At each 'and' level the function detects items for equality predicates
    and replaced them by a set of multiple equality items of class Item_equal,
    taking into account inherited equalities from upper levels.
    If an equality predicate is used not in a conjunction it's just
    replaced by a multiple equality predicate.
    For each 'and' level the function set a pointer to the inherited
    multiple equalities in the cond_equal field of the associated
    object of the type Item_cond_and.
    The function also traverses the cond tree and and for each field reference
    sets a pointer to the multiple equality item containing the field, if there
    is any. If this multiple equality equates fields to a constant the
    function replaces the field reference by the constant in the cases
    when the field is not of a string type or when the field reference is
    just an argument of a comparison predicate.
    The function also determines the maximum number of members in
    equality lists of each Item_cond_and object assigning it to
    session->lex().current_select->max_equal_elems.

  @note
    Multiple equality predicate =(f1,..fn) is equivalent to the conjuction of
    f1=f2, .., fn-1=fn. It substitutes any inference from these
    equality predicates that is equivalent to the conjunction.
    Thus, =(a1,a2,a3) can substitute for ((a1=a3) AND (a2=a3) AND (a2=a1)) as
    it is equivalent to ((a1=a2) AND (a2=a3)).
    The function always makes a substitution of all equality predicates occured
    in a conjuction for a minimal set of multiple equality predicates.
    This set can be considered as a canonical representation of the
    sub-conjunction of the equality predicates.
    E.g. (t1.a=t2.b AND t2.b>5 AND t1.a=t3.c) is replaced by
    (=(t1.a,t2.b,t3.c) AND t2.b>5), not by
    (=(t1.a,t2.b) AND =(t1.a,t3.c) AND t2.b>5);
    while (t1.a=t2.b AND t2.b>5 AND t3.c=t4.d) is replaced by
    (=(t1.a,t2.b) AND =(t3.c=t4.d) AND t2.b>5),
    but if additionally =(t4.d,t2.b) is inherited, it
    will be replaced by (=(t1.a,t2.b,t3.c,t4.d) AND t2.b>5)

    The function performs the substitution in a recursive descent by
    the condtion tree, passing to the next AND level a chain of multiple
    equality predicates which have been built at the upper levels.
    The Item_equal items built at the level are attached to other
    non-equality conjucts as a sublist. The pointer to the inherited
    multiple equalities is saved in the and condition object (Item_cond_and).
    This chain allows us for any field reference occurence easyly to find a
    multiple equality that must be held for this occurence.
    For each AND level we do the following:
    - scan it for all equality predicate (=) items
    - join them into disjoint Item_equal() groups
    - process the included OR conditions recursively to do the same for
      lower AND levels.

    We need to do things in this order as lower AND levels need to know about
    all possible Item_equal objects in upper levels.

  @param session        thread handle
  @param cond       condition(expression) where to make replacement
  @param inherited  path to all inherited multiple equality items

  @return
    pointer to the transformed condition
*/
static COND *build_equal_items_for_cond(Session *session, COND *cond, COND_EQUAL *inherited)
{
  Item_equal *item_equal;
  COND_EQUAL cond_equal;
  cond_equal.upper_levels= inherited;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item> eq_list;
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List<Item> *args= ((Item_cond*) cond)->argument_list();

    List<Item>::iterator li(args->begin());
    Item *item;

    if (and_level)
    {
      /*
         Retrieve all conjucts of this level detecting the equality
         that are subject to substitution by multiple equality items and
         removing each such predicate from the conjunction after having
         found/created a multiple equality whose inference the predicate is.
     */
      while ((item= li++))
      {
        /*
          PS/SP note: we can safely remove a node from AND-OR
          structure here because it's restored before each
          re-execution of any prepared statement/stored procedure.
        */
        if (check_equality(session, item, &cond_equal, &eq_list))
          li.remove();
      }

      List<Item_equal>::iterator it(cond_equal.current_level.begin());
      while ((item_equal= it++))
      {
        item_equal->fix_length_and_dec();
        item_equal->update_used_tables();
        set_if_bigger(session->lex().current_select->max_equal_elems,
                      item_equal->members());
      }

      ((Item_cond_and*)cond)->cond_equal= cond_equal;
      inherited= &(((Item_cond_and*)cond)->cond_equal);
    }
    /*
       Make replacement of equality predicates for lower levels
       of the condition expression.
    */
    li= args->begin();
    while ((item= li++))
    {
      Item *new_item;
      if ((new_item= build_equal_items_for_cond(session, item, inherited)) != item)
      {
        /* This replacement happens only for standalone equalities */
        /*
          This is ok with PS/SP as the replacement is done for
          arguments of an AND/OR item, which are restored for each
          execution of PS/SP.
        */
        li.replace(new_item);
      }
    }
    if (and_level)
    {
      args->concat(&eq_list);
      args->concat((List<Item> *)&cond_equal.current_level);
    }
  }
  else if (cond->type() == Item::FUNC_ITEM)
  {
    List<Item> eq_list;
    /*
      If an equality predicate forms the whole and level,
      we call it standalone equality and it's processed here.
      E.g. in the following where condition
      WHERE a=5 AND (b=5 or a=c)
      (b=5) and (a=c) are standalone equalities.
      In general we can't leave alone standalone eqalities:
      for WHERE a=b AND c=d AND (b=c OR d=5)
      b=c is replaced by =(a,b,c,d).
     */
    if (check_equality(session, cond, &cond_equal, &eq_list))
    {
      int n= cond_equal.current_level.size() + eq_list.size();
      if (n == 0)
        return new Item_int((int64_t) 1,1);
      else if (n == 1)
      {
        if ((item_equal= cond_equal.current_level.pop()))
        {
          item_equal->fix_length_and_dec();
          item_equal->update_used_tables();
        }
        else
          item_equal= (Item_equal *) eq_list.pop();
        set_if_bigger(session->lex().current_select->max_equal_elems,
                      item_equal->members());
        return item_equal;
      }
      else
      {
        /*
          Here a new AND level must be created. It can happen only
          when a row equality is processed as a standalone predicate.
        */
        Item_cond_and *and_cond= new Item_cond_and(eq_list);
        and_cond->quick_fix_field();
        List<Item> *args= and_cond->argument_list();
        List<Item_equal>::iterator it(cond_equal.current_level.begin());
        while ((item_equal= it++))
        {
          item_equal->fix_length_and_dec();
          item_equal->update_used_tables();
          set_if_bigger(session->lex().current_select->max_equal_elems,
                        item_equal->members());
        }
        and_cond->cond_equal= cond_equal;
        args->concat((List<Item> *)&cond_equal.current_level);

        return and_cond;
      }
    }
    /*
      For each field reference in cond, not from equal item predicates,
      set a pointer to the multiple equality it belongs to (if there is any)
      as soon the field is not of a string type or the field reference is
      an argument of a comparison predicate.
    */
    unsigned char *is_subst_valid= (unsigned char *) 1;
    cond= cond->compile(&Item::subst_argument_checker,
                        &is_subst_valid,
                        &Item::equal_fields_propagator,
                        (unsigned char *) inherited);
    cond->update_used_tables();
  }
  return cond;
}

/**
  Build multiple equalities for a condition and all on expressions that
  inherit these multiple equalities.

    The function first applies the build_equal_items_for_cond function
    to build all multiple equalities for condition cond utilizing equalities
    referred through the parameter inherited. The extended set of
    equalities is returned in the structure referred by the cond_equal_ref
    parameter. After this the function calls itself recursively for
    all on expressions whose direct references can be found in join_list
    and who inherit directly the multiple equalities just having built.

  @note
    The on expression used in an outer join operation inherits all equalities
    from the on expression of the embedding join, if there is any, or
    otherwise - from the where condition.
    This fact is not obvious, but presumably can be proved.
    Consider the following query:
    @code
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t1.a=t3.a AND t2.a=t4.a
        WHERE t1.a=t2.a;
    @endcode
    If the on expression in the query inherits =(t1.a,t2.a), then we
    can build the multiple equality =(t1.a,t2.a,t3.a,t4.a) that infers
    the equality t3.a=t4.a. Although the on expression
    t1.a=t3.a AND t2.a=t4.a AND t3.a=t4.a is not equivalent to the one
    in the query the latter can be replaced by the former: the new query
    will return the same result set as the original one.

    Interesting that multiple equality =(t1.a,t2.a,t3.a,t4.a) allows us
    to use t1.a=t3.a AND t3.a=t4.a under the on condition:
    @code
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t1.a=t3.a AND t3.a=t4.a
        WHERE t1.a=t2.a
    @endcode
    This query equivalent to:
    @code
      SELECT * FROM (t1 LEFT JOIN (t3,t4) ON t1.a=t3.a AND t3.a=t4.a),t2
        WHERE t1.a=t2.a
    @endcode
    Similarly the original query can be rewritten to the query:
    @code
      SELECT * FROM (t1,t2) LEFT JOIN (t3,t4) ON t2.a=t4.a AND t3.a=t4.a
        WHERE t1.a=t2.a
    @endcode
    that is equivalent to:
    @code
      SELECT * FROM (t2 LEFT JOIN (t3,t4)ON t2.a=t4.a AND t3.a=t4.a), t1
        WHERE t1.a=t2.a
    @endcode
    Thus, applying equalities from the where condition we basically
    can get more freedom in performing join operations.
    Althogh we don't use this property now, it probably makes sense to use
    it in the future.
  @param session		      Thread Cursor
  @param cond                condition to build the multiple equalities for
  @param inherited           path to all inherited multiple equality items
  @param join_list           list of join tables to which the condition
                             refers to
  @param[out] cond_equal_ref pointer to the structure to place built
                             equalities in

  @return
    pointer to the transformed condition containing multiple equalities
*/
static COND *build_equal_items(Session *session, COND *cond,
                               COND_EQUAL *inherited,
                               List<TableList> *join_list,
                               COND_EQUAL **cond_equal_ref)
{
  COND_EQUAL *cond_equal= 0;

  if (cond)
  {
    cond= build_equal_items_for_cond(session, cond, inherited);
    cond->update_used_tables();
    if (cond->type() == Item::COND_ITEM &&
        ((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
      cond_equal= &((Item_cond_and*) cond)->cond_equal;
    else if (cond->type() == Item::FUNC_ITEM &&
             ((Item_cond*) cond)->functype() == Item_func::MULT_EQUAL_FUNC)
    {
      cond_equal= new COND_EQUAL;
      cond_equal->current_level.push_back((Item_equal *) cond);
    }
  }
  if (cond_equal)
  {
    cond_equal->upper_levels= inherited;
    inherited= cond_equal;
  }
  *cond_equal_ref= cond_equal;

  if (join_list)
  {
    TableList *table;
    List<TableList>::iterator li(join_list->begin());

    while ((table= li++))
    {
      if (table->on_expr)
      {
        List<TableList> *nested_join_list= table->getNestedJoin() ?
          &table->getNestedJoin()->join_list : NULL;
        /*
          We can modify table->on_expr because its old value will
          be restored before re-execution of PS/SP.
        */
        table->on_expr= build_equal_items(session, table->on_expr, inherited,
                                          nested_join_list,
                                          &table->cond_equal);
      }
    }
  }

  return cond;
}

/**
  Compare field items by table order in the execution plan.

    field1 considered as better than field2 if the table containing
    field1 is accessed earlier than the table containing field2.
    The function finds out what of two fields is better according
    this criteria.

  @param field1          first field item to compare
  @param field2          second field item to compare
  @param table_join_idx  index to tables determining table order

  @retval
    1  if field1 is better than field2
  @retval
    -1  if field2 is better than field1
  @retval
    0  otherwise
*/
static int compare_fields_by_table_order(Item_field *field1,
                                         Item_field *field2,
                                         void *table_join_idx)
{
  int cmp= 0;
  bool outer_ref= 0;
  if (field2->used_tables() & OUTER_REF_TABLE_BIT)
  {
    outer_ref= 1;
    cmp= -1;
  }
  if (field2->used_tables() & OUTER_REF_TABLE_BIT)
  {
    outer_ref= 1;
    cmp++;
  }
  if (outer_ref)
    return cmp;
  JoinTable **idx= (JoinTable **) table_join_idx;
  cmp= idx[field2->field->getTable()->tablenr]-idx[field1->field->getTable()->tablenr];
  return cmp < 0 ? -1 : (cmp ? 1 : 0);
}

/**
  Generate minimal set of simple equalities equivalent to a multiple equality.

    The function retrieves the fields of the multiple equality item
    item_equal and  for each field f:
    - if item_equal contains const it generates the equality f=const_item;
    - otherwise, if f is not the first field, generates the equality
      f=item_equal->get_first().
    All generated equality are added to the cond conjunction.

  @param cond            condition to add the generated equality to
  @param upper_levels    structure to access multiple equality of upper levels
  @param item_equal      multiple equality to generate simple equality from

  @note
    Before generating an equality function checks that it has not
    been generated for multiple equalities of the upper levels.
    E.g. for the following where condition
    WHERE a=5 AND ((a=b AND b=c) OR  c>4)
    the upper level AND condition will contain =(5,a),
    while the lower level AND condition will contain =(5,a,b,c).
    When splitting =(5,a,b,c) into a separate equality predicates
    we should omit 5=a, as we have it already in the upper level.
    The following where condition gives us a more complicated case:
    WHERE t1.a=t2.b AND t3.c=t4.d AND (t2.b=t3.c OR t4.e>5 ...) AND ...
    Given the tables are accessed in the order t1->t2->t3->t4 for
    the selected query execution plan the lower level multiple
    equality =(t1.a,t2.b,t3.c,t4.d) formally  should be converted to
    t1.a=t2.b AND t1.a=t3.c AND t1.a=t4.d. But t1.a=t2.a will be
    generated for the upper level. Also t3.c=t4.d will be generated there.
    So only t1.a=t3.c should be left in the lower level.
    If cond is equal to 0, then not more then one equality is generated
    and a pointer to it is returned as the result of the function.

  @return
    - The condition with generated simple equalities or
    a pointer to the simple generated equality, if success.
    - 0, otherwise.
*/
static Item *eliminate_item_equal(COND *cond, COND_EQUAL *upper_levels, Item_equal *item_equal)
{
  List<Item> eq_list;
  Item_func_eq *eq_item= 0;
  if (((Item *) item_equal)->const_item() && !item_equal->val_int())
    return new Item_int((int64_t) 0,1);
  Item *item_const= item_equal->get_const();
  Item_equal_iterator it(item_equal->begin());
  Item *head;
  if (item_const)
    head= item_const;
  else
  {
    head= item_equal->get_first();
    it++;
  }
  Item_field *item_field;
  while ((item_field= it++))
  {
    Item_equal *upper= item_field->find_item_equal(upper_levels);
    Item_field *item= item_field;
    if (upper)
    {
      if (item_const && upper->get_const())
        item= 0;
      else
      {
        Item_equal_iterator li(item_equal->begin());
        while ((item= li++) != item_field)
        {
          if (item->find_item_equal(upper_levels) == upper)
            break;
        }
      }
    }
    if (item == item_field)
    {
      if (eq_item)
        eq_list.push_back(eq_item);
      eq_item= new Item_func_eq(item_field, head);
      if (!eq_item)
        return 0;
      eq_item->set_cmp_func();
      eq_item->quick_fix_field();
   }
  }

  if (!cond && !&eq_list.front())
  {
    if (!eq_item)
      return new Item_int((int64_t) 1,1);
    return eq_item;
  }

  if (eq_item)
    eq_list.push_back(eq_item);
  if (!cond)
    cond= new Item_cond_and(eq_list);
  else
  {
    assert(cond->type() == Item::COND_ITEM);
    ((Item_cond *) cond)->add_at_head(&eq_list);
  }

  cond->quick_fix_field();
  cond->update_used_tables();

  return cond;
}

/**
  Substitute every field reference in a condition by the best equal field
  and eliminate all multiple equality predicates.

    The function retrieves the cond condition and for each encountered
    multiple equality predicate it sorts the field references in it
    according to the order of tables specified by the table_join_idx
    parameter. Then it eliminates the multiple equality predicate it
    replacing it by the conjunction of simple equality predicates
    equating every field from the multiple equality to the first
    field in it, or to the constant, if there is any.
    After this the function retrieves all other conjuncted
    predicates substitute every field reference by the field reference
    to the first equal field or equal constant if there are any.
  @param cond            condition to process
  @param cond_equal      multiple equalities to take into consideration
  @param table_join_idx  index to tables determining field preference

  @note
    At the first glance full sort of fields in multiple equality
    seems to be an overkill. Yet it's not the case due to possible
    new fields in multiple equality item of lower levels. We want
    the order in them to comply with the order of upper levels.

  @return
    The transformed condition
*/
COND* substitute_for_best_equal_field(COND *cond, COND_EQUAL *cond_equal, void *table_join_idx)
{
  Item_equal *item_equal;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item> *cond_list= ((Item_cond*) cond)->argument_list();

    bool and_level= ((Item_cond*) cond)->functype() ==
                      Item_func::COND_AND_FUNC;
    if (and_level)
    {
      cond_equal= &((Item_cond_and *) cond)->cond_equal;
      cond_list->disjoin((List<Item> *) &cond_equal->current_level);

      List<Item_equal>::iterator it(cond_equal->current_level.begin());
      while ((item_equal= it++))
      {
        item_equal->sort(&compare_fields_by_table_order, table_join_idx);
      }
    }

    List<Item>::iterator li(cond_list->begin());
    Item *item;
    while ((item= li++))
    {
      Item *new_item =substitute_for_best_equal_field(item, cond_equal,
                                                      table_join_idx);
      /*
        This works OK with PS/SP re-execution as changes are made to
        the arguments of AND/OR items only
      */
      if (new_item != item)
        li.replace(new_item);
    }

    if (and_level)
    {
      List<Item_equal>::iterator it(cond_equal->current_level.begin());
      while ((item_equal= it++))
      {
        cond= eliminate_item_equal(cond, cond_equal->upper_levels, item_equal);
        // This occurs when eliminate_item_equal() founds that cond is
        // always false and substitutes it with Item_int 0.
        // Due to this, value of item_equal will be 0, so just return it.
        if (cond->type() != Item::COND_ITEM)
          break;
      }
    }
    if (cond->type() == Item::COND_ITEM &&
        !((Item_cond*)cond)->argument_list()->size())
      cond= new Item_int((int32_t)cond->val_bool());

  }
  else if (cond->type() == Item::FUNC_ITEM &&
           ((Item_cond*) cond)->functype() == Item_func::MULT_EQUAL_FUNC)
  {
    item_equal= (Item_equal *) cond;
    item_equal->sort(&compare_fields_by_table_order, table_join_idx);
    if (cond_equal && &cond_equal->current_level.front() == item_equal)
      cond_equal= 0;
    return eliminate_item_equal(0, cond_equal, item_equal);
  }
  else
    cond->transform(&Item::replace_equal_field, 0);
  return cond;
}

/**
  Check appearance of new constant items in multiple equalities
  of a condition after reading a constant table.

    The function retrieves the cond condition and for each encountered
    multiple equality checks whether new constants have appeared after
    reading the constant (single row) table tab. If so it adjusts
    the multiple equality appropriately.

  @param cond       condition whose multiple equalities are to be checked
  @param table      constant table that has been read
*/
void update_const_equal_items(COND *cond, JoinTable *tab)
{
  if (!(cond->used_tables() & tab->table->map))
    return;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item> *cond_list= ((Item_cond*) cond)->argument_list();
    List<Item>::iterator li(cond_list->begin());
    Item *item;
    while ((item= li++))
      update_const_equal_items(item, tab);
  }
  else if (cond->type() == Item::FUNC_ITEM &&
           ((Item_cond*) cond)->functype() == Item_func::MULT_EQUAL_FUNC)
  {
    Item_equal *item_equal= (Item_equal *) cond;
    bool contained_const= item_equal->get_const() != NULL;
    item_equal->update_const();
    if (!contained_const && item_equal->get_const())
    {
      /* Update keys for range analysis */
      Item_equal_iterator it(item_equal->begin());
      Item_field *item_field;
      while ((item_field= it++))
      {
        Field *field= item_field->field;
        JoinTable *stat= field->getTable()->reginfo.join_tab;
        key_map possible_keys= field->key_start;
        possible_keys&= field->getTable()->keys_in_use_for_query;
        stat[0].const_keys|= possible_keys;

        /*
          For each field in the multiple equality (for which we know that it
          is a constant) we have to find its corresponding key part, and set
          that key part in const_key_parts.
        */
        if (possible_keys.any())
        {
          Table *field_tab= field->getTable();
          optimizer::KeyUse *use;
          for (use= stat->keyuse; use && use->getTable() == field_tab; use++)
            if (possible_keys.test(use->getKey()) &&
                field_tab->key_info[use->getKey()].key_part[use->getKeypart()].field ==
                field)
              field_tab->const_key_parts[use->getKey()]|= use->getKeypartMap();
        }
      }
    }
  }
}

/*
  change field = field to field = const for each found field = const in the
  and_level
*/
static void change_cond_ref_to_const(Session *session,
                                     list<COND_CMP>& save_list,
                                     Item *and_father,
                                     Item *cond,
                                     Item *field,
                                     Item *value)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC;
    List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
    Item *item;
    while ((item=li++))
      change_cond_ref_to_const(session, save_list, and_level ? cond : item, item, field, value);

    return;
  }
  if (cond->eq_cmp_result() == Item::COND_OK)
    return;					// Not a boolean function

  Item_bool_func2 *func=  (Item_bool_func2*) cond;
  Item **args= func->arguments();
  Item *left_item=  args[0];
  Item *right_item= args[1];
  Item_func::Functype functype=  func->functype();

  if (right_item->eq(field,0) && left_item != value &&
      right_item->cmp_context == field->cmp_context &&
      (left_item->result_type() != STRING_RESULT ||
       value->result_type() != STRING_RESULT ||
       left_item->collation.collation == value->collation.collation))
  {
    Item *tmp=value->clone_item();
    if (tmp)
    {
      tmp->collation.set(right_item->collation);
      args[1]= tmp;
      func->update_used_tables();
      if ((functype == Item_func::EQ_FUNC || functype == Item_func::EQUAL_FUNC) &&
	        and_father != cond &&
          ! left_item->const_item())
      {
        cond->marker=1;
        save_list.push_back( COND_CMP(and_father, func) );
      }
      func->set_cmp_func();
    }
  }
  else if (left_item->eq(field,0) && right_item != value &&
           left_item->cmp_context == field->cmp_context &&
           (right_item->result_type() != STRING_RESULT ||
            value->result_type() != STRING_RESULT ||
            right_item->collation.collation == value->collation.collation))
  {
    Item *tmp= value->clone_item();
    if (tmp)
    {
      tmp->collation.set(left_item->collation);
      *args= tmp;
      value= tmp;
      func->update_used_tables();
      if ((functype == Item_func::EQ_FUNC || functype == Item_func::EQUAL_FUNC) &&
          and_father != cond &&
          ! right_item->const_item())
      {
        args[0]= args[1];                       // For easy check
        args[1]= value;
        cond->marker=1;
        save_list.push_back( COND_CMP(and_father, func) );
      }
      func->set_cmp_func();
    }
  }
}

/**
  Remove additional condition inserted by IN/ALL/ANY transformation.

  @param conds   condition for processing

  @return
    new conditions
*/
Item *remove_additional_cond(Item* conds)
{
  if (conds->name == in_additional_cond)
    return 0;
  if (conds->type() == Item::COND_ITEM)
  {
    Item_cond *cnd= (Item_cond*) conds;
    List<Item>::iterator li(cnd->argument_list()->begin());
    Item *item;
    while ((item= li++))
    {
      if (item->name == in_additional_cond)
      {
	li.remove();
	if (cnd->argument_list()->size() == 1)
	  return &cnd->argument_list()->front();
	return conds;
      }
    }
  }
  return conds;
}

static void propagate_cond_constants(Session *session,
                                     list<COND_CMP>& save_list,
                                     COND *and_father,
                                     COND *cond)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC;
    List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
    Item *item;
    list<COND_CMP> save;
    while ((item=li++))
    {
      propagate_cond_constants(session, save, and_level ? cond : item, item);
    }
    if (and_level)
    {
      // Handle other found items
      for (list<COND_CMP>::iterator iter= save.begin(); iter != save.end(); ++iter)
      {
        Item **args= iter->second->arguments();
        if (not args[0]->const_item())
        {
          change_cond_ref_to_const(session, save, iter->first,
                                   iter->first, args[0], args[1] );
        }
      }
    }
  }
  else if (and_father != cond && !cond->marker)		// In a AND group
  {
    if (cond->type() == Item::FUNC_ITEM &&
        (((Item_func*) cond)->functype() == Item_func::EQ_FUNC ||
        ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC))
    {
      Item_func_eq *func=(Item_func_eq*) cond;
      Item **args= func->arguments();
      bool left_const= args[0]->const_item();
      bool right_const= args[1]->const_item();
      if (!(left_const && right_const) &&
          args[0]->result_type() == args[1]->result_type())
      {
        if (right_const)
        {
                resolve_const_item(session, &args[1], args[0]);
          func->update_used_tables();
                change_cond_ref_to_const(session, save_list, and_father, and_father,
                                        args[0], args[1]);
        }
        else if (left_const)
        {
                resolve_const_item(session, &args[0], args[1]);
          func->update_used_tables();
                change_cond_ref_to_const(session, save_list, and_father, and_father,
                                        args[1], args[0]);
        }
      }
    }
  }
}

/**
  Check interleaving with an inner tables of an outer join for
  extension table.

    Check if table next_tab can be added to current partial join order, and
    if yes, record that it has been added.

    The function assumes that both current partial join order and its
    extension with next_tab are valid wrt table dependencies.

  @verbatim
     IMPLEMENTATION
       LIMITATIONS ON JOIN order_st
         The nested [outer] joins executioner algorithm imposes these limitations
         on join order:
         1. "Outer tables first" -  any "outer" table must be before any
             corresponding "inner" table.
         2. "No interleaving" - tables inside a nested join must form a continuous
            sequence in join order (i.e. the sequence must not be interrupted by
            tables that are outside of this nested join).

         #1 is checked elsewhere, this function checks #2 provided that #1 has
         been already checked.

       WHY NEED NON-INTERLEAVING
         Consider an example:

           select * from t0 join t1 left join (t2 join t3) on cond1

         The join order "t1 t2 t0 t3" is invalid:

         table t0 is outside of the nested join, so WHERE condition for t0 is
         attached directly to t0 (without triggers, and it may be used to access
         t0). Applying WHERE(t0) to (t2,t0,t3) record is invalid as we may miss
         combinations of (t1, t2, t3) that satisfy condition cond1, and produce a
         null-complemented (t1, t2.NULLs, t3.NULLs) row, which should not have
         been produced.

         If table t0 is not between t2 and t3, the problem doesn't exist:
          If t0 is located after (t2,t3), WHERE(t0) is applied after nested join
           processing has finished.
          If t0 is located before (t2,t3), predicates like WHERE_cond(t0, t2) are
           wrapped into condition triggers, which takes care of correct nested
           join processing.

       HOW IT IS IMPLEMENTED
         The limitations on join order can be rephrased as follows: for valid
         join order one must be able to:
           1. write down the used tables in the join order on one line.
           2. for each nested join, put one '(' and one ')' on the said line
           3. write "LEFT JOIN" and "ON (...)" where appropriate
           4. get a query equivalent to the query we're trying to execute.

         Calls to check_interleaving_with_nj() are equivalent to writing the
         above described line from left to right.
         A single check_interleaving_with_nj(A,B) call is equivalent to writing
         table B and appropriate brackets on condition that table A and
         appropriate brackets is the last what was written. Graphically the
         transition is as follows:

                              +---- current position
                              |
             ... last_tab ))) | ( next_tab )  )..) | ...
                                X          Y   Z   |
                                                   +- need to move to this
                                                      position.

         Notes about the position:
           The caller guarantees that there is no more then one X-bracket by
           checking "!(remaining_tables & s->dependent)" before calling this
           function. X-bracket may have a pair in Y-bracket.

         When "writing" we store/update this auxilary info about the current
         position:
          1. join->cur_embedding_map - bitmap of pairs of brackets (aka nested
             joins) we've opened but didn't close.
          2. {each NestedJoin class not simplified away}->counter - number
             of this nested join's children that have already been added to to
             the partial join order.
  @endverbatim

  @param join       Join being processed
  @param next_tab   Table we're going to extend the current partial join with

  @retval
    false  Join order extended, nested joins info about current join
    order (see NOTE section) updated.
  @retval
    true   Requested join order extension not allowed.
*/
bool check_interleaving_with_nj(JoinTable *next_tab)
{
  TableList *next_emb= next_tab->table->pos_in_table_list->getEmbedding();
  Join *join= next_tab->join;

  if ((join->cur_embedding_map & ~next_tab->embedding_map).any())
  {
    /*
      next_tab is outside of the "pair of brackets" we're currently in.
      Cannot add it.
    */
    return true;
  }

  /*
    Do update counters for "pairs of brackets" that we've left (marked as
    X,Y,Z in the above picture)
  */
  for (;next_emb; next_emb= next_emb->getEmbedding())
  {
    next_emb->getNestedJoin()->counter_++;
    if (next_emb->getNestedJoin()->counter_ == 1)
    {
      /*
        next_emb is the first table inside a nested join we've "entered". In
        the picture above, we're looking at the 'X' bracket. Don't exit yet as
        X bracket might have Y pair bracket.
      */
      join->cur_embedding_map |= next_emb->getNestedJoin()->nj_map;
    }

    if (next_emb->getNestedJoin()->join_list.size() !=
        next_emb->getNestedJoin()->counter_)
      break;

    /*
      We're currently at Y or Z-bracket as depicted in the above picture.
      Mark that we've left it and continue walking up the brackets hierarchy.
    */
    join->cur_embedding_map &= ~next_emb->getNestedJoin()->nj_map;
  }
  return false;
}

COND *optimize_cond(Join *join, COND *conds, List<TableList> *join_list, Item::cond_result *cond_value)
{
  Session *session= join->session;

  if (!conds)
    *cond_value= Item::COND_TRUE;
  else
  {
    /*
      Build all multiple equality predicates and eliminate equality
      predicates that can be inferred from these multiple equalities.
      For each reference of a field included into a multiple equality
      that occurs in a function set a pointer to the multiple equality
      predicate. Substitute a constant instead of this field if the
      multiple equality contains a constant.
    */
    conds= build_equal_items(join->session, conds, NULL, join_list,
                             &join->cond_equal);

    /* change field = field to field = const for each found field = const */
    list<COND_CMP> temp;
    propagate_cond_constants(session, temp, conds, conds);
    /*
      Remove all instances of item == item
      Remove all and-levels where CONST item != CONST item
    */
    conds= remove_eq_conds(session, conds, cond_value) ;
  }
  return(conds);
}

/**
  Remove const and eq items.

  @return
    Return new item, or NULL if no condition @n
    cond_value is set to according:
    - COND_OK     : query is possible (field = constant)
    - COND_TRUE   : always true	( 1 = 1 )
    - COND_FALSE  : always false	( 1 = 2 )
*/
COND *remove_eq_conds(Session *session, COND *cond, Item::cond_result *cond_value)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC);

    List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
    Item::cond_result tmp_cond_value;
    bool should_fix_fields= false;

    *cond_value= Item::COND_UNDEF;
    Item *item;
    while ((item= li++))
    {
      Item *new_item= remove_eq_conds(session, item, &tmp_cond_value);
      if (! new_item)
	      li.remove();
      else if (item != new_item)
      {
        li.replace(new_item);
        should_fix_fields= true;
      }
      if (*cond_value == Item::COND_UNDEF)
	      *cond_value= tmp_cond_value;

      switch (tmp_cond_value)
      {
        case Item::COND_OK:			/* Not true or false */
          if (and_level || (*cond_value == Item::COND_FALSE))
            *cond_value= tmp_cond_value;
          break;
        case Item::COND_FALSE:
          if (and_level)
          {
            *cond_value= tmp_cond_value;
            return (COND *) NULL;			/* Always false */
          }
          break;
        case Item::COND_TRUE:
          if (! and_level)
          {
            *cond_value= tmp_cond_value;
            return (COND *) NULL;			/* Always true */
          }
          break;
        case Item::COND_UNDEF:			/* Impossible */
          break;
      }
    }

    if (should_fix_fields)
      cond->update_used_tables();

    if (! ((Item_cond*) cond)->argument_list()->size() || *cond_value != Item::COND_OK)
      return (COND*) NULL;

    if (((Item_cond*) cond)->argument_list()->size() == 1)
    {
      /* Argument list contains only one element, so reduce it so a single item, then remove list */
      item= &((Item_cond*) cond)->argument_list()->front();
      ((Item_cond*) cond)->argument_list()->clear();
      return item;
    }
  }
  else if (cond->type() == Item::FUNC_ITEM && ((Item_func*) cond)->functype() == Item_func::ISNULL_FUNC)
  {
    /*
      Handles this special case for some ODBC applications:
      The are requesting the row that was just updated with a auto_increment
      value with this construct:

      SELECT * from table_name where auto_increment_column IS NULL
      This will be changed to:
      SELECT * from table_name where auto_increment_column = LAST_INSERT_ID
    */

    Item_func_isnull *func= (Item_func_isnull*) cond;
    Item **args= func->arguments();
    if (args[0]->type() == Item::FIELD_ITEM)
    {
      Field *field= ((Item_field*) args[0])->field;
      if (field->flags & AUTO_INCREMENT_FLAG
          && ! field->getTable()->maybe_null
          && session->options & OPTION_AUTO_IS_NULL
          && (
            session->first_successful_insert_id_in_prev_stmt > 0
            && session->substitute_null_with_insert_id
            )
          )
      {
        COND *new_cond= new Item_func_eq(args[0], new Item_int("last_insert_id()", 
          session->read_first_successful_insert_id_in_prev_stmt(), MY_INT64_NUM_DECIMAL_DIGITS));
        cond= new_cond;
        /*
        Item_func_eq can't be fixed after creation so we do not check
        cond->fixed, also it do not need tables so we use 0 as second
        argument.
        */
        cond->fix_fields(session, &cond);
        /*
          IS NULL should be mapped to LAST_INSERT_ID only for first row, so
          clear for next row
        */
        session->substitute_null_with_insert_id= false;
      }
#ifdef NOTDEFINED
      /* fix to replace 'NULL' dates with '0' (shreeve@uci.edu) */
      else if (
          ((field->type() == DRIZZLE_TYPE_DATE) || (field->type() == DRIZZLE_TYPE_DATETIME))
          && (field->flags & NOT_NULL_FLAG)
          && ! field->table->maybe_null)
      {
        COND* new_cond= new Item_func_eq(args[0],new Item_int("0", 0, 2));
        cond= new_cond;
        /*
        Item_func_eq can't be fixed after creation so we do not check
        cond->fixed, also it do not need tables so we use 0 as second
        argument.
        */
        cond->fix_fields(session, &cond);
      }
#endif /* NOTDEFINED */
    }
    if (cond->const_item())
    {
      *cond_value= eval_const_cond(cond) ? Item::COND_TRUE : Item::COND_FALSE;
      return (COND *) NULL;
    }
  }
  else if (cond->const_item() && !cond->is_expensive())
  /*
    @todo
    Excluding all expensive functions is too restritive we should exclude only
    materialized IN subquery predicates because they can't yet be evaluated
    here (they need additional initialization that is done later on).

    The proper way to exclude the subqueries would be to walk the cond tree and
    check for materialized subqueries there.

  */
  {
    *cond_value= eval_const_cond(cond) ? Item::COND_TRUE : Item::COND_FALSE;
    return (COND *) NULL;
  }
  else if ((*cond_value= cond->eq_cmp_result()) != Item::COND_OK)
  {
    /* boolan compare function */
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->eq(right_item,1))
    {
      if (!left_item->maybe_null || ((Item_func*) cond)->functype() == Item_func::EQUAL_FUNC)
	      return (COND*) NULL;			/* Comparison of identical items */
    }
  }
  *cond_value= Item::COND_OK;
  return cond;					/* Point at next and return into recursion */
}

/*
  Check if equality can be used in removing components of GROUP BY/DISTINCT

  SYNOPSIS
    test_if_equality_guarantees_uniqueness()
      l          the left comparison argument (a field if any)
      r          the right comparison argument (a const of any)

  DESCRIPTION
    Checks if an equality predicate can be used to take away
    DISTINCT/GROUP BY because it is known to be true for exactly one
    distinct value (e.g. <expr> == <const>).
    Arguments must be of the same type because e.g.
    <string_field> = <int_const> may match more than 1 distinct value from
    the column.
    We must take into consideration and the optimization done for various
    string constants when compared to dates etc (see Item_int_with_ref) as
    well as the collation of the arguments.

  RETURN VALUE
    true    can be used
    false   cannot be used
*/
static bool test_if_equality_guarantees_uniqueness(Item *l, Item *r)
{
  return r->const_item() &&
    /* elements must be compared as dates */
     (Arg_comparator::can_compare_as_dates(l, r, 0) ||
      /* or of the same result type */
      (r->result_type() == l->result_type() &&
       /* and must have the same collation if compared as strings */
       (l->result_type() != STRING_RESULT ||
        l->collation.collation == r->collation.collation)));
}

/**
  Return true if the item is a const value in all the WHERE clause.
*/
bool const_expression_in_where(COND *cond, Item *comp_item, Item **const_item)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= (((Item_cond*) cond)->functype()
		     == Item_func::COND_AND_FUNC);
    List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
    Item *item;
    while ((item=li++))
    {
      bool res=const_expression_in_where(item, comp_item, const_item);
      if (res)					// Is a const value
      {
        if (and_level)
          return 1;
      }
      else if (!and_level)
        return 0;
    }
    return and_level ? 0 : 1;
  }
  else if (cond->eq_cmp_result() != Item::COND_OK)
  {						// boolan compare function
    Item_func* func= (Item_func*) cond;
    if (func->functype() != Item_func::EQUAL_FUNC &&
	      func->functype() != Item_func::EQ_FUNC)
      return 0;
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->eq(comp_item,1))
    {
      if (test_if_equality_guarantees_uniqueness (left_item, right_item))
      {
        if (*const_item)
          return right_item->eq(*const_item, 1);
        *const_item=right_item;
        return 1;
      }
    }
    else if (right_item->eq(comp_item,1))
    {
      if (test_if_equality_guarantees_uniqueness (right_item, left_item))
      {
        if (*const_item)
          return left_item->eq(*const_item, 1);
        *const_item=left_item;
        return 1;
      }
    }
  }
  return 0;
}

/**
  @details
  Rows produced by a join sweep may end up in a temporary table or be sent
  to a client. Setup the function of the nested loop join algorithm which
  handles final fully constructed and matched records.

  @param join   join to setup the function for.

  @return
    end_select function to use. This function can't fail.
*/
Next_select_func setup_end_select_func(Join *join)
{
  Table *table= join->tmp_table;
  Tmp_Table_Param *tmp_tbl= &join->tmp_table_param;
  Next_select_func end_select;

  /* Set up select_end */
  if (table)
  {
    if (table->group && tmp_tbl->sum_func_count &&
        !tmp_tbl->precomputed_group_by)
    {
      if (table->getShare()->sizeKeys())
      {
        end_select= end_update;
      }
      else
      {
        end_select= end_unique_update;
      }
    }
    else if (join->sort_and_group && !tmp_tbl->precomputed_group_by)
    {
      end_select= end_write_group;
    }
    else
    {
      end_select= end_write;
      if (tmp_tbl->precomputed_group_by)
      {
        /*
          A preceding call to create_tmp_table in the case when loose
          index scan is used guarantees that
          Tmp_Table_Param::items_to_copy has enough space for the group
          by functions. It is OK here to use memcpy since we copy
          Item_sum pointers into an array of Item pointers.
        */
        memcpy(tmp_tbl->items_to_copy + tmp_tbl->func_count,
               join->sum_funcs,
               sizeof(Item*)*tmp_tbl->sum_func_count);
        tmp_tbl->items_to_copy[tmp_tbl->func_count+tmp_tbl->sum_func_count]= 0;
      }
    }
  }
  else
  {
    if ((join->sort_and_group) &&
        !tmp_tbl->precomputed_group_by)
      end_select= end_send_group;
    else
      end_select= end_send;
  }
  return end_select;
}

/**
  Make a join of all tables and write it on socket or to table.

  @retval
    0  if ok
  @retval
    1  if error is sent
  @retval
    -1  if error should be sent
*/
int do_select(Join *join, List<Item> *fields, Table *table)
{
  int rc= 0;
  enum_nested_loop_state error= NESTED_LOOP_OK;
  JoinTable *join_tab= NULL;

  join->tmp_table= table;			/* Save for easy recursion */
  join->fields= fields;

  if (table)
  {
    table->cursor->extra(HA_EXTRA_WRITE_CACHE);
    table->emptyRecord();
    if (table->group && join->tmp_table_param.sum_func_count &&
        table->getShare()->sizeKeys() && !table->cursor->inited)
    {
      int tmp_error;
      tmp_error= table->cursor->startIndexScan(0, 0);
      if (tmp_error != 0)
      {
        table->print_error(tmp_error, MYF(0));
        return -1;
      }
    }
  }
  /* Set up select_end */
  Next_select_func end_select= setup_end_select_func(join);
  if (join->tables)
  {
    join->join_tab[join->tables-1].next_select= end_select;

    join_tab=join->join_tab+join->const_tables;
  }
  join->send_records=0;
  if (join->tables == join->const_tables)
  {
    /*
      HAVING will be checked after processing aggregate functions,
      But WHERE should checkd here (we alredy have read tables)
    */
    if (!join->conds || join->conds->val_int())
    {
      error= (*end_select)(join, 0, 0);
      if (error == NESTED_LOOP_OK || error == NESTED_LOOP_QUERY_LIMIT)
	      error= (*end_select)(join, 0, 1);

      /*
        If we don't go through evaluate_join_record(), do the counting
        here.  join->send_records is increased on success in end_send(),
        so we don't touch it here.
      */
      join->examined_rows++;
      join->session->row_count++;
      assert(join->examined_rows <= 1);
    }
    else if (join->send_row_on_empty_set())
    {
      List<Item> *columns_list= fields;
      rc= join->result->send_data(*columns_list);
    }
  }
  else
  {
    assert(join->tables);
    error= sub_select(join,join_tab,0);
    if (error == NESTED_LOOP_OK || error == NESTED_LOOP_NO_MORE_ROWS)
      error= sub_select(join,join_tab,1);
    if (error == NESTED_LOOP_QUERY_LIMIT)
      error= NESTED_LOOP_OK;                    /* select_limit used */
  }
  if (error == NESTED_LOOP_NO_MORE_ROWS)
    error= NESTED_LOOP_OK;

  if (error == NESTED_LOOP_OK)
  {
    /*
      Sic: this branch works even if rc != 0, e.g. when
      send_data above returns an error.
    */
    if (!table)					// If sending data to client
    {
      /*
        The following will unlock all cursors if the command wasn't an
        update command
      */
      join->join_free();			// Unlock all cursors
      if (join->result->send_eof())
        rc= 1;                                  // Don't send error
    }
  }
  else
    rc= -1;
  if (table)
  {
    int tmp, new_errno= 0;
    if ((tmp=table->cursor->extra(HA_EXTRA_NO_CACHE)))
    {
      new_errno= tmp;
    }
    if ((tmp=table->cursor->ha_index_or_rnd_end()))
    {
      new_errno= tmp;
    }
    if (new_errno)
      table->print_error(new_errno,MYF(0));
  }
  return(join->session->is_error() ? -1 : rc);
}

enum_nested_loop_state sub_select_cache(Join *join, JoinTable *join_tab, bool end_of_records)
{
  enum_nested_loop_state rc;

  if (end_of_records)
  {
    rc= flush_cached_records(join,join_tab,false);
    if (rc == NESTED_LOOP_OK || rc == NESTED_LOOP_NO_MORE_ROWS)
      rc= sub_select(join,join_tab,end_of_records);
    return rc;
  }
  if (join->session->getKilled())		// If aborted by user
  {
    join->session->send_kill_message();
    return NESTED_LOOP_KILLED;
  }
  if (join_tab->use_quick != 2 || test_if_quick_select(join_tab) <= 0)
  {
    if (! join_tab->cache.store_record_in_cache())
      return NESTED_LOOP_OK;                     // There is more room in cache
    return flush_cached_records(join,join_tab,false);
  }
  rc= flush_cached_records(join, join_tab, true);
  if (rc == NESTED_LOOP_OK || rc == NESTED_LOOP_NO_MORE_ROWS)
    rc= sub_select(join, join_tab, end_of_records);
  return rc;
}

/**
  Retrieve records ends with a given beginning from the result of a join.

    For a given partial join record consisting of records from the tables
    preceding the table join_tab in the execution plan, the function
    retrieves all matching full records from the result set and
    send them to the result set stream.

  @note
    The function effectively implements the  final (n-k) nested loops
    of nested loops join algorithm, where k is the ordinal number of
    the join_tab table and n is the total number of tables in the join query.
    It performs nested loops joins with all conjunctive predicates from
    the where condition pushed as low to the tables as possible.
    E.g. for the query
    @code
      SELECT * FROM t1,t2,t3
      WHERE t1.a=t2.a AND t2.b=t3.b AND t1.a BETWEEN 5 AND 9
    @endcode
    the predicate (t1.a BETWEEN 5 AND 9) will be pushed to table t1,
    given the selected plan prescribes to nest retrievals of the
    joined tables in the following order: t1,t2,t3.
    A pushed down predicate are attached to the table which it pushed to,
    at the field join_tab->select_cond.
    When executing a nested loop of level k the function runs through
    the rows of 'join_tab' and for each row checks the pushed condition
    attached to the table.
    If it is false the function moves to the next row of the
    table. If the condition is true the function recursively executes (n-k-1)
    remaining embedded nested loops.
    The situation becomes more complicated if outer joins are involved in
    the execution plan. In this case the pushed down predicates can be
    checked only at certain conditions.
    Suppose for the query
    @code
      SELECT * FROM t1 LEFT JOIN (t2,t3) ON t3.a=t1.a
      WHERE t1>2 AND (t2.b>5 OR t2.b IS NULL)
    @endcode
    the optimizer has chosen a plan with the table order t1,t2,t3.
    The predicate P1=t1>2 will be pushed down to the table t1, while the
    predicate P2=(t2.b>5 OR t2.b IS NULL) will be attached to the table
    t2. But the second predicate can not be unconditionally tested right
    after a row from t2 has been read. This can be done only after the
    first row with t3.a=t1.a has been encountered.
    Thus, the second predicate P2 is supplied with a guarded value that are
    stored in the field 'found' of the first inner table for the outer join
    (table t2). When the first row with t3.a=t1.a for the  current row
    of table t1  appears, the value becomes true. For now on the predicate
    is evaluated immediately after the row of table t2 has been read.
    When the first row with t3.a=t1.a has been encountered all
    conditions attached to the inner tables t2,t3 must be evaluated.
    Only when all of them are true the row is sent to the output stream.
    If not, the function returns to the lowest nest level that has a false
    attached condition.
    The predicates from on expressions are also pushed down. If in the
    the above example the on expression were (t3.a=t1.a AND t2.a=t1.a),
    then t1.a=t2.a would be pushed down to table t2, and without any
    guard.
    If after the run through all rows of table t2, the first inner table
    for the outer join operation, it turns out that no matches are
    found for the current row of t1, then current row from table t1
    is complemented by nulls  for t2 and t3. Then the pushed down predicates
    are checked for the composed row almost in the same way as it had
    been done for the first row with a match. The only difference is
    the predicates from on expressions are not checked.

  @par
  @b IMPLEMENTATION
  @par
    The function forms output rows for a current partial join of k
    tables tables recursively.
    For each partial join record ending with a certain row from
    join_tab it calls sub_select that builds all possible matching
    tails from the result set.
    To be able  check predicates conditionally items of the class
    Item_func_trig_cond are employed.
    An object of  this class is constructed from an item of class COND
    and a pointer to a guarding boolean variable.
    When the value of the guard variable is true the value of the object
    is the same as the value of the predicate, otherwise it's just returns
    true.
    To carry out a return to a nested loop level of join table t the pointer
    to t is remembered in the field 'return_tab' of the join structure.
    Consider the following query:
    @code
        SELECT * FROM t1,
                      LEFT JOIN
                      (t2, t3 LEFT JOIN (t4,t5) ON t5.a=t3.a)
                      ON t4.a=t2.a
           WHERE (t2.b=5 OR t2.b IS NULL) AND (t4.b=2 OR t4.b IS NULL)
    @endcode
    Suppose the chosen execution plan dictates the order t1,t2,t3,t4,t5
    and suppose for a given joined rows from tables t1,t2,t3 there are
    no rows in the result set yet.
    When first row from t5 that satisfies the on condition
    t5.a=t3.a is found, the pushed down predicate t4.b=2 OR t4.b IS NULL
    becomes 'activated', as well the predicate t4.a=t2.a. But
    the predicate (t2.b=5 OR t2.b IS NULL) can not be checked until
    t4.a=t2.a becomes true.
    In order not to re-evaluate the predicates that were already evaluated
    as attached pushed down predicates, a pointer to the the first
    most inner unmatched table is maintained in join_tab->first_unmatched.
    Thus, when the first row from t5 with t5.a=t3.a is found
    this pointer for t5 is changed from t4 to t2.

    @par
    @b STRUCTURE @b NOTES
    @par
    join_tab->first_unmatched points always backwards to the first inner
    table of the embedding nested join, if any.

  @param join      pointer to the structure providing all context info for
                   the query
  @param join_tab  the first next table of the execution plan to be retrieved
  @param end_records  true when we need to perform final steps of retrival

  @return
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/
enum_nested_loop_state sub_select(Join *join, JoinTable *join_tab, bool end_of_records)
{
  join_tab->table->null_row=0;
  if (end_of_records)
    return (*join_tab->next_select)(join,join_tab+1,end_of_records);

  int error;
  enum_nested_loop_state rc;
  ReadRecord *info= &join_tab->read_record;

  if (join->resume_nested_loop)
  {
    /* If not the last table, plunge down the nested loop */
    if (join_tab < join->join_tab + join->tables - 1)
      rc= (*join_tab->next_select)(join, join_tab + 1, 0);
    else
    {
      join->resume_nested_loop= false;
      rc= NESTED_LOOP_OK;
    }
  }
  else
  {
    join->return_tab= join_tab;

    if (join_tab->last_inner)
    {
      /* join_tab is the first inner table for an outer join operation. */

      /* Set initial state of guard variables for this table.*/
      join_tab->found=0;
      join_tab->not_null_compl= 1;

      /* Set first_unmatched for the last inner table of this group */
      join_tab->last_inner->first_unmatched= join_tab;
    }
    join->session->row_count= 0;

    error= (*join_tab->read_first_record)(join_tab);
    rc= evaluate_join_record(join, join_tab, error);
  }

  /*
    Note: psergey has added the 2nd part of the following condition; the
    change should probably be made in 5.1, too.
  */
  while (rc == NESTED_LOOP_OK && join->return_tab >= join_tab)
  {
    error= info->read_record(info);
    rc= evaluate_join_record(join, join_tab, error);
  }

  if (rc == NESTED_LOOP_NO_MORE_ROWS &&
      join_tab->last_inner && !join_tab->found)
    rc= evaluate_null_complemented_join_record(join, join_tab);

  if (rc == NESTED_LOOP_NO_MORE_ROWS)
    rc= NESTED_LOOP_OK;
  return rc;
}

int safe_index_read(JoinTable *tab)
{
  int error;
  Table *table= tab->table;
  if ((error=table->cursor->index_read_map(table->getInsertRecord(),
                                         tab->ref.key_buff,
                                         make_prev_keypart_map(tab->ref.key_parts),
                                         HA_READ_KEY_EXACT)))
    return table->report_error(error);
  return 0;
}

/**
  Read a (constant) table when there is at most one matching row.

  @param tab			Table to read

  @retval
    0	Row was found
  @retval
    -1   Row was not found
  @retval
    1   Got an error (other than row not found) during read
*/
int join_read_const(JoinTable *tab)
{
  int error;
  Table *table= tab->table;
  if (table->status & STATUS_GARBAGE)		// If first read
  {
    table->status= 0;
    if (cp_buffer_from_ref(tab->join->session, &tab->ref))
      error= HA_ERR_KEY_NOT_FOUND;
    else
    {
      error=table->cursor->index_read_idx_map(table->getInsertRecord(),tab->ref.key,
                                            (unsigned char*) tab->ref.key_buff,
                                            make_prev_keypart_map(tab->ref.key_parts),
                                            HA_READ_KEY_EXACT);
    }
    if (error)
    {
      table->status= STATUS_NOT_FOUND;
      tab->table->mark_as_null_row();
      table->emptyRecord();
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
        return table->report_error(error);
      return -1;
    }
    table->storeRecord();
  }
  else if (!(table->status & ~STATUS_NULL_ROW))	// Only happens with left join
  {
    table->status=0;
    table->restoreRecord();			// restore old record
  }
  table->null_row=0;
  return table->status ? -1 : 0;
}

/*
  eq_ref access method implementation: "read_first" function

  SYNOPSIS
    join_read_key()
      tab  JoinTable of the accessed table

  DESCRIPTION
    This is "read_fist" function for the "ref" access method. The difference
    from "ref" is that it has a one-element "cache" (see cmp_buffer_with_ref)

  RETURN
    0  - Ok
   -1  - Row not found
    1  - Error
*/
int join_read_key(JoinTable *tab)
{
  int error;
  Table *table= tab->table;

  if (!table->cursor->inited)
  {
    error= table->cursor->startIndexScan(tab->ref.key, tab->sorted);
    if (error != 0)
    {
      table->print_error(error, MYF(0));
    }
  }

  /* @todo Why don't we do "Late NULLs Filtering" here? */
  if (cmp_buffer_with_ref(tab) ||
      (table->status & (STATUS_GARBAGE | STATUS_NO_PARENT | STATUS_NULL_ROW)))
  {
    if (tab->ref.key_err)
    {
      table->status=STATUS_NOT_FOUND;
      return -1;
    }
    error=table->cursor->index_read_map(table->getInsertRecord(),
                                      tab->ref.key_buff,
                                      make_prev_keypart_map(tab->ref.key_parts),
                                      HA_READ_KEY_EXACT);
    if (error && error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return table->report_error(error);
  }
  table->null_row=0;
  return table->status ? -1 : 0;
}

/*
  ref access method implementation: "read_first" function

  SYNOPSIS
    join_read_always_key()
      tab  JoinTable of the accessed table

  DESCRIPTION
    This is "read_first" function for the "ref" access method.

    The functon must leave the index initialized when it returns.
    ref_or_null access implementation depends on that.

  RETURN
    0  - Ok
   -1  - Row not found
    1  - Error
*/
int join_read_always_key(JoinTable *tab)
{
  int error;
  Table *table= tab->table;

  /* Initialize the index first */
  if (!table->cursor->inited)
  {
    error= table->cursor->startIndexScan(tab->ref.key, tab->sorted);
    if (error != 0)
      return table->report_error(error);
  }

  /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
  for (uint32_t i= 0 ; i < tab->ref.key_parts ; i++)
  {
    if ((tab->ref.null_rejecting & 1 << i) && tab->ref.items[i]->is_null())
        return -1;
  }

  if (cp_buffer_from_ref(tab->join->session, &tab->ref))
    return -1;
  if ((error=table->cursor->index_read_map(table->getInsertRecord(),
                                         tab->ref.key_buff,
                                         make_prev_keypart_map(tab->ref.key_parts),
                                         HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return table->report_error(error);
    return -1;
  }

  return 0;
}

/**
  This function is used when optimizing away ORDER BY in
  SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC.
*/
int join_read_last_key(JoinTable *tab)
{
  int error;
  Table *table= tab->table;

  if (!table->cursor->inited)
  {
    error= table->cursor->startIndexScan(tab->ref.key, tab->sorted);
    if (error != 0)
      return table->report_error(error);
  }
  if (cp_buffer_from_ref(tab->join->session, &tab->ref))
    return -1;
  if ((error=table->cursor->index_read_last_map(table->getInsertRecord(),
                                              tab->ref.key_buff,
                                              make_prev_keypart_map(tab->ref.key_parts))))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return table->report_error(error);
    return -1;
  }
  return 0;
}

int join_no_more_records(ReadRecord *)
{
  return -1;
}

int join_read_next_same_diff(ReadRecord *info)
{
  Table *table= info->table;
  JoinTable *tab=table->reginfo.join_tab;
  if (tab->insideout_match_tab->found_match)
  {
    KeyInfo *key= tab->table->key_info + tab->index;
    do
    {
      int error;
      /* Save index tuple from record to the buffer */
      key_copy(tab->insideout_buf, info->record, key, 0);

      if ((error=table->cursor->index_next_same(table->getInsertRecord(),
                                              tab->ref.key_buff,
                                              tab->ref.key_length)))
      {
        if (error != HA_ERR_END_OF_FILE)
          return table->report_error(error);
        table->status= STATUS_GARBAGE;
        return -1;
      }
    } while (!key_cmp(tab->table->key_info[tab->index].key_part,
                      tab->insideout_buf, key->key_length));
    tab->insideout_match_tab->found_match= 0;
    return 0;
  }
  else
    return join_read_next_same(info);
}

int join_read_next_same(ReadRecord *info)
{
  int error;
  Table *table= info->table;
  JoinTable *tab=table->reginfo.join_tab;

  if ((error=table->cursor->index_next_same(table->getInsertRecord(),
					  tab->ref.key_buff,
					  tab->ref.key_length)))
  {
    if (error != HA_ERR_END_OF_FILE)
      return table->report_error(error);
    table->status= STATUS_GARBAGE;
    return -1;
  }

  return 0;
}

int join_read_prev_same(ReadRecord *info)
{
  int error;
  Table *table= info->table;
  JoinTable *tab=table->reginfo.join_tab;

  if ((error=table->cursor->index_prev(table->getInsertRecord())))
    return table->report_error(error);
  if (key_cmp_if_same(table, tab->ref.key_buff, tab->ref.key,
                      tab->ref.key_length))
  {
    table->status=STATUS_NOT_FOUND;
    error= -1;
  }
  return error;
}

int join_init_quick_read_record(JoinTable *tab)
{
  if (test_if_quick_select(tab) == -1)
    return -1;					/* No possible records */
  return join_init_read_record(tab);
}

int init_read_record_seq(JoinTable *tab)
{
  tab->read_record.init_reard_record_sequential();

  if (tab->read_record.cursor->startTableScan(1))
    return 1;
  return (*tab->read_record.read_record)(&tab->read_record);
}

int test_if_quick_select(JoinTable *tab)
{
  safe_delete(tab->select->quick);

  return tab->select->test_quick_select(tab->join->session, tab->keys,
					(table_map) 0, HA_POS_ERROR, 0, false);
}

int join_init_read_record(JoinTable *tab)
{
  if (tab->select && tab->select->quick && tab->select->quick->reset())
    return 1;

  if (tab->read_record.init_read_record(tab->join->session, tab->table, tab->select, 1, true))
    return 1;

  return (*tab->read_record.read_record)(&tab->read_record);
}

int join_read_first(JoinTable *tab)
{
  int error;
  Table *table=tab->table;
  if (!table->key_read && table->covering_keys.test(tab->index) &&
      !table->no_keyread)
  {
    table->key_read= 1;
    table->cursor->extra(HA_EXTRA_KEYREAD);
  }
  tab->table->status= 0;
  tab->read_record.table=table;
  tab->read_record.cursor=table->cursor;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->getInsertRecord();
  if (tab->insideout_match_tab)
  {
    tab->read_record.do_insideout_scan= tab;
    tab->read_record.read_record=join_read_next_different;
    tab->insideout_match_tab->found_match= 0;
  }
  else
  {
    tab->read_record.read_record=join_read_next;
    tab->read_record.do_insideout_scan= 0;
  }

  if (!table->cursor->inited)
  {
    error= table->cursor->startIndexScan(tab->index, tab->sorted);
    if (error != 0)
    {
      table->report_error(error);
      return -1;
    }
  }
  if ((error=tab->table->cursor->index_first(tab->table->getInsertRecord())))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      table->report_error(error);
    return -1;
  }

  return 0;
}

int join_read_next_different(ReadRecord *info)
{
  JoinTable *tab= info->do_insideout_scan;
  if (tab->insideout_match_tab->found_match)
  {
    KeyInfo *key= tab->table->key_info + tab->index;
    do
    {
      int error;
      /* Save index tuple from record to the buffer */
      key_copy(tab->insideout_buf, info->record, key, 0);

      if ((error=info->cursor->index_next(info->record)))
        return info->table->report_error(error);
    } while (!key_cmp(tab->table->key_info[tab->index].key_part,
                      tab->insideout_buf, key->key_length));
    tab->insideout_match_tab->found_match= 0;
    return 0;
  }
  else
    return join_read_next(info);
}

int join_read_next(ReadRecord *info)
{
  int error;
  if ((error=info->cursor->index_next(info->record)))
    return info->table->report_error(error);
  return 0;
}

int join_read_last(JoinTable *tab)
{
  Table *table=tab->table;
  int error;
  if (!table->key_read && table->covering_keys.test(tab->index) &&
      !table->no_keyread)
  {
    table->key_read=1;
    table->cursor->extra(HA_EXTRA_KEYREAD);
  }
  tab->table->status=0;
  tab->read_record.read_record=join_read_prev;
  tab->read_record.table=table;
  tab->read_record.cursor=table->cursor;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->getInsertRecord();
  if (!table->cursor->inited)
  {
    error= table->cursor->startIndexScan(tab->index, 1);
    if (error != 0)
      return table->report_error(error);
  }
  if ((error= tab->table->cursor->index_last(tab->table->getInsertRecord())))
    return table->report_error(error);

  return 0;
}

int join_read_prev(ReadRecord *info)
{
  int error;
  if ((error= info->cursor->index_prev(info->record)))
    return info->table->report_error(error);

  return 0;
}

/**
  Reading of key with key reference and one part that may be NULL.
*/
int join_read_always_key_or_null(JoinTable *tab)
{
  int res;

  /* First read according to key which is NOT NULL */
  *tab->ref.null_ref_key= 0;			// Clear null byte
  if ((res= join_read_always_key(tab)) >= 0)
    return res;

  /* Then read key with null value */
  *tab->ref.null_ref_key= 1;			// Set null byte
  return safe_index_read(tab);
}

int join_read_next_same_or_null(ReadRecord *info)
{
  int error;
  if ((error= join_read_next_same(info)) >= 0)
    return error;
  JoinTable *tab= info->table->reginfo.join_tab;

  /* Test if we have already done a read after null key */
  if (*tab->ref.null_ref_key)
    return -1;					// All keys read
  *tab->ref.null_ref_key= 1;			// Set null byte
  return safe_index_read(tab);			// then read null keys
}

enum_nested_loop_state end_send_group(Join *join, JoinTable *, bool end_of_records)
{
  int idx= -1;
  enum_nested_loop_state ok_code= NESTED_LOOP_OK;

  if (!join->first_record || end_of_records ||
      (idx=test_if_item_cache_changed(join->group_fields)) >= 0)
  {
    if (join->first_record ||
        (end_of_records && !join->group && !join->group_optimized_away))
    {
      if (idx < (int) join->send_group_parts)
      {
        int error=0;
        {
          if (!join->first_record)
          {
                  List<Item>::iterator it(join->fields->begin());
                  Item *item;
            /* No matching rows for group function */
            join->clear();

            while ((item= it++))
              item->no_rows_in_result();
          }
          if (join->having && join->having->val_int() == 0)
            error= -1;				// Didn't satisfy having
          else
          {
            if (join->do_send_rows)
              error=join->result->send_data(*join->fields) ? 1 : 0;
            join->send_records++;
          }
          if (join->rollup.getState() != Rollup::STATE_NONE && error <= 0)
          {
            if (join->rollup_send_data((uint32_t) (idx+1)))
              error= 1;
          }
        }
        if (error > 0)
          return(NESTED_LOOP_ERROR);
        if (end_of_records)
          return(NESTED_LOOP_OK);
        if (join->send_records >= join->unit->select_limit_cnt &&
            join->do_send_rows)
        {
          if (!(join->select_options & OPTION_FOUND_ROWS))
            return(NESTED_LOOP_QUERY_LIMIT); // Abort nicely
          join->do_send_rows=0;
          join->unit->select_limit_cnt = HA_POS_ERROR;
        }
        else if (join->send_records >= join->fetch_limit)
        {
          /*
            There is a server side cursor and all rows
            for this fetch request are sent.
          */
          /*
            Preventing code duplication. When finished with the group reset
            the group functions and copy_fields. We fall through. bug #11904
          */
          ok_code= NESTED_LOOP_CURSOR_LIMIT;
        }
      }
    }
    else
    {
      if (end_of_records)
        return(NESTED_LOOP_OK);
      join->first_record=1;
      test_if_item_cache_changed(join->group_fields);
    }
    if (idx < (int) join->send_group_parts)
    {
      /*
        This branch is executed also for cursors which have finished their
        fetch limit - the reason for ok_code.
      */
      copy_fields(&join->tmp_table_param);
      if (init_sum_functions(join->sum_funcs, join->sum_funcs_end[idx+1]))
        return(NESTED_LOOP_ERROR);
      return(ok_code);
    }
  }
  if (update_sum_func(join->sum_funcs))
    return(NESTED_LOOP_ERROR);
  return(NESTED_LOOP_OK);
}

enum_nested_loop_state end_write_group(Join *join, JoinTable *, bool end_of_records)
{
  Table *table=join->tmp_table;
  int	  idx= -1;

  if (join->session->getKilled())
  {						// Aborted by user
    join->session->send_kill_message();
    return NESTED_LOOP_KILLED;
  }
  if (!join->first_record || end_of_records ||
      (idx=test_if_item_cache_changed(join->group_fields)) >= 0)
  {
    if (join->first_record || (end_of_records && !join->group))
    {
      int send_group_parts= join->send_group_parts;
      if (idx < send_group_parts)
      {
        if (!join->first_record)
        {
          /* No matching rows for group function */
          join->clear();
        }
        copy_sum_funcs(join->sum_funcs, join->sum_funcs_end[send_group_parts]);
        if (!join->having || join->having->val_int())
        {
          int error= table->cursor->insertRecord(table->getInsertRecord());

          if (error)
          {
            my_error(ER_USE_SQL_BIG_RESULT, MYF(0));
            return NESTED_LOOP_ERROR;
          }
        }
        if (join->rollup.getState() != Rollup::STATE_NONE)
        {
          if (join->rollup_write_data((uint32_t) (idx+1), table))
            return NESTED_LOOP_ERROR;
        }
        if (end_of_records)
          return NESTED_LOOP_OK;
      }
    }
    else
    {
      if (end_of_records)
        return NESTED_LOOP_OK;
      join->first_record=1;
      test_if_item_cache_changed(join->group_fields);
    }
    if (idx < (int) join->send_group_parts)
    {
      copy_fields(&join->tmp_table_param);
      if (copy_funcs(join->tmp_table_param.items_to_copy, join->session))
        return NESTED_LOOP_ERROR;
      if (init_sum_functions(join->sum_funcs, join->sum_funcs_end[idx+1]))
        return NESTED_LOOP_ERROR;
      return NESTED_LOOP_OK;
    }
  }
  if (update_sum_func(join->sum_funcs))
    return NESTED_LOOP_ERROR;
  return NESTED_LOOP_OK;
}

/*****************************************************************************
  Remove calculation with tables that aren't yet read. Remove also tests
  against fields that are read through key where the table is not a
  outer join table.
  We can't remove tests that are made against columns which are stored
  in sorted order.
  @return
    1 if right_item used is a removable reference key on left_item
    0 otherwise.
****************************************************************************/
bool test_if_ref(Item_field *left_item,Item *right_item)
{
  Field *field=left_item->field;
  // No need to change const test. We also have to keep tests on LEFT JOIN
  if (not field->getTable()->const_table && !field->getTable()->maybe_null)
  {
    Item *ref_item=part_of_refkey(field->getTable(),field);
    if (ref_item && ref_item->eq(right_item,1))
    {
      right_item= right_item->real_item();
      if (right_item->type() == Item::FIELD_ITEM)
        return (field->eq_def(((Item_field *) right_item)->field));
      /* remove equalities injected by IN->EXISTS transformation */
      else if (right_item->type() == Item::CACHE_ITEM)
        return ((Item_cache *)right_item)->eq_def (field);
      if (right_item->const_item() && !(right_item->is_null()))
      {
        /*
          We can remove binary fields and numerical fields except float,
          as float comparison isn't 100 % secure
          We have to keep normal strings to be able to check for end spaces

                sergefp: the above seems to be too restrictive. Counterexample:
                  create table t100 (v varchar(10), key(v)) default charset=latin1;
                  insert into t100 values ('a'),('a ');
                  explain select * from t100 where v='a';
                The EXPLAIN shows 'using Where'. Running the query returns both
                rows, so it seems there are no problems with endspace in the most
                frequent case?
        */
        if (field->binary() &&
            field->real_type() != DRIZZLE_TYPE_VARCHAR &&
            field->decimals() == 0)
        {
          return ! store_val_in_field(field, right_item, CHECK_FIELD_WARN);
        }
      }
    }
  }
  return 0;
}

/*
  Extract a condition that can be checked after reading given table

  SYNOPSIS
    make_cond_for_table()
      cond         Condition to analyze
      tables       Tables for which "current field values" are available
      used_table   Table that we're extracting the condition for (may
                   also include PSEUDO_TABLE_BITS

  DESCRIPTION
    Extract the condition that can be checked after reading the table
    specified in 'used_table', given that current-field values for tables
    specified in 'tables' bitmap are available.

    The function assumes that
      - Constant parts of the condition has already been checked.
      - Condition that could be checked for tables in 'tables' has already
        been checked.

    The function takes into account that some parts of the condition are
    guaranteed to be true by employed 'ref' access methods (the code that
    does this is located at the end, search down for "EQ_FUNC").


  SEE ALSO
    make_cond_for_info_schema uses similar algorithm

  RETURN
    Extracted condition
*/
COND *make_cond_for_table(COND *cond, table_map tables, table_map used_table, bool exclude_expensive_cond)
{
  if (used_table && !(cond->used_tables() & used_table) &&
    /*
      Exclude constant conditions not checked at optimization time if
      the table we are pushing conditions to is the first one.
      As a result, such conditions are not considered as already checked
      and will be checked at execution time, attached to the first table.
    */
    !((used_table & 1) && cond->is_expensive()))
    return (COND*) 0;				// Already checked
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
        return (COND*) 0;
      List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
      Item *item;
      while ((item=li++))
      {
        Item *fix= make_cond_for_table(item,tables,used_table,
                                            exclude_expensive_cond);
        if (fix)
          new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->size())
      {
        case 0:
          return (COND*) 0;			// Always true
        case 1:
          return &new_cond->argument_list()->front();
        default:
          /*
            Item_cond_and do not need fix_fields for execution, its parameters
            are fixed or do not need fix_fields, too
          */
          new_cond->quick_fix_field();
          new_cond->used_tables_cache= ((Item_cond_and*) cond)->used_tables_cache & tables;
          return new_cond;
      }
    }
    else
    {						// Or list
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
        return (COND*) 0;
      List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
      Item *item;
      while ((item=li++))
      {
        Item *fix= make_cond_for_table(item,tables,0L, exclude_expensive_cond);
        if (!fix)
          return (COND*) 0;			// Always true
        new_cond->argument_list()->push_back(fix);
      }
      /*
        Item_cond_and do not need fix_fields for execution, its parameters
        are fixed or do not need fix_fields, too
      */
      new_cond->quick_fix_field();
      new_cond->used_tables_cache= ((Item_cond_or*) cond)->used_tables_cache;
      new_cond->top_level_item();
      return new_cond;
    }
  }

  /*
    Because the following test takes a while and it can be done
    table_count times, we mark each item that we have examined with the result
    of the test
  */

  if (cond->marker == 3 || (cond->used_tables() & ~tables) ||
      /*
        When extracting constant conditions, treat expensive conditions as
        non-constant, so that they are not evaluated at optimization time.
      */
      (!used_table && exclude_expensive_cond && cond->is_expensive()))
    return (COND*) 0;				// Can't check this yet
  if (cond->marker == 2 || cond->eq_cmp_result() == Item::COND_OK)
    return cond;				// Not boolean op

  /*
    Remove equalities that are guaranteed to be true by use of 'ref' access
    method
  */
  if (((Item_func*) cond)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM && test_if_ref((Item_field*) left_item,right_item))
    {
      cond->marker=3;			// Checked when read
      return (COND*) 0;
    }
    if (right_item->type() == Item::FIELD_ITEM &&	test_if_ref((Item_field*) right_item,left_item))
    {
      cond->marker=3;			// Checked when read
      return (COND*) 0;
    }
  }
  cond->marker=2;
  return cond;
}

static Item *part_of_refkey(Table *table,Field *field)
{
  if (!table->reginfo.join_tab)
    return (Item*) 0;             // field from outer non-select (UPDATE,...)

  uint32_t ref_parts=table->reginfo.join_tab->ref.key_parts;
  if (ref_parts)
  {
    KeyPartInfo *key_part=
      table->key_info[table->reginfo.join_tab->ref.key].key_part;
    uint32_t part;

    for (part=0 ; part < ref_parts ; part++)
    {
      if (table->reginfo.join_tab->ref.cond_guards[part])
        return 0;
    }

    for (part=0 ; part < ref_parts ; part++,key_part++)
    {
      if (field->eq(key_part->field) &&
	  !(key_part->key_part_flag & HA_PART_KEY_SEG) &&
          //If field can be NULL, we should not remove this predicate, as
          //it may lead to non-rejection of NULL values.
          !(field->real_maybe_null()))
      {
	return table->reginfo.join_tab->ref.items[part];
      }
    }
  }
  return (Item*) 0;
}

/**
  Test if one can use the key to resolve order_st BY.

  @param order                 Sort order
  @param table                 Table to sort
  @param idx                   Index to check
  @param used_key_parts        Return value for used key parts.


  @note
    used_key_parts is set to correct key parts used if return value != 0
    (On other cases, used_key_part may be changed)

  @retval
    1   key is ok.
  @retval
    0   Key can't be used
  @retval
    -1   Reverse key can be used
*/
static int test_if_order_by_key(Order *order, Table *table, uint32_t idx, uint32_t *used_key_parts)
{
  KeyPartInfo *key_part= NULL;
  KeyPartInfo *key_part_end= NULL;
  key_part= table->key_info[idx].key_part;
  key_part_end= key_part + table->key_info[idx].key_parts;
  key_part_map const_key_parts=table->const_key_parts[idx];
  int reverse= 0;
  bool on_primary_key= false;

  for (; order ; order=order->next, const_key_parts>>=1)
  {
    Field *field=((Item_field*) (*order->item)->real_item())->field;
    int flag;

    /*
      Skip key parts that are constants in the WHERE clause.
      These are already skipped in the ORDER BY by const_expression_in_where()
    */
    for (; const_key_parts & 1 ; const_key_parts>>= 1)
      key_part++;

    if (key_part == key_part_end)
    {
      /*
        We are at the end of the key. Check if the engine has the primary
        key as a suffix to the secondary keys. If it has continue to check
        the primary key as a suffix.
      */
      if (!on_primary_key &&
          (table->cursor->getEngine()->check_flag(HTON_BIT_PRIMARY_KEY_IN_READ_INDEX)) &&
          table->getShare()->hasPrimaryKey())
      {
        on_primary_key= true;
        key_part= table->key_info[table->getShare()->getPrimaryKey()].key_part;
        key_part_end=key_part+table->key_info[table->getShare()->getPrimaryKey()].key_parts;
        const_key_parts=table->const_key_parts[table->getShare()->getPrimaryKey()];

        for (; const_key_parts & 1 ; const_key_parts>>= 1)
          key_part++;
        /*
         The primary and secondary key parts were all const (i.e. there's
         one row).  The sorting doesn't matter.
        */
        if (key_part == key_part_end && reverse == 0)
          return 1;
      }
      else
        return 0;
    }

    if (key_part->field != field)
      return 0;

    /* set flag to 1 if we can use read-next on key, else to -1 */
    flag= ((order->asc == !(key_part->key_part_flag & HA_REVERSE_SORT)) ?
           1 : -1);
    if (reverse && flag != reverse)
      return 0;
    reverse=flag;				// Remember if reverse
    key_part++;
  }
  *used_key_parts= on_primary_key ? table->key_info[idx].key_parts :
    (uint32_t) (key_part - table->key_info[idx].key_part);
  if (reverse == -1 && !(table->index_flags(idx) &
                         HA_READ_PREV))
    reverse= 0;                                 // Index can't be used
  return(reverse);
}

/**
  Test if a second key is the subkey of the first one.

  @param key_part              First key parts
  @param ref_key_part          Second key parts
  @param ref_key_part_end      Last+1 part of the second key

  @note
    Second key MUST be shorter than the first one.

  @retval
    1	is a subkey
  @retval
    0	no sub key
*/
inline bool is_subkey(KeyPartInfo *key_part,
                      KeyPartInfo *ref_key_part,
	              KeyPartInfo *ref_key_part_end)
{
  for (; ref_key_part < ref_key_part_end; key_part++, ref_key_part++)
    if (! key_part->field->eq(ref_key_part->field))
      return 0;
  return 1;
}

/**
  Test if we can use one of the 'usable_keys' instead of 'ref' key
  for sorting.

  @param ref			Number of key, used for WHERE clause
  @param usable_keys		Keys for testing

  @return
    - MAX_KEY			If we can't use other key
    - the number of found key	Otherwise
*/
static uint32_t test_if_subkey(Order *order,
                               Table *table,
                               uint32_t ref,
                               uint32_t ref_key_parts,
	                       const key_map *usable_keys)
{
  uint32_t nr;
  uint32_t min_length= UINT32_MAX;
  uint32_t best= MAX_KEY;
  uint32_t not_used;
  KeyPartInfo *ref_key_part= table->key_info[ref].key_part;
  KeyPartInfo *ref_key_part_end= ref_key_part + ref_key_parts;

  for (nr= 0 ; nr < table->getShare()->sizeKeys() ; nr++)
  {
    if (usable_keys->test(nr) &&
	table->key_info[nr].key_length < min_length &&
	table->key_info[nr].key_parts >= ref_key_parts &&
	is_subkey(table->key_info[nr].key_part, ref_key_part,
		  ref_key_part_end) &&
	test_if_order_by_key(order, table, nr, &not_used))
    {
      min_length= table->key_info[nr].key_length;
      best= nr;
    }
  }
  return best;
}

/**
  Check if GROUP BY/DISTINCT can be optimized away because the set is
  already known to be distinct.

  Used in removing the GROUP BY/DISTINCT of the following types of
  statements:
  @code
    SELECT [DISTINCT] <unique_key_cols>... FROM <single_table_ref>
      [GROUP BY <unique_key_cols>,...]
  @endcode

    If (a,b,c is distinct)
    then <any combination of a,b,c>,{whatever} is also distinct

    This function checks if all the key parts of any of the unique keys
    of the table are referenced by a list : either the select list
    through find_field_in_item_list or GROUP BY list through
    find_field_in_order_list.
    If the above holds and the key parts cannot contain NULLs then we
    can safely remove the GROUP BY/DISTINCT,
    as no result set can be more distinct than an unique key.

  @param table                The table to operate on.
  @param find_func            function to iterate over the list and search
                              for a field

  @retval
    1                    found
  @retval
    0                    not found.
*/
bool list_contains_unique_index(Table *table, bool (*find_func) (Field *, void *), void *data)
{
  for (uint32_t keynr= 0; keynr < table->getShare()->sizeKeys(); keynr++)
  {
    if (keynr == table->getShare()->getPrimaryKey() ||
         (table->key_info[keynr].flags & HA_NOSAME))
    {
      KeyInfo *keyinfo= table->key_info + keynr;
      KeyPartInfo *key_part= NULL;
      KeyPartInfo *key_part_end= NULL;

      for (key_part=keyinfo->key_part,
           key_part_end=key_part+ keyinfo->key_parts;
           key_part < key_part_end;
           key_part++)
      {
        if (key_part->field->maybe_null() ||
            ! find_func(key_part->field, data))
          break;
      }
      if (key_part == key_part_end)
        return 1;
    }
  }
  return 0;
}

/**
  Helper function for list_contains_unique_index.
  Find a field reference in a list of order_st structures.
  Finds a direct reference of the Field in the list.

  @param field                The field to search for.
  @param data                 order_st *.The list to search in

  @retval
    1                    found
  @retval
    0                    not found.
*/
bool find_field_in_order_list (Field *field, void *data)
{
  Order *group= (Order *) data;
  bool part_found= 0;
  for (Order *tmp_group= group; tmp_group; tmp_group=tmp_group->next)
  {
    Item *item= (*tmp_group->item)->real_item();
    if (item->type() == Item::FIELD_ITEM &&
        ((Item_field*) item)->field->eq(field))
    {
      part_found= 1;
      break;
    }
  }
  return part_found;
}

/**
  Helper function for list_contains_unique_index.
  Find a field reference in a dynamic list of Items.
  Finds a direct reference of the Field in the list.

  @param[in] field             The field to search for.
  @param[in] data              List<Item> *.The list to search in

  @retval
    1                    found
  @retval
    0                    not found.
*/
bool find_field_in_item_list (Field *field, void *data)
{
  List<Item> *fields= (List<Item> *) data;
  bool part_found= 0;
  List<Item>::iterator li(fields->begin());
  Item *item;

  while ((item= li++))
  {
    if (item->type() == Item::FIELD_ITEM &&
        ((Item_field*) item)->field->eq(field))
    {
      part_found= 1;
      break;
    }
  }
  return part_found;
}

/**
  Test if we can skip the ORDER BY by using an index.

  SYNOPSIS
    test_if_skip_sort_order()
      tab
      order
      select_limit
      no_changes
      map

  If we can use an index, the JoinTable / tab->select struct
  is changed to use the index.

  The index must cover all fields in <order>, or it will not be considered.

  @todo
    - sergeyp: Results of all index merge selects actually are ordered
    by clustered PK values.

  @retval
    0    We have to use filesort to do the sorting
  @retval
    1    We can use an index.
*/
bool test_if_skip_sort_order(JoinTable *tab, Order *order, ha_rows select_limit, bool no_changes, const key_map *map)
{
  int32_t ref_key;
  uint32_t ref_key_parts;
  int order_direction;
  uint32_t used_key_parts;
  Table *table=tab->table;
  optimizer::SqlSelect *select= tab->select;
  key_map usable_keys;
  optimizer::QuickSelectInterface *save_quick= NULL;

  /*
    Keys disabled by ALTER Table ... DISABLE KEYS should have already
    been taken into account.
  */
  usable_keys= *map;

  for (Order *tmp_order=order; tmp_order ; tmp_order=tmp_order->next)
  {
    Item *item= (*tmp_order->item)->real_item();
    if (item->type() != Item::FIELD_ITEM)
    {
      usable_keys.reset();
      return 0;
    }
    usable_keys&= ((Item_field*) item)->field->part_of_sortkey;
    if (usable_keys.none())
      return 0;					// No usable keys
  }

  ref_key= -1;
  /* Test if constant range in WHERE */
  if (tab->ref.key >= 0 && tab->ref.key_parts)
  {
    ref_key=	   tab->ref.key;
    ref_key_parts= tab->ref.key_parts;
    if (tab->type == AM_REF_OR_NULL)
      return 0;
  }
  else if (select && select->quick)		// Range found by optimizer/range
  {
    int quick_type= select->quick->get_type();
    save_quick= select->quick;
    /*
      assume results are not ordered when index merge is used
      @todo sergeyp: Results of all index merge selects actually are ordered
      by clustered PK values.
    */

    if (quick_type == optimizer::QuickSelectInterface::QS_TYPE_INDEX_MERGE ||
        quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_UNION ||
        quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_INTERSECT)
      return 0;
    ref_key=	   select->quick->index;
    ref_key_parts= select->quick->used_key_parts;
  }

  if (ref_key >= 0)
  {
    /*
      We come here when there is a REF key.
    */
    if (! usable_keys.test(ref_key))
    {
      /*
        We come here when ref_key is not among usable_keys
      */
      uint32_t new_ref_key;
      /*
        If using index only read, only consider other possible index only
        keys
      */
      if (table->covering_keys.test(ref_key))
        usable_keys&= table->covering_keys;
      if (tab->pre_idx_push_select_cond)
        tab->select_cond= tab->select->cond= tab->pre_idx_push_select_cond;
      if ((new_ref_key= test_if_subkey(order, table, ref_key, ref_key_parts,
				       &usable_keys)) < MAX_KEY)
      {
        /* Found key that can be used to retrieve data in sorted order */
        if (tab->ref.key >= 0)
        {
          /*
            We'll use ref access method on key new_ref_key. In general case
            the index search tuple for new_ref_key will be different (e.g.
            when one index is defined as (part1, part2, ...) and another as
            (part1, part2(N), ...) and the WHERE clause contains
            "part1 = const1 AND part2=const2".
            So we build tab->ref from scratch here.
          */
          optimizer::KeyUse *keyuse= tab->keyuse;
          while (keyuse->getKey() != new_ref_key && keyuse->getTable() == tab->table)
            keyuse++;

          if (create_ref_for_key(tab->join, tab, keyuse,
                                 tab->join->const_table_map))
            return 0;
        }
        else
        {
          /*
            The range optimizer constructed QuickRange for ref_key, and
            we want to use instead new_ref_key as the index. We can't
            just change the index of the quick select, because this may
            result in an incosistent QUICK_SELECT object. Below we
            create a new QUICK_SELECT from scratch so that all its
            parameres are set correctly by the range optimizer.
           */
          key_map new_ref_key_map;
          new_ref_key_map.reset();  // Force the creation of quick select
          new_ref_key_map.set(new_ref_key); // only for new_ref_key.

          if (select->test_quick_select(tab->join->session, new_ref_key_map, 0,
                                        (tab->join->select_options &
                                         OPTION_FOUND_ROWS) ?
                                        HA_POS_ERROR :
                                        tab->join->unit->select_limit_cnt,0,
                                        true) <=
              0)
            return 0;
        }
        ref_key= new_ref_key;
      }
    }
    /* Check if we get the rows in requested sorted order by using the key */
    if (usable_keys.test(ref_key) &&
        (order_direction= test_if_order_by_key(order,table,ref_key,
					       &used_key_parts)))
      goto check_reverse_order;
  }
  {
    /*
      Check whether there is an index compatible with the given order
      usage of which is cheaper than usage of the ref_key index (ref_key>=0)
      or a table scan.
      It may be the case if order_st/GROUP BY is used with LIMIT.
    */
    uint32_t nr;
    key_map keys;
    uint32_t best_key_parts= 0;
    int best_key_direction= 0;
    ha_rows best_records= 0;
    double read_time;
    int best_key= -1;
    bool is_best_covering= false;
    double fanout= 1;
    Join *join= tab->join;
    uint32_t tablenr= tab - join->join_tab;
    ha_rows table_records= table->cursor->stats.records;
    bool group= join->group && order == join->group_list;
    optimizer::Position cur_pos;

    /*
      If not used with LIMIT, only use keys if the whole query can be
      resolved with a key;  This is because filesort() is usually faster than
      retrieving all rows through an index.
    */
    if (select_limit >= table_records)
    {
      /*
        filesort() and join cache are usually faster than reading in
        index order and not using join cache
        */
      if (tab->type == AM_ALL && tab->join->tables > tab->join->const_tables + 1)
        return 0;
      keys= *table->cursor->keys_to_use_for_scanning();
      keys|= table->covering_keys;

      /*
        We are adding here also the index specified in FORCE INDEX clause,
        if any.
        This is to allow users to use index in order_st BY.
      */
      if (table->force_index)
        keys|= (group ? table->keys_in_use_for_group_by :
                                table->keys_in_use_for_order_by);
      keys&= usable_keys;
    }
    else
      keys= usable_keys;

    cur_pos= join->getPosFromOptimalPlan(tablenr);
    read_time= cur_pos.getCost();
    for (uint32_t i= tablenr+1; i < join->tables; i++)
    {
      cur_pos= join->getPosFromOptimalPlan(i);
      fanout*= cur_pos.getFanout(); // fanout is always >= 1
    }

    for (nr=0; nr < table->getShare()->sizeKeys() ; nr++)
    {
      int direction;
      if (keys.test(nr) &&
          (direction= test_if_order_by_key(order, table, nr, &used_key_parts)))
      {
        bool is_covering= table->covering_keys.test(nr) || (nr == table->getShare()->getPrimaryKey() && table->cursor->primary_key_is_clustered());

        /*
          Don't use an index scan with ORDER BY without limit.
          For GROUP BY without limit always use index scan
          if there is a suitable index.
          Why we hold to this asymmetry hardly can be explained
          rationally. It's easy to demonstrate that using
          temporary table + filesort could be cheaper for grouping
          queries too.
        */
        if (is_covering ||
            select_limit != HA_POS_ERROR ||
            (ref_key < 0 && (group || table->force_index)))
        {
          double rec_per_key;
          double index_scan_time;
          KeyInfo *keyinfo= tab->table->key_info+nr;
          if (select_limit == HA_POS_ERROR)
            select_limit= table_records;
          if (group)
          {
            rec_per_key= keyinfo->rec_per_key[used_key_parts-1];
            set_if_bigger(rec_per_key, 1.0);
            /*
              With a grouping query each group containing on average
              rec_per_key records produces only one row that will
              be included into the result set.
            */
            if (select_limit > table_records/rec_per_key)
                select_limit= table_records;
            else
              select_limit= (ha_rows) (select_limit*rec_per_key);
          }
          /*
            If tab=tk is not the last joined table tn then to get first
            L records from the result set we can expect to retrieve
            only L/fanout(tk,tn) where fanout(tk,tn) says how many
            rows in the record set on average will match each row tk.
            Usually our estimates for fanouts are too pessimistic.
            So the estimate for L/fanout(tk,tn) will be too optimistic
            and as result we'll choose an index scan when using ref/range
            access + filesort will be cheaper.
          */
          select_limit= (ha_rows) (select_limit < fanout ?
                                   1 : select_limit/fanout);
          /*
            We assume that each of the tested indexes is not correlated
            with ref_key. Thus, to select first N records we have to scan
            N/selectivity(ref_key) index entries.
            selectivity(ref_key) = #scanned_records/#table_records =
            table->quick_condition_rows/table_records.
            In any case we can't select more than #table_records.
            N/(table->quick_condition_rows/table_records) > table_records
            <=> N > table->quick_condition_rows.
          */
          if (select_limit > table->quick_condition_rows)
            select_limit= table_records;
          else
            select_limit= (ha_rows) (select_limit *
                                     (double) table_records /
                                      table->quick_condition_rows);
          rec_per_key= keyinfo->rec_per_key[keyinfo->key_parts-1];
          set_if_bigger(rec_per_key, 1.0);
          /*
            Here we take into account the fact that rows are
            accessed in sequences rec_per_key records in each.
            Rows in such a sequence are supposed to be ordered
            by rowid/primary key. When reading the data
            in a sequence we'll touch not more pages than the
            table cursor contains.
            TODO. Use the formula for a disk sweep sequential access
            to calculate the cost of accessing data rows for one
            index entry.
          */
          index_scan_time= select_limit/rec_per_key *
                           min(rec_per_key, table->cursor->scan_time());
          if (is_covering || (ref_key < 0 && (group || table->force_index)) ||
              index_scan_time < read_time)
          {
            ha_rows quick_records= table_records;
            if (is_best_covering && !is_covering)
              continue;
            if (table->quick_keys.test(nr))
              quick_records= table->quick_rows[nr];
            if (best_key < 0 ||
                (select_limit <= min(quick_records,best_records) ?
                 keyinfo->key_parts < best_key_parts :
                 quick_records < best_records))
            {
              best_key= nr;
              best_key_parts= keyinfo->key_parts;
              best_records= quick_records;
              is_best_covering= is_covering;
              best_key_direction= direction;
            }
          }
        }
      }
    }
    if (best_key >= 0)
    {
      bool quick_created= false;
      if (table->quick_keys.test(best_key) && best_key != ref_key)
      {
        key_map test_map;
        test_map.reset();       // Force the creation of quick select
        test_map.set(best_key); // only best_key.
        quick_created=
          select->test_quick_select(join->session, test_map, 0,
                                    join->select_options & OPTION_FOUND_ROWS ?
                                    HA_POS_ERROR :
                                    join->unit->select_limit_cnt,
                                    true, false) > 0;
      }
      if (!no_changes)
      {
        if (!quick_created)
        {
          tab->index= best_key;
          tab->read_first_record= best_key_direction > 0 ?
                                  join_read_first:join_read_last;
          tab->type= AM_NEXT;           // Read with index_first(), index_next()
          if (select && select->quick)
          {
            safe_delete(select->quick);
          }
          if (table->covering_keys.test(best_key))
          {
            table->key_read=1;
            table->cursor->extra(HA_EXTRA_KEYREAD);
          }
          table->cursor->ha_index_or_rnd_end();
          if (join->select_options & SELECT_DESCRIBE)
          {
            tab->ref.key= -1;
            tab->ref.key_parts= 0;
            if (select_limit < table_records)
              tab->limit= select_limit;
          }
        }
        else if (tab->type != AM_ALL)
        {
          /*
            We're about to use a quick access to the table.
            We need to change the access method so as the quick access
            method is actually used.
          */
          assert(tab->select->quick);
          tab->type= AM_ALL;
          tab->use_quick=1;
          tab->ref.key= -1;
          tab->ref.key_parts=0;		// Don't use ref key.
          tab->read_first_record= join_init_read_record;
        }
      }
      used_key_parts= best_key_parts;
      order_direction= best_key_direction;
    }
    else
      return 0;
  }

check_reverse_order:
  if (order_direction == -1)		// If ORDER BY ... DESC
  {
    if (select && select->quick)
    {
      /*
        Don't reverse the sort order, if it's already done.
        (In some cases test_if_order_by_key() can be called multiple times
      */
      if (! select->quick->reverse_sorted())
      {
        optimizer::QuickSelectDescending *tmp= NULL;
        bool error= false;
        int quick_type= select->quick->get_type();
        if (quick_type == optimizer::QuickSelectInterface::QS_TYPE_INDEX_MERGE ||
            quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_INTERSECT ||
            quick_type == optimizer::QuickSelectInterface::QS_TYPE_ROR_UNION ||
            quick_type == optimizer::QuickSelectInterface::QS_TYPE_GROUP_MIN_MAX)
        {
          tab->limit= 0;
          select->quick= save_quick;
          return 0; // Use filesort
        }

        /* ORDER BY range_key DESC */
        tmp= new optimizer::QuickSelectDescending((optimizer::QuickRangeSelect*)(select->quick),
                                                  used_key_parts,
                                                  &error);
        if (! tmp || error)
        {
          delete tmp;
          select->quick= save_quick;
          tab->limit= 0;
          return 0; // Reverse sort not supported
        }
        select->quick= tmp;
      }
    }
    else if (tab->type != AM_NEXT &&
             tab->ref.key >= 0 && tab->ref.key_parts <= used_key_parts)
    {
      /*
        SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC

        Use a traversal function that starts by reading the last row
        with key part (A) and then traverse the index backwards.
      */
      tab->read_first_record= join_read_last_key;
      tab->read_record.read_record= join_read_prev_same;
    }
  }
  else if (select && select->quick)
    select->quick->sorted= 1;
  return 1;
}

/*
  If not selecting by given key, create an index how records should be read

  SYNOPSIS
   create_sort_index()
     session		Thread Cursor
     tab		Table to sort (in join structure)
     order		How table should be sorted
     filesort_limit	Max number of rows that needs to be sorted
     select_limit	Max number of rows in final output
		        Used to decide if we should use index or not
     is_order_by        true if we are sorting on order_st BY, false if GROUP BY
                        Used to decide if we should use index or not


  IMPLEMENTATION
   - If there is an index that can be used, 'tab' is modified to use
     this index.
   - If no index, create with filesort() an index cursor that can be used to
     retrieve rows in order (should be done with 'read_record').
     The sorted data is stored in tab->table and will be freed when calling
     tab->table->free_io_cache().

  RETURN VALUES
    0		ok
    -1		Some fatal error
    1		No records
*/
int create_sort_index(Session *session, Join *join, Order *order, ha_rows filesort_limit, ha_rows select_limit, bool is_order_by)
{
  uint32_t length= 0;
  ha_rows examined_rows;
  Table *table;
  optimizer::SqlSelect *select= NULL;
  JoinTable *tab;

  if (join->tables == join->const_tables)
    return 0;				// One row, no need to sort
  tab=    join->join_tab + join->const_tables;
  table=  tab->table;
  select= tab->select;

  /*
    When there is SQL_BIG_RESULT do not sort using index for GROUP BY,
    and thus force sorting on disk unless a group min-max optimization
    is going to be used as it is applied now only for one table queries
    with covering indexes.
  */
  if ((order != join->group_list ||
       !(join->select_options & SELECT_BIG_RESULT) ||
       (select && select->quick && (select->quick->get_type() == optimizer::QuickSelectInterface::QS_TYPE_GROUP_MIN_MAX))) &&
      test_if_skip_sort_order(tab,order,select_limit,0,
                              is_order_by ?  &table->keys_in_use_for_order_by :
                              &table->keys_in_use_for_group_by))
    return 0;
  for (Order *ord= join->order; ord; ord= ord->next)
    length++;
  join->sortorder= make_unireg_sortorder(order, &length, join->sortorder);
  table->sort.io_cache= new internal::io_cache_st;
  table->status=0;				// May be wrong if quick_select

  // If table has a range, move it to select
  if (select && !select->quick && tab->ref.key >= 0)
  {
    if (tab->quick)
    {
      select->quick=tab->quick;
      tab->quick=0;
      /*
        We can only use 'Only index' if quick key is same as ref_key
        and in index_merge 'Only index' cannot be used
      */
      if (table->key_read && ((uint32_t) tab->ref.key != select->quick->index))
      {
        table->key_read=0;
        table->cursor->extra(HA_EXTRA_NO_KEYREAD);
      }
    }
    else
    {
      /*
        We have a ref on a const;  Change this to a range that filesort
        can use.
        For impossible ranges (like when doing a lookup on NULL on a NOT NULL
        field, quick will contain an empty record set.
      */
      if (! (select->quick= (optimizer::get_quick_select_for_ref(session,
                                                                 table,
                                                                 &tab->ref,
                                                                 tab->found_records))))
      {
	return(-1);
      }
    }
  }

  if (table->getShare()->getType())
    table->cursor->info(HA_STATUS_VARIABLE);	// Get record count

  FileSort filesort(*session);
  table->sort.found_records=filesort.run(table,join->sortorder, length,
					 select, filesort_limit, 0,
					 examined_rows);
  tab->records= table->sort.found_records;	// For SQL_CALC_ROWS
  if (select)
  {
    select->cleanup();				// filesort did select
    tab->select= 0;
  }
  tab->select_cond=0;
  tab->last_inner= 0;
  tab->first_unmatched= 0;
  tab->type= AM_ALL;				// Read with normal read_record
  tab->read_first_record= join_init_read_record;
  tab->join->examined_rows+=examined_rows;
  if (table->key_read)				// Restore if we used indexes
  {
    table->key_read=0;
    table->cursor->extra(HA_EXTRA_NO_KEYREAD);
  }

  return(table->sort.found_records == HA_POS_ERROR);
}

int remove_dup_with_compare(Session *session, Table *table, Field **first_field, uint32_t offset, Item *having)
{
  Cursor *cursor=table->cursor;
  char *org_record,*new_record;
  unsigned char *record;
  int error;
  uint32_t reclength= table->getShare()->getRecordLength() - offset;

  org_record=(char*) (record=table->getInsertRecord())+offset;
  new_record=(char*) table->getUpdateRecord()+offset;

  if ((error= cursor->startTableScan(1)))
    goto err;

  error=cursor->rnd_next(record);
  for (;;)
  {
    if (session->getKilled())
    {
      session->send_kill_message();
      error=0;
      goto err;
    }
    if (error)
    {
      if (error == HA_ERR_RECORD_DELETED)
        continue;
      if (error == HA_ERR_END_OF_FILE)
        break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=cursor->deleteRecord(record)))
        goto err;
      error=cursor->rnd_next(record);
      continue;
    }
    copy_blobs(first_field);
    memcpy(new_record,org_record,reclength);

    /* Read through rest of cursor and mark duplicated rows deleted */
    bool found=0;
    for (;;)
    {
      if ((error=cursor->rnd_next(record)))
      {
        if (error == HA_ERR_RECORD_DELETED)
          continue;
        if (error == HA_ERR_END_OF_FILE)
          break;
        goto err;
      }
      if (table->compare_record(first_field) == 0)
      {
        if ((error=cursor->deleteRecord(record)))
          goto err;
      }
      else if (!found)
      {
        found= 1;
        cursor->position(record);	// Remember position
      }
    }
    if (!found)
      break;					// End of cursor
    /* Move current position to the next row */
    error= cursor->rnd_pos(record, cursor->ref);
  }

  cursor->extra(HA_EXTRA_NO_CACHE);
  return 0;
err:
  cursor->extra(HA_EXTRA_NO_CACHE);
  if (error)
    table->print_error(error,MYF(0));
  return 1;
}

/**
  Generate a hash index for each row to quickly find duplicate rows.

  @note
    Note that this will not work on tables with blobs!
*/
int remove_dup_with_hash_index(Session *session,
                               Table *table,
                               uint32_t field_count,
                               Field **first_field,
                               uint32_t key_length,
                               Item *having)
{
  unsigned char *key_pos, *record=table->getInsertRecord();
  int error;
  Cursor &cursor= *table->cursor;
  uint32_t extra_length= ALIGN_SIZE(key_length)-key_length;
  uint32_t *field_length;
  HASH hash;
  std::vector<unsigned char> key_buffer((key_length + extra_length) * (long) cursor.stats.records);
  std::vector<uint32_t> field_lengths(field_count);

  {
    Field **ptr;
    uint32_t total_length= 0;

    for (ptr= first_field, field_length= &field_lengths[0] ; *ptr ; ptr++)
    {
      uint32_t length= (*ptr)->sort_length();
      (*field_length++)= length;
      total_length+= length;
    }
    assert(total_length <= key_length);
    key_length= total_length;
    extra_length= ALIGN_SIZE(key_length)-key_length;
  }

  hash_init(&hash, &my_charset_bin, (uint32_t) cursor.stats.records, 0, key_length, (hash_get_key) 0, 0, 0);

  if ((error= cursor.startTableScan(1)))
    goto err;

  key_pos= &key_buffer[0];
  for (;;)
  {
    if (session->getKilled())
    {
      session->send_kill_message();
      error=0;
      goto err;
    }
    if ((error=cursor.rnd_next(record)))
    {
      if (error == HA_ERR_RECORD_DELETED)
        continue;
      if (error == HA_ERR_END_OF_FILE)
        break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=cursor.deleteRecord(record)))
        goto err;
      continue;
    }

    /* copy fields to key buffer */
    unsigned char* org_key_pos= key_pos;
    field_length= &field_lengths[0];
    for (Field **ptr= first_field ; *ptr ; ptr++)
    {
      (*ptr)->sort_string(key_pos,*field_length);
      key_pos+= *field_length++;
    }
    /* Check if it exists before */
    if (hash_search(&hash, org_key_pos, key_length))
    {
      /* Duplicated found ; Remove the row */
      if ((error=cursor.deleteRecord(record)))
        goto err;
    }
    else
      (void) my_hash_insert(&hash, org_key_pos);
    key_pos+=extra_length;
  }
  hash_free(&hash);
  cursor.extra(HA_EXTRA_NO_CACHE);
  (void) cursor.endTableScan();
  return 0;

err:
  hash_free(&hash);
  cursor.extra(HA_EXTRA_NO_CACHE);
  (void) cursor.endTableScan();
  if (error)
    table->print_error(error,MYF(0));
  return 1;
}

SortField* make_unireg_sortorder(Order* order, uint32_t* length, SortField* sortorder)
{
  SortField *sort,*pos;

  uint32_t count= 0;
  for (Order *tmp = order; tmp; tmp=tmp->next)
    count++;
  if (not sortorder)
    sortorder= (SortField*) memory::sql_alloc(sizeof(SortField) * (max(count, *length) + 1));
  pos= sort= sortorder;

  for (; order; order= order->next,pos++)
  {
    Item *item= order->item[0]->real_item();
    pos->field= 0; pos->item= 0;
    if (item->type() == Item::FIELD_ITEM)
      pos->field= ((Item_field*) item)->field;
    else if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item())
      pos->field= ((Item_sum*) item)->get_tmp_table_field();
    else if (item->type() == Item::COPY_STR_ITEM)
    {						// Blob patch
      pos->item= ((Item_copy_string*) item)->item;
    }
    else
      pos->item= *order->item;
    pos->reverse=! order->asc;
  }
  *length=count;
  return sort;
}

/*
  eq_ref: Create the lookup key and check if it is the same as saved key

  SYNOPSIS
    cmp_buffer_with_ref()
      tab  Join tab of the accessed table

  DESCRIPTION
    Used by eq_ref access method: create the index lookup key and check if
    we've used this key at previous lookup (If yes, we don't need to repeat
    the lookup - the record has been already fetched)

  RETURN
    true   No cached record for the key, or failed to create the key (due to
           out-of-domain error)
    false  The created key is the same as the previous one (and the record
           is already in table->record)
*/
static bool cmp_buffer_with_ref(JoinTable *tab)
{
  bool no_prev_key;
  if (!tab->ref.disable_cache)
  {
    if (!(no_prev_key= tab->ref.key_err))
    {
      /* Previous access found a row. Copy its key */
      memcpy(tab->ref.key_buff2, tab->ref.key_buff, tab->ref.key_length);
    }
  }
  else
    no_prev_key= true;
  if ((tab->ref.key_err= cp_buffer_from_ref(tab->join->session, &tab->ref)) ||
      no_prev_key)
    return 1;
  return memcmp(tab->ref.key_buff2, tab->ref.key_buff, tab->ref.key_length)
    != 0;
}

bool cp_buffer_from_ref(Session *session, table_reference_st *ref)
{
  enum enum_check_fields save_count_cuted_fields= session->count_cuted_fields;
  session->count_cuted_fields= CHECK_FIELD_IGNORE;
  bool result= 0;

  for (StoredKey **copy=ref->key_copy ; *copy ; copy++)
  {
    if ((*copy)->copy() & 1)
    {
      result= 1;
      break;
    }
  }
  session->count_cuted_fields= save_count_cuted_fields;
  return result;
}

/*****************************************************************************
  Group and order functions
*****************************************************************************/

/**
  Resolve an ORDER BY or GROUP BY column reference.

  Given a column reference (represented by 'order') from a GROUP BY or order_st
  BY clause, find the actual column it represents. If the column being
  resolved is from the GROUP BY clause, the procedure searches the SELECT
  list 'fields' and the columns in the FROM list 'tables'. If 'order' is from
  the ORDER BY clause, only the SELECT list is being searched.

  If 'order' is resolved to an Item, then order->item is set to the found
  Item. If there is no item for the found column (that is, it was resolved
  into a table field), order->item is 'fixed' and is added to all_fields and
  ref_pointer_array.

  ref_pointer_array and all_fields are updated.

  @param[in] session		     Pointer to current thread structure
  @param[in,out] ref_pointer_array  All select, group and order by fields
  @param[in] tables                 List of tables to search in (usually
    FROM clause)
  @param[in] order                  Column reference to be resolved
  @param[in] fields                 List of fields to search in (usually
    SELECT list)
  @param[in,out] all_fields         All select, group and order by fields
  @param[in] is_group_field         True if order is a GROUP field, false if
    order_st by field

  @retval
    false if OK
  @retval
    true  if error occurred
*/
static bool find_order_in_list(Session *session,
                               Item **ref_pointer_array,
                               TableList *tables,
                               Order *order,
                               List<Item> &fields,
                               List<Item> &all_fields,
                               bool is_group_field)
{
  Item *order_item= *order->item; /* The item from the GROUP/order_st caluse. */
  Item::Type order_item_type;
  Item **select_item; /* The corresponding item from the SELECT clause. */
  Field *from_field;  /* The corresponding field from the FROM clause. */
  uint32_t counter;
  enum_resolution_type resolution;

  /*
    Local SP variables may be int but are expressions, not positions.
    (And they can't be used before fix_fields is called for them).
  */
  if (order_item->type() == Item::INT_ITEM && order_item->basic_const_item())
  {						/* Order by position */
    uint32_t count= (uint32_t) order_item->val_int();
    if (!count || count > fields.size())
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), order_item->full_name(), session->where());
      return true;
    }
    order->item= ref_pointer_array + count - 1;
    order->in_field_list= 1;
    order->counter= count;
    order->counter_used= 1;
    return false;
  }
  /* Lookup the current GROUP/order_st field in the SELECT clause. */
  select_item= find_item_in_list(session, order_item, fields, &counter, REPORT_EXCEPT_NOT_FOUND, &resolution);
  if (!select_item)
    return true; /* The item is not unique, or some other error occured. */


  /* Check whether the resolved field is not ambiguos. */
  if (select_item != not_found_item)
  {
    Item *view_ref= NULL;
    /*
      If we have found field not by its alias in select list but by its
      original field name, we should additionaly check if we have conflict
      for this name (in case if we would perform lookup in all tables).
    */
    if (resolution == RESOLVED_BEHIND_ALIAS && !order_item->fixed &&
        order_item->fix_fields(session, order->item))
      return true;

    /* Lookup the current GROUP field in the FROM clause. */
    order_item_type= order_item->type();
    from_field= (Field*) not_found_field;
    if ((is_group_field && order_item_type == Item::FIELD_ITEM) ||
        order_item_type == Item::REF_ITEM)
    {
      from_field= find_field_in_tables(session, (Item_ident*) order_item, tables,
                                       NULL, &view_ref, IGNORE_ERRORS, false);
      if (!from_field)
        from_field= (Field*) not_found_field;
    }

    if (from_field == not_found_field ||
        (from_field != view_ref_found ?
         /* it is field of base table => check that fields are same */
         ((*select_item)->type() == Item::FIELD_ITEM &&
          ((Item_field*) (*select_item))->field->eq(from_field)) :
         /*
           in is field of view table => check that references on translation
           table are same
         */
         ((*select_item)->type() == Item::REF_ITEM &&
          view_ref->type() == Item::REF_ITEM &&
          ((Item_ref *) (*select_item))->ref ==
          ((Item_ref *) view_ref)->ref)))
    {
      /*
        If there is no such field in the FROM clause, or it is the same field
        as the one found in the SELECT clause, then use the Item created for
        the SELECT field. As a result if there was a derived field that
        'shadowed' a table field with the same name, the table field will be
        chosen over the derived field.
      */
      order->item= ref_pointer_array + counter;
      order->in_field_list=1;
      return false;
    }
    else
    {
      /*
        There is a field with the same name in the FROM clause. This
        is the field that will be chosen. In this case we issue a
        warning so the user knows that the field from the FROM clause
        overshadows the column reference from the SELECT list.
      */
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_NON_UNIQ_ERROR,
                          ER(ER_NON_UNIQ_ERROR),
                          ((Item_ident*) order_item)->field_name,
                          session->where());
    }
  }

  order->in_field_list=0;
  /*
    The call to order_item->fix_fields() means that here we resolve
    'order_item' to a column from a table in the list 'tables', or to
    a column in some outer query. Exactly because of the second case
    we come to this point even if (select_item == not_found_item),
    inspite of that fix_fields() calls find_item_in_list() one more
    time.

    We check order_item->fixed because Item_func_group_concat can put
    arguments for which fix_fields already was called.
  */
  if (!order_item->fixed &&
      (order_item->fix_fields(session, order->item) ||
       (order_item= *order->item)->check_cols(1) ||
       session->is_fatal_error))
    return true; /* Wrong field. */

  uint32_t el= all_fields.size();
  all_fields.push_front(order_item); /* Add new field to field list. */
  ref_pointer_array[el]= order_item;
  order->item= ref_pointer_array + el;
  return false;
}

/**
  Change order to point at item in select list.

  If item isn't a number and doesn't exits in the select list, add it the
  the field list.
*/
int setup_order(Session *session,
                Item **ref_pointer_array,
                TableList *tables,
		            List<Item> &fields,
                List<Item> &all_fields,
                Order *order)
{
  session->setWhere("order clause");
  for (; order; order= order->next)
  {
    if (find_order_in_list(session, ref_pointer_array, tables, order, fields, all_fields, false))
      return 1;
  }
  return 0;
}

/**
  Intitialize the GROUP BY list.

  @param session			Thread Cursor
  @param ref_pointer_array	We store references to all fields that was
                               not in 'fields' here.
  @param fields		All fields in the select part. Any item in
                               'order' that is part of these list is replaced
                               by a pointer to this fields.
  @param all_fields		Total list of all unique fields used by the
                               select. All items in 'order' that was not part
                               of fields will be added first to this list.
  @param order			The fields we should do GROUP BY on.
  @param hidden_group_fields	Pointer to flag that is set to 1 if we added
                               any fields to all_fields.

  @todo
    change ER_WRONG_FIELD_WITH_GROUP to more detailed
    ER_NON_GROUPING_FIELD_USED

  @retval
    0  ok
  @retval
    1  error (probably out of memory)
*/
int setup_group(Session *session,
                Item **ref_pointer_array,
                TableList *tables,
	              List<Item> &fields,
                List<Item> &all_fields,
                Order *order,
	              bool *hidden_group_fields)
{
  *hidden_group_fields=0;
  Order *ord;

  if (!order)
    return 0;				/* Everything is ok */

  uint32_t org_fields=all_fields.size();

  session->setWhere("group statement");
  for (ord= order; ord; ord= ord->next)
  {
    if (find_order_in_list(session, ref_pointer_array, tables, ord, fields,
			   all_fields, true))
      return 1;
    (*ord->item)->marker= UNDEF_POS;		/* Mark found */
    if ((*ord->item)->with_sum_func)
    {
      my_error(ER_WRONG_GROUP_FIELD, MYF(0), (*ord->item)->full_name());
      return 1;
    }
  }
  /* MODE_ONLY_FULL_GROUP_BY */
  {
    /*
      Don't allow one to use fields that is not used in GROUP BY
      For each select a list of field references that aren't under an
      aggregate function is created. Each field in this list keeps the
      position of the select list expression which it belongs to.

      First we check an expression from the select list against the GROUP BY
      list. If it's found there then it's ok. It's also ok if this expression
      is a constant or an aggregate function. Otherwise we scan the list
      of non-aggregated fields and if we'll find at least one field reference
      that belongs to this expression and doesn't occur in the GROUP BY list
      we throw an error. If there are no fields in the created list for a
      select list expression this means that all fields in it are used under
      aggregate functions.
    */
    Item *item;
    Item_field *field;
    int cur_pos_in_select_list= 0;
    List<Item>::iterator li(fields.begin());
    List<Item_field>::iterator naf_it(session->lex().current_select->non_agg_fields.begin());

    field= naf_it++;
    while (field && (item=li++))
    {
      if (item->type() != Item::SUM_FUNC_ITEM && item->marker >= 0 &&
          !item->const_item() &&
          !(item->real_item()->type() == Item::FIELD_ITEM &&
            item->used_tables() & OUTER_REF_TABLE_BIT))
      {
        while (field)
        {
          /* Skip fields from previous expressions. */
          if (field->marker < cur_pos_in_select_list)
            goto next_field;
          /* Found a field from the next expression. */
          if (field->marker > cur_pos_in_select_list)
            break;
          /*
            Check whether the field occur in the GROUP BY list.
            Throw the error later if the field isn't found.
          */
          for (ord= order; ord; ord= ord->next)
            if ((*ord->item)->eq((Item*)field, 0))
              goto next_field;
          /*
            @todo change ER_WRONG_FIELD_WITH_GROUP to more detailed ER_NON_GROUPING_FIELD_USED
          */
          my_error(ER_WRONG_FIELD_WITH_GROUP, MYF(0), field->full_name());
          return 1;
next_field:
          field= naf_it++;
        }
      }
      cur_pos_in_select_list++;
    }
  }
  if (org_fields != all_fields.size())
    *hidden_group_fields=1;			// group fields is not used
  return 0;
}

/**
  Create a group by that consist of all non const fields.

  Try to use the fields in the order given by 'order' to allow one to
  optimize away 'order by'.
*/
Order *create_distinct_group(Session *session,
                                Item **ref_pointer_array,
                                Order *order_list,
                                List<Item> &fields,
                                List<Item> &,
                                bool *all_order_by_fields_used)
{
  List<Item>::iterator li(fields.begin());
  Order *order,*group,**prev;

  *all_order_by_fields_used= 1;
  while (Item* item=li++)
    item->marker=0;			/* Marker that field is not used */

  prev= &group;  group=0;
  for (order=order_list ; order; order=order->next)
  {
    if (order->in_field_list)
    {
      Order *ord=(Order*) session->mem.memdup(order,sizeof(Order));
      *prev=ord;
      prev= &ord->next;
      (*ord->item)->marker=1;
    }
    else
      *all_order_by_fields_used= 0;
  }

  li= fields.begin();
  while (Item* item=li++)
  {
    if (!item->const_item() && !item->with_sum_func && !item->marker)
    {
      /*
        Don't put duplicate columns from the SELECT list into the
        GROUP BY list.
      */
      Order *ord_iter;
      for (ord_iter= group; ord_iter; ord_iter= ord_iter->next)
        if ((*ord_iter->item)->eq(item, 1))
          goto next_item;

      Order *ord=(Order*) session->mem.calloc(sizeof(Order));

      /*
        We have here only field_list (not all_field_list), so we can use
        simple indexing of ref_pointer_array (order in the array and in the
        list are same)
      */
      ord->item= ref_pointer_array;
      ord->asc=1;
      *prev=ord;
      prev= &ord->next;
    }
next_item:
    ref_pointer_array++;
  }
  *prev=0;
  return group;
}

/**
  Update join with count of the different type of fields.
*/
void count_field_types(Select_Lex *select_lex, Tmp_Table_Param *param, List<Item> &fields, bool reset_with_sum_func)
{
  List<Item>::iterator li(fields.begin());
  Item *field;

  param->field_count=param->sum_func_count=param->func_count=
    param->hidden_field_count=0;
  param->quick_group=1;
  while ((field=li++))
  {
    Item::Type real_type= field->real_item()->type();
    if (real_type == Item::FIELD_ITEM)
      param->field_count++;
    else if (real_type == Item::SUM_FUNC_ITEM)
    {
      if (! field->const_item())
      {
        Item_sum *sum_item=(Item_sum*) field->real_item();
        if (!sum_item->depended_from() ||
            sum_item->depended_from() == select_lex)
        {
          if (!sum_item->quick_group)
            param->quick_group=0;			// UDF SUM function
          param->sum_func_count++;

          for (uint32_t i=0 ; i < sum_item->arg_count ; i++)
          {
            if (sum_item->args[0]->real_item()->type() == Item::FIELD_ITEM)
              param->field_count++;
            else
              param->func_count++;
          }
        }
        param->func_count++;
      }
    }
    else
    {
      param->func_count++;
      if (reset_with_sum_func)
        field->with_sum_func=0;
    }
  }
}

/*
  Test if a single-row cache of items changed, and update the cache.

  @details Test if a list of items that typically represents a result
  row has changed. If the value of some item changed, update the cached
  value for this item.

  @param list list of <item, cached_value> pairs stored as Cached_item.

  @return -1 if no item changed
  @return index of the first item that changed
*/
int test_if_item_cache_changed(List<Cached_item> &list)
{
  List<Cached_item>::iterator li(list.begin());
  int idx= -1,i;
  Cached_item *buff;

  for (i=(int) list.size()-1 ; (buff=li++) ; i--)
  {
    if (buff->cmp())
      idx=i;
  }
  return(idx);
}

/**
  Setup copy_fields to save fields at start of new group.

  Setup copy_fields to save fields at start of new group

  Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
  Change old item_field to use a new field with points at saved fieldvalue
  This function is only called before use of send_fields.

  @param session                   Session pointer
  @param param                 temporary table parameters
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @todo
    In most cases this result will be sent to the user.
    This should be changed to use copy_int or copy_real depending
    on how the value is to be used: In some cases this may be an
    argument in a group function, like: IF(ISNULL(col),0,COUNT(*))

  @retval
    0     ok
  @retval
    !=0   error
*/
bool setup_copy_fields(Session *session,
                       Tmp_Table_Param *param,
                       Item **ref_pointer_array,
                       List<Item> &res_selected_fields,
                       List<Item> &res_all_fields,
                       uint32_t elements,
                       List<Item> &all_fields)
{
  Item *pos;
  List<Item>::iterator li(all_fields.begin());
  CopyField *copy= NULL;
  res_selected_fields.clear();
  res_all_fields.clear();
  List<Item>::iterator itr(res_all_fields.begin());
  List<Item> extra_funcs;
  uint32_t i, border= all_fields.size() - elements;

  if (param->field_count &&
      !(copy= param->copy_field= new CopyField[param->field_count]))
    return true;

  param->copy_funcs.clear();
  for (i= 0; (pos= li++); i++)
  {
    Field *field;
    unsigned char *tmp;
    Item *real_pos= pos->real_item();
    if (real_pos->type() == Item::FIELD_ITEM)
    {
      Item_field* item= new Item_field(session, ((Item_field*) real_pos));
      if (pos->type() == Item::REF_ITEM)
      {
        /* preserve the names of the ref when dereferncing */
        Item_ref *ref= (Item_ref *) pos;
        item->db_name= ref->db_name;
        item->table_name= ref->table_name;
        item->name= ref->name;
      }
      pos= item;
      if (item->field->flags & BLOB_FLAG)
      {
        pos= new Item_copy_string(pos);
            /*
              Item_copy_string::copy for function can call
              Item_copy_string::val_int for blob via Item_ref.
              But if Item_copy_string::copy for blob isn't called before,
              it's value will be wrong
              so let's insert Item_copy_string for blobs in the beginning of
              copy_funcs
              (to see full test case look at having.test, BUG #4358)
            */
        param->copy_funcs.push_front(pos);
      }
      else
      {
        /*
          set up save buffer and change result_field to point at
          saved value
        */
        field= item->field;
        item->result_field=field->new_field(session->mem_root,field->getTable(), 1);
              /*
                We need to allocate one extra byte for null handling and
                another extra byte to not get warnings from purify in
                Field_varstring::val_int
              */
        if (!(tmp= (unsigned char*) memory::sql_alloc(field->pack_length()+2)))
          goto err;
        if (copy)
        {
          copy->set(tmp, item->result_field);
          item->result_field->move_field(copy->to_ptr,copy->to_null_ptr,1);
#ifdef HAVE_VALGRIND
          copy->to_ptr[copy->from_length]= 0;
#endif
          copy++;
        }
      }
    }
    else if ((real_pos->type() == Item::FUNC_ITEM ||
	      real_pos->type() == Item::SUBSELECT_ITEM ||
	      real_pos->type() == Item::CACHE_ITEM ||
	      real_pos->type() == Item::COND_ITEM) &&
	     !real_pos->with_sum_func)
    {						// Save for send fields
      pos= real_pos;
      /*
        @todo In most cases this result will be sent to the user.
        This should be changed to use copy_int or copy_real depending
        on how the value is to be used: In some cases this may be an
        argument in a group function, like: IF(ISNULL(col),0,COUNT(*))
      */
      pos=new Item_copy_string(pos);
      if (i < border)                           // HAVING, order_st and GROUP BY
        extra_funcs.push_back(pos);
      else 
				param->copy_funcs.push_back(pos);
    }
    res_all_fields.push_back(pos);
    ref_pointer_array[((i < border)? all_fields.size()-i-1 : i-border)]=
      pos;
  }
  param->copy_field_end= copy;

  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);
  /*
    Put elements from HAVING, ORDER BY and GROUP BY last to ensure that any
    reference used in these will resolve to a item that is already calculated
  */
  param->copy_funcs.concat(&extra_funcs);

  return 0;

err:
  if (copy)
    delete [] param->copy_field;			// This is never 0
  param->copy_field=0;
  return true;
}

/**
  Make a copy of all simple SELECT'ed items.

  This is done at the start of a new group so that we can retrieve
  these later when the group changes.
*/
void copy_fields(Tmp_Table_Param *param)
{
  CopyField *ptr= param->copy_field;
  CopyField *end= param->copy_field_end;

  for (; ptr != end; ptr++)
    (*ptr->do_copy)(ptr);

  List<Item>::iterator it(param->copy_funcs.begin());
  Item_copy_string *item;
  while ((item = (Item_copy_string*) it++))
    item->copy();
}

/**
  Change all funcs and sum_funcs to fields in tmp table, and create
  new list of all items.

  @param session                   Session pointer
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @retval
    0     ok
  @retval
    !=0   error
*/
bool change_to_use_tmp_fields(Session *session,
                              Item **ref_pointer_array,
			                        List<Item> &res_selected_fields,
			                        List<Item> &res_all_fields,
			                        uint32_t elements,
                              List<Item> &all_fields)
{
  List<Item>::iterator it(all_fields.begin());
  Item *item_field,*item;

  res_selected_fields.clear();
  res_all_fields.clear();

  uint32_t i, border= all_fields.size() - elements;
  for (i= 0; (item= it++); i++)
  {
    Field *field;

    if ((item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM) ||
        (item->type() == Item::FUNC_ITEM &&
         ((Item_func*)item)->functype() == Item_func::SUSERVAR_FUNC))
      item_field= item;
    else
    {
      if (item->type() == Item::FIELD_ITEM)
      {
        item_field= item->get_tmp_table_item(session);
      }
      else if ((field= item->get_tmp_table_field()))
      {
        if (item->type() == Item::SUM_FUNC_ITEM && field->getTable()->group)
          item_field= ((Item_sum*) item)->result_item(field);
        else
          item_field= (Item*) new Item_field(field);
        if (!item_field)
          return true;                    // Fatal error

        if (item->real_item()->type() != Item::FIELD_ITEM)
          field->orig_table= 0;
        item_field->name= item->name;
        if (item->type() == Item::REF_ITEM)
        {
          Item_field *ifield= (Item_field *) item_field;
          Item_ref *iref= (Item_ref *) item;
          ifield->table_name= iref->table_name;
          ifield->db_name= iref->db_name;
        }
      }
      else
        item_field= item;
    }
    res_all_fields.push_back(item_field);
    ref_pointer_array[((i < border)? all_fields.size()-i-1 : i-border)]=
      item_field;
  }

  List<Item>::iterator itr(res_all_fields.begin());
  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);
  return false;
}

/**
  Change all sum_func refs to fields to point at fields in tmp table.
  Change all funcs to be fields in tmp table.

  @param session                   Session pointer
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @retval
    0	ok
  @retval
    1	error
*/
bool change_refs_to_tmp_fields(Session *session,
                               Item **ref_pointer_array,
                               List<Item> &res_selected_fields,
                               List<Item> &res_all_fields,
                               uint32_t elements,
			                         List<Item> &all_fields)
{
  List<Item>::iterator it(all_fields.begin());
  Item *item, *new_item;
  res_selected_fields.clear();
  res_all_fields.clear();

  uint32_t i, border= all_fields.size() - elements;
  for (i= 0; (item= it++); i++)
  {
    res_all_fields.push_back(new_item= item->get_tmp_table_item(session));
    ref_pointer_array[((i < border)? all_fields.size()-i-1 : i-border)]=
      new_item;
  }

  List<Item>::iterator itr(res_all_fields.begin());
  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);

  return session->is_fatal_error;
}

/******************************************************************************
  Code for calculating functions
******************************************************************************/

/**
  Call ::setup for all sum functions.

  @param session           thread Cursor
  @param func_ptr      sum function list

  @retval
    false  ok
  @retval
    true   error
*/
bool setup_sum_funcs(Session *session, Item_sum **func_ptr)
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
  {
    if (func->setup(session))
      return true;
  }
  return false;
}

void init_tmptable_sum_functions(Item_sum **func_ptr)
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->reset_field();
}

/** Update record 0 in tmp_table from record 1. */
void update_tmptable_sum_func(Item_sum **func_ptr, Table *)
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->update_field();
}

/** Copy result of sum functions to record in tmp_table. */
void copy_sum_funcs(Item_sum **func_ptr, Item_sum **end_ptr)
{
  for (; func_ptr != end_ptr ; func_ptr++)
    (void) (*func_ptr)->save_in_result_field(1);
  return;
}

bool init_sum_functions(Item_sum **func_ptr, Item_sum **end_ptr)
{
  for (; func_ptr != end_ptr ;func_ptr++)
  {
    if ((*func_ptr)->reset())
      return 1;
  }
  /* If rollup, calculate the upper sum levels */
  for ( ; *func_ptr ; func_ptr++)
  {
    if ((*func_ptr)->add())
      return 1;
  }
  return 0;
}

bool update_sum_func(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func= (Item_sum*) *func_ptr) ; func_ptr++)
    if (func->add())
      return 1;
  return 0;
}

/** Copy result of functions to record in tmp_table. */
bool copy_funcs(Item **func_ptr, const Session *session)
{
  Item *func;
  for (; (func = *func_ptr) ; func_ptr++)
  {
    func->save_in_result_field(1);
    /*
      Need to check the THD error state because Item::val_xxx() don't
      return error code, but can generate errors
      @todo change it for a real status check when Item::val_xxx()
      are extended to return status code.
    */
    if (session->is_error())
      return true;
  }
  return false;
}

/**
  Free joins of subselect of this select.

  @param session      Session pointer
  @param select   pointer to Select_Lex which subselects joins we will free
*/
void free_underlaid_joins(Session *, Select_Lex *select)
{
  for (Select_Lex_Unit *unit= select->first_inner_unit();
       unit;
       unit= unit->next_unit())
    unit->cleanup();
}

/****************************************************************************
  ROLLUP handling
****************************************************************************/

/**
  Replace occurences of group by fields in an expression by ref items.

  The function replaces occurrences of group by fields in expr
  by ref objects for these fields unless they are under aggregate
  functions.
  The function also corrects value of the the maybe_null attribute
  for the items of all subexpressions containing group by fields.

  @b EXAMPLES
    @code
      SELECT a+1 FROM t1 GROUP BY a WITH ROLLUP
      SELECT SUM(a)+a FROM t1 GROUP BY a WITH ROLLUP
  @endcode

  @b IMPLEMENTATION

    The function recursively traverses the tree of the expr expression,
    looks for occurrences of the group by fields that are not under
    aggregate functions and replaces them for the corresponding ref items.

  @note
    This substitution is needed GROUP BY queries with ROLLUP if
    SELECT list contains expressions over group by attributes.

  @param session                  reference to the context
  @param expr                 expression to make replacement
  @param group_list           list of references to group by items
  @param changed        out:  returns 1 if item contains a replaced field item

  @todo
    - @todo Some functions are not null-preserving. For those functions
    updating of the maybe_null attribute is an overkill.

  @retval
    0	if ok
  @retval
    1   on error
*/
bool change_group_ref(Session *session, Item_func *expr, Order *group_list, bool *changed)
{
  if (expr->arg_count)
  {
    Name_resolution_context *context= &session->lex().current_select->context;
    Item **arg,**arg_end;
    bool arg_changed= false;
    for (arg= expr->arguments(),
         arg_end= expr->arguments()+expr->arg_count;
         arg != arg_end; arg++)
    {
      Item *item= *arg;
      if (item->type() == Item::FIELD_ITEM || item->type() == Item::REF_ITEM)
      {
        Order *group_tmp;
        for (group_tmp= group_list; group_tmp; group_tmp= group_tmp->next)
        {
          if (item->eq(*group_tmp->item,0))
          {
            Item* new_item= new Item_ref(context, group_tmp->item, 0, item->name);
            *arg= new_item;
            arg_changed= true;
          }
        }
      }
      else if (item->type() == Item::FUNC_ITEM)
      {
        if (change_group_ref(session, (Item_func *) item, group_list, &arg_changed))
          return 1;
      }
    }
    if (arg_changed)
    {
      expr->maybe_null= 1;
      *changed= true;
    }
  }
  return 0;
}


static void print_table_array(Session *session, String *str, TableList **table,
                              TableList **end)
{
  (*table)->print(session, str);

  for (TableList **tbl= table + 1; tbl < end; tbl++)
  {
    TableList *curr= *tbl;
    if (curr->outer_join)
    {
      /* MySQL converts right to left joins */
      str->append(STRING_WITH_LEN(" left join "));
    }
    else if (curr->straight)
      str->append(STRING_WITH_LEN(" straight_join "));
    else
      str->append(STRING_WITH_LEN(" join "));
    curr->print(session, str);
    if (curr->on_expr)
    {
      str->append(STRING_WITH_LEN(" on("));
      curr->on_expr->print(str);
      str->append(')');
    }
  }
}

/**
  Print joins from the FROM clause.
  @param session     thread Cursor
  @param str     string where table should be printed
  @param tables  list of tables in join
*/
void print_join(Session *session, String *str,
                List<TableList> *tables)
{
  /* List is reversed => we should reverse it before using */
  List<TableList>::iterator ti(tables->begin());
  TableList **table= new (session->mem) TableList*[tables->size()];

  for (TableList **t= table + (tables->size() - 1); t >= table; t--)
    *t= ti++;
  assert(tables->size() >= 1);
  print_table_array(session, str, table, table + tables->size());
}

void Select_Lex::print(Session *session, String *str)
{
  /* QQ: session may not be set for sub queries, but this should be fixed */
  if(not session)
    session= current_session;


  str->append(STRING_WITH_LEN("select "));

  /* First add options */
  if (options & SELECT_STRAIGHT_JOIN)
    str->append(STRING_WITH_LEN("straight_join "));
  if (options & SELECT_DISTINCT)
    str->append(STRING_WITH_LEN("distinct "));
  if (options & SELECT_SMALL_RESULT)
    str->append(STRING_WITH_LEN("sql_small_result "));
  if (options & SELECT_BIG_RESULT)
    str->append(STRING_WITH_LEN("sql_big_result "));
  if (options & OPTION_BUFFER_RESULT)
    str->append(STRING_WITH_LEN("sql_buffer_result "));
  if (options & OPTION_FOUND_ROWS)
    str->append(STRING_WITH_LEN("sql_calc_found_rows "));

  //Item List
  bool first= 1;
  List<Item>::iterator it(item_list.begin());
  Item *item;
  while ((item= it++))
  {
    if (first)
      first= 0;
    else
      str->append(',');
    item->print_item_w_name(str);
  }

  /*
    from clause
    @todo support USING/FORCE/IGNORE index
  */
  if (table_list.size())
  {
    str->append(STRING_WITH_LEN(" from "));
    /* go through join tree */
    print_join(session, str, &top_join_list);
  }
  else if (where)
  {
    /*
      "SELECT 1 FROM DUAL WHERE 2" should not be printed as
      "SELECT 1 WHERE 2": the 1st syntax is valid, but the 2nd is not.
    */
    str->append(STRING_WITH_LEN(" from DUAL "));
  }

  // Where
  Item *cur_where= where;
  if (join)
    cur_where= join->conds;
  if (cur_where || cond_value != Item::COND_UNDEF)
  {
    str->append(STRING_WITH_LEN(" where "));
    if (cur_where)
      cur_where->print(str);
    else
      str->append(cond_value != Item::COND_FALSE ? "1" : "0");
  }

  // group by & olap
  if (group_list.size())
  {
    str->append(STRING_WITH_LEN(" group by "));
    print_order(str, (Order *) group_list.first);
    switch (olap)
    {
      case CUBE_TYPE:
	str->append(STRING_WITH_LEN(" with cube"));
	break;
      case ROLLUP_TYPE:
	str->append(STRING_WITH_LEN(" with rollup"));
	break;
      default:
	;  //satisfy compiler
    }
  }

  // having
  Item *cur_having= having;
  if (join)
    cur_having= join->having;

  if (cur_having || having_value != Item::COND_UNDEF)
  {
    str->append(STRING_WITH_LEN(" having "));
    if (cur_having)
      cur_having->print(str);
    else
      str->append(having_value != Item::COND_FALSE ? "1" : "0");
  }

  if (order_list.size())
  {
    str->append(STRING_WITH_LEN(" order by "));
    print_order(str, (Order *) order_list.first);
  }

  // limit
  print_limit(session, str);

  // PROCEDURE unsupported here
}

/**
  @} (end of group Query_Optimizer)
*/

} /* namespace drizzled */
