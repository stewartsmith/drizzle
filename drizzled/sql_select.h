/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/cached_item.h>
#include <drizzled/field/varstring.h>
#include <drizzled/item/null.h>
#include <drizzled/enum_nested_loop_state.h>
#include <drizzled/optimizer/position.h>
#include <drizzled/optimizer/sargable_param.h>
#include <drizzled/optimizer/key_use.h>
#include <drizzled/join_cache.h>
#include <drizzled/join_table.h>
#include <drizzled/records.h>
#include <drizzled/stored_key.h>

#include <vector>

namespace drizzled {

/**
 * @file API and Classes to use when handling where clause
 */

/* PREV_BITS only used in sql_select.cc */
#define PREV_BITS(type,A)	((type) (((type) 1 << (A)) -1))

/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2

enum_nested_loop_state sub_select_cache(Join *join, JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state sub_select(Join *join,JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state end_send_group(Join *join, JoinTable *join_tab, bool end_of_records);
enum_nested_loop_state end_write_group(Join *join, JoinTable *join_tab, bool end_of_records);

class Rollup
{
public:
  enum State { STATE_NONE, STATE_INITED, STATE_READY };

  Rollup()
  :
  state(),
  null_items(NULL),
  ref_pointer_arrays(NULL),
  fields()
  {}
  
  Rollup(State in_state,
         Item_null_result **in_null_items,
         Item ***in_ref_pointer_arrays,
         List<Item> *in_fields)
  :
  state(in_state),
  null_items(in_null_items),
  ref_pointer_arrays(in_ref_pointer_arrays),
  fields(in_fields)
  {}
  
  State getState() const
  {
    return state;
  }

  void setState(State in_state)
  {
    state= in_state;
  }
 
  Item_null_result **getNullItems() const
  {
    return null_items;
  }

  void setNullItems(Item_null_result **in_null_items)
  {
    null_items= in_null_items;
  }

  Item ***getRefPointerArrays() const
  {
    return ref_pointer_arrays;
  }

  void setRefPointerArrays(Item ***in_ref_pointer_arrays)
  {
    ref_pointer_arrays= in_ref_pointer_arrays;
  }

  List<Item> *getFields() const
  {
    return fields;
  }

  void setFields(List<Item> *in_fields)
  {
    fields= in_fields;
  }
  
private:
  State state;
  Item_null_result **null_items;
  Item ***ref_pointer_arrays;
  List<Item> *fields;
};

} /* namespace drizzled */

/** @TODO why is this in the middle of the file??? */

#include <drizzled/join.h>

namespace drizzled
{

/*****************************************************************************
  Make som simple condition optimization:
  If there is a test 'field = const' change all refs to 'field' to 'const'
  Remove all dummy tests 'item = item', 'const op const'.
  Remove all 'item is NULL', when item can never be null!
  item->marker should be 0 for all items on entry
  Return in cond_value false if condition is impossible (1 = 2)
*****************************************************************************/
typedef std::pair<Item*, Item_func*> COND_CMP;

void TEST_join(Join *join);

/* Extern functions in sql_select.cc */
bool store_val_in_field(Field *field, Item *val, enum_check_fields check_flag);
Table *create_tmp_table(Session *session,Tmp_Table_Param *param,List<Item> &fields,
			Order *group, bool distinct, bool save_sum_fields,
			uint64_t select_options, ha_rows rows_limit,
			const char* alias);
void count_field_types(Select_Lex *select_lex, Tmp_Table_Param *param,
                       List<Item> &fields, bool reset_with_sum_func);
bool setup_copy_fields(Session *session, Tmp_Table_Param *param,
		       Item **ref_pointer_array,
		       List<Item> &new_list1, List<Item> &new_list2,
		       uint32_t elements, List<Item> &fields);
void copy_fields(Tmp_Table_Param *param);
bool copy_funcs(Item **func_ptr, const Session *session);
Field* create_tmp_field_from_field(Session *session, Field* org_field,
                                   const char *name, Table *table,
                                   Item_field *item, uint32_t convert_blob_length);
bool test_if_ref(Item_field *left_item,Item *right_item);
COND *optimize_cond(Join *join, COND *conds, List<TableList> *join_list, Item::cond_result *cond_value);
COND *make_cond_for_table(COND *cond,table_map table, table_map used_table, bool exclude_expensive_cond);
COND* substitute_for_best_equal_field(COND *cond, COND_EQUAL *cond_equal, void *table_join_idx);
bool list_contains_unique_index(Table *table, bool (*find_func) (Field *, void *), void *data);
bool find_field_in_order_list (Field *field, void *data);
bool find_field_in_item_list (Field *field, void *data);
bool test_if_skip_sort_order(JoinTable *tab,Order *order,ha_rows select_limit, bool no_changes, const key_map *map);
Order *create_distinct_group(Session *session,
                                Item **ref_pointer_array,
                                Order *order_list,
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
int do_select(Join *join, List<Item> *fields, Table *tmp_table);
bool const_expression_in_where(COND *conds,Item *item, Item **comp_item);
int create_sort_index(Session *session, Join *join, Order *order, ha_rows filesort_limit, ha_rows select_limit, bool is_order_by);
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
bool change_group_ref(Session *session, Item_func *expr, Order *group_list, bool *changed);
bool check_interleaving_with_nj(JoinTable *next);
void update_const_equal_items(COND *cond, JoinTable *tab);
int join_read_const(JoinTable *tab);
int join_read_key(JoinTable *tab);
int join_read_always_key(JoinTable *tab);
int join_read_last_key(JoinTable *tab);
int join_no_more_records(ReadRecord *info);
int join_read_next(ReadRecord *info);
int join_read_next_different(ReadRecord *info);
int join_init_quick_read_record(JoinTable *tab);
int init_read_record_seq(JoinTable *tab);
int test_if_quick_select(JoinTable *tab);
int join_init_read_record(JoinTable *tab);
int join_read_first(JoinTable *tab);
int join_read_next_same(ReadRecord *info);
int join_read_next_same_diff(ReadRecord *info);
int join_read_last(JoinTable *tab);
int join_read_prev_same(ReadRecord *info);
int join_read_prev(ReadRecord *info);
int join_read_always_key_or_null(JoinTable *tab);
int join_read_next_same_or_null(ReadRecord *info);

void calc_used_field_length(Session *, JoinTable *join_tab);
StoredKey *get_store_key(Session *session, 
                         optimizer::KeyUse *keyuse,
                         table_map used_tables,
                         KeyPartInfo *key_part,
                         unsigned char *key_buff,
                         uint32_t maybe_null);
int join_tab_cmp(const void* ptr1, const void* ptr2);
int join_tab_cmp_straight(const void* ptr1, const void* ptr2);
void push_index_cond(JoinTable *tab, uint32_t keyno, bool other_tbls_ok);
void add_not_null_conds(Join *join);
uint32_t max_part_bit(key_part_map bits);
COND *add_found_match_trig_cond(JoinTable *tab, COND *cond, JoinTable *root_tab);
bool eq_ref_table(Join *join, Order *start_order, JoinTable *tab);
int remove_dup_with_compare(Session *session, Table *table, Field **first_field, uint32_t offset, Item *having);
int remove_dup_with_hash_index(Session *session, 
                               Table *table,
                               uint32_t field_count,
                               Field **first_field,
                               uint32_t key_length,
                               Item *having);
void update_ref_and_keys(Session *session,
                         DYNAMIC_ARRAY *keyuse,
                         JoinTable *join_tab,
                         uint32_t tables,
                         COND *cond, 
                         COND_EQUAL *,
                         table_map normal_tables,
                         Select_Lex *select_lex,
                         std::vector<optimizer::SargableParam> &sargables);
ha_rows get_quick_record_count(Session *session, optimizer::SqlSelect *select, Table *table, const key_map *keys,ha_rows limit);
void optimize_keyuse(Join *join, DYNAMIC_ARRAY *keyuse_array);
void add_group_and_distinct_keys(Join *join, JoinTable *join_tab);
void read_cached_record(JoinTable *tab);
bool select_query(Session *session, Item ***rref_pointer_array,
                  TableList *tables, uint32_t wild_num,  List<Item> &list,
                  COND *conds, uint32_t og_num, Order *order, Order *group,
                  Item *having, uint64_t select_type,
                  select_result *result, Select_Lex_Unit *unit,
                  Select_Lex *select_lex);
// Create list for using with tempory table
void init_tmptable_sum_functions(Item_sum **func);
void update_tmptable_sum_func(Item_sum **func,Table *tmp_table);
bool only_eq_ref_tables(Join *join, Order *order, table_map tables);
bool create_ref_for_key(Join *join, JoinTable *j, 
                        optimizer::KeyUse *org_keyuse, 
                        table_map used_tables);

bool cp_buffer_from_ref(Session *session, table_reference_st *ref);
int safe_index_read(JoinTable *tab);
COND *remove_eq_conds(Session *session, COND *cond, Item::cond_result *cond_value);
int test_if_item_cache_changed(List<Cached_item> &list);

void print_join(Session *session, String *str, List<TableList> *tables);

} /* namespace drizzled */

