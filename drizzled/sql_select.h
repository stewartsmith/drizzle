/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef DRIZZLED_SQL_SELECT_H
#define DRIZZLED_SQL_SELECT_H

#include "drizzled/cached_item.h"
#include "drizzled/session.h"
#include "drizzled/field/varstring.h"
#include "drizzled/item/null.h"
#include <drizzled/enum_nested_loop_state.h>
#include "drizzled/join_cache.h"
#include "drizzled/join_table.h"


class select_result;

/**
 * @file API and Classes to use when handling where clause
 */

/* PREV_BITS only used in sql_select.cc */
#define PREV_BITS(type,A)	((type) (((type) 1 << (A)) -1))

#include <plugin/myisam/myisam.h>
#include <drizzled/sql_array.h>

/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2

class KeyUse 
{
public:
  Table *table; /**< Pointer to the table this key belongs to */
  Item *val;	/**< or value if no field */
  table_map used_tables;
  uint32_t key;
  uint32_t keypart;
  uint32_t optimize; /**< 0, or KEY_OPTIMIZE_* */
  key_part_map keypart_map;
  ha_rows ref_table_rows;
  /**
    If true, the comparison this value was created from will not be
    satisfied if val has NULL 'value'.
  */
  bool null_rejecting;
  /**
    !NULL - This KeyUse was created from an equality that was wrapped into
            an Item_func_trig_cond. This means the equality (and validity of
            this KeyUse element) can be turned on and off. The on/off state
            is indicted by the pointed value:
              *cond_guard == true <=> equality condition is on
              *cond_guard == false <=> equality condition is off

    NULL  - Otherwise (the source equality can't be turned off)
  */
  bool *cond_guard;
};

class JOIN;

enum_nested_loop_state sub_select_cache(JOIN *join, JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state sub_select(JOIN *join,JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state end_send_group(JOIN *join, JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state end_write_group(JOIN *join, JoinTable *join_tab, bool end_of_records);

/**
 * Information about a position of table within a join order. Used in join
 * optimization.
 */
class Position
{
public:

  Position()
    :
      records_read(0),
      read_time(0),
      table(NULL),
      key(NULL),
      ref_depend_map(0),
      use_insideout_scan(false)
  {}

  /**
    The "fanout": number of output rows that will be produced (after
    pushed down selection condition is applied) per each row combination of
    previous tables.
  */
  double records_read;

  /**
    Cost accessing the table in course of the entire complete join execution,
    i.e. cost of one access method use (e.g. 'range' or 'ref' scan ) times
    number the access method will be invoked.
  */
  double read_time;
  JoinTable *table;

  /**
    NULL  -  'index' or 'range' or 'index_merge' or 'ALL' access is used.
    Other - [eq_]ref[_or_null] access is used. Pointer to {t.keypart1 = expr}
  */
  KeyUse *key;

  /** If ref-based access is used: bitmap of tables this table depends on  */
  table_map ref_depend_map;

  bool use_insideout_scan;
};

typedef struct st_rollup
{
  enum State { STATE_NONE, STATE_INITED, STATE_READY };
  State state;
  Item_null_result **null_items;
  Item ***ref_pointer_arrays;
  List<Item> *fields;
} ROLLUP;

#include "drizzled/join.h"

typedef struct st_select_check {
  uint32_t const_ref,reg_ref;
} SELECT_CHECK;

/*
   This structure is used to collect info on potentially sargable
   predicates in order to check whether they become sargable after
   reading const tables.
   We form a bitmap of indexes that can be used for sargable predicates.
   Only such indexes are involved in range analysis.
*/
typedef struct st_sargable_param
{
  Field *field;              /* field against which to check sargability */
  Item **arg_value;          /* values of potential keys for lookups     */
  uint32_t num_values;           /* number of values in the above array      */
} SARGABLE_PARAM;

/**
 * Structure used when finding key fields
 */
typedef struct key_field_t 
{
  Field *field;
  Item *val; /**< May be empty if diff constant */
  uint32_t level;
  uint32_t optimize; /**< KEY_OPTIMIZE_* */
  bool eq_func;
  /**
    If true, the condition this struct represents will not be satisfied
    when val IS NULL.
  */
  bool null_rejecting;
  bool *cond_guard; /**< @see KeyUse::cond_guard */
} KEY_FIELD;

/*****************************************************************************
  Make som simple condition optimization:
  If there is a test 'field = const' change all refs to 'field' to 'const'
  Remove all dummy tests 'item = item', 'const op const'.
  Remove all 'item is NULL', when item can never be null!
  item->marker should be 0 for all items on entry
  Return in cond_value false if condition is impossible (1 = 2)
*****************************************************************************/
struct COND_CMP {
  Item *and_level;
  Item_func *cmp_func;
  COND_CMP(Item *a,Item_func *b) :and_level(a),cmp_func(b) {}
};

void TEST_join(JOIN *join);

/* Extern functions in sql_select.cc */
bool store_val_in_field(Field *field, Item *val, enum_check_fields check_flag);
Table *create_tmp_table(Session *session,Tmp_Table_Param *param,List<Item> &fields,
			order_st *group, bool distinct, bool save_sum_fields,
			uint64_t select_options, ha_rows rows_limit,
			const char* alias);
void free_tmp_table(Session *session, Table *entry);
void count_field_types(Select_Lex *select_lex, Tmp_Table_Param *param,
                       List<Item> &fields, bool reset_with_sum_func);
bool setup_copy_fields(Session *session, Tmp_Table_Param *param,
		       Item **ref_pointer_array,
		       List<Item> &new_list1, List<Item> &new_list2,
		       uint32_t elements, List<Item> &fields);
void copy_fields(Tmp_Table_Param *param);
void copy_funcs(Item **func_ptr);
Field* create_tmp_field_from_field(Session *session, Field* org_field,
                                   const char *name, Table *table,
                                   Item_field *item, uint32_t convert_blob_length);
bool test_if_ref(Item_field *left_item,Item *right_item);
COND *optimize_cond(JOIN *join, COND *conds, List<TableList> *join_list, Item::cond_result *cond_value);
COND *make_cond_for_table(COND *cond,table_map table, table_map used_table, bool exclude_expensive_cond);
COND* substitute_for_best_equal_field(COND *cond, COND_EQUAL *cond_equal, void *table_join_idx);
bool list_contains_unique_index(Table *table, bool (*find_func) (Field *, void *), void *data);
bool find_field_in_order_list (Field *field, void *data);
bool find_field_in_item_list (Field *field, void *data);
bool test_if_skip_sort_order(JoinTable *tab,order_st *order,ha_rows select_limit, bool no_changes, const key_map *map);
order_st *create_distinct_group(Session *session,
                                Item **ref_pointer_array,
                                order_st *order_list,
                                List<Item> &fields,
                                List<Item> &,
                                bool *all_order_by_fields_used);
// Create list for using with tempory table
bool change_to_use_tmp_fields(Session *session,
                              Item **ref_pointer_array,
			                        List<Item> &res_selected_fields,
			                        List<Item> &res_all_fields,
			                        uint32_t elements,
                              List<Item> &all_fields);
int do_select(JOIN *join, List<Item> *fields, Table *tmp_table);
bool const_expression_in_where(COND *conds,Item *item, Item **comp_item);
int create_sort_index(Session *session, JOIN *join, order_st *order, ha_rows filesort_limit, ha_rows select_limit, bool is_order_by);
void save_index_subquery_explain_info(JoinTable *join_tab, Item* where);
Item *remove_additional_cond(Item* conds);
bool setup_sum_funcs(Session *session, Item_sum **func_ptr);
bool init_sum_functions(Item_sum **func, Item_sum **end);
bool update_sum_func(Item_sum **func);
void copy_sum_funcs(Item_sum **func_ptr, Item_sum **end);
bool change_refs_to_tmp_fields(Session *session,
                               Item **ref_pointer_array,
                               List<Item> &res_selected_fields,
                               List<Item> &res_all_fields,
                               uint32_t elements,
			                         List<Item> &all_fields);
void select_describe(JOIN *join, bool need_tmp_table,bool need_order, bool distinct, const char *message= NULL);
bool change_group_ref(Session *session, Item_func *expr, order_st *group_list, bool *changed);
bool check_interleaving_with_nj(JoinTable *last, JoinTable *next);

int join_read_const_table(JoinTable *tab, Position *pos);
int join_read_system(JoinTable *tab);
int join_read_const(JoinTable *tab);
int join_read_key(JoinTable *tab);
int join_read_always_key(JoinTable *tab);
int join_read_last_key(JoinTable *tab);
int join_no_more_records(READ_RECORD *info);
int join_read_next(READ_RECORD *info);
int join_read_next_different(READ_RECORD *info);
int join_init_quick_read_record(JoinTable *tab);
int init_read_record_seq(JoinTable *tab);
int test_if_quick_select(JoinTable *tab);
int join_init_read_record(JoinTable *tab);
int join_read_first(JoinTable *tab);
int join_read_next_same(READ_RECORD *info);
int join_read_next_same_diff(READ_RECORD *info);
int join_read_last(JoinTable *tab);
int join_read_prev_same(READ_RECORD *info);
int join_read_prev(READ_RECORD *info);
int join_read_always_key_or_null(JoinTable *tab);
int join_read_next_same_or_null(READ_RECORD *info);

void calc_used_field_length(Session *, JoinTable *join_tab);
StoredKey *get_store_key(Session *session, 
                         KeyUse *keyuse,
                         table_map used_tables,
                         KEY_PART_INFO *key_part,
                         unsigned char *key_buff,
                         uint32_t maybe_null);
extern "C" int join_tab_cmp(const void* ptr1, const void* ptr2);
extern "C" int join_tab_cmp_straight(const void* ptr1, const void* ptr2);
void push_index_cond(JoinTable *tab, uint32_t keyno, bool other_tbls_ok);
void add_not_null_conds(JOIN *join);
uint32_t max_part_bit(key_part_map bits);
COND *add_found_match_trig_cond(JoinTable *tab, COND *cond, JoinTable *root_tab);
order_st *create_distinct_group(Session *session,
                                Item **ref_pointer_array,
                                order_st *order,
                                List<Item> &fields,
                                List<Item> &all_fields,
                                bool *all_order_by_fields_used);
bool eq_ref_table(JOIN *join, order_st *start_order, JoinTable *tab);
int join_tab_cmp(const void* ptr1, const void* ptr2);
int remove_dup_with_compare(Session *session, Table *table, Field **first_field, uint32_t offset, Item *having);
int remove_dup_with_hash_index(Session *session, 
                               Table *table,
                               uint32_t field_count,
                               Field **first_field,
                               uint32_t key_length,
                               Item *having);
bool update_ref_and_keys(Session *session,
                         DYNAMIC_ARRAY *keyuse,
                         JoinTable *join_tab,
                         uint32_t tables,
                         COND *cond, 
                         COND_EQUAL *,
                         table_map normal_tables,
                         Select_Lex *select_lex,
                         SARGABLE_PARAM **sargables);
ha_rows get_quick_record_count(Session *session, SQL_SELECT *select, Table *table, const key_map *keys,ha_rows limit);
void optimize_keyuse(JOIN *join, DYNAMIC_ARRAY *keyuse_array);
void add_group_and_distinct_keys(JOIN *join, JoinTable *join_tab);
void read_cached_record(JoinTable *tab);
// Create list for using with tempory table
void init_tmptable_sum_functions(Item_sum **func);
void update_tmptable_sum_func(Item_sum **func,Table *tmp_table);
bool only_eq_ref_tables(JOIN *join, order_st *order, table_map tables);
bool create_ref_for_key(JOIN *join, JoinTable *j, KeyUse *org_keyuse, table_map used_tables);

/* functions from opt_sum.cc */
bool simple_pred(Item_func *func_item, Item **args, bool *inv_order);
int opt_sum_query(TableList *tables, List<Item> &all_fields,COND *conds);

/* from sql_delete.cc, used by opt_range.cc */
extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b);

#include "drizzled/stored_key.h"

bool cp_buffer_from_ref(Session *session, table_reference_st *ref);
int safe_index_read(JoinTable *tab);
COND *remove_eq_conds(Session *session, COND *cond, Item::cond_result *cond_value);
int test_if_item_cache_changed(List<Cached_item> &list);

void print_join(Session *session, String *str,
                List<TableList> *tables, enum_query_type);

#endif /* DRIZZLED_SQL_SELECT_H */
