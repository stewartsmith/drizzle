/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/cached_item.h>
#include <drizzled/session.h>
#include <drizzled/field/varstring.h>
#include <drizzled/item/null.h>

class select_result;

/**
  @file

  @brief
  classes to use when handling where clause
*/


/* PREV_BITS only used in sql_select.cc */
#define PREV_BITS(type,A)	((type) (((type) 1 << (A)) -1))

#include <plugin/myisam/myisam.h>
#include <drizzled/sql_array.h>

/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2

typedef struct keyuse_t {
  Table *table;
  Item	*val;				/**< or value if no field */
  table_map used_tables;
  uint	key, keypart;
  uint32_t optimize; // 0, or KEY_OPTIMIZE_*
  key_part_map keypart_map;
  ha_rows      ref_table_rows;
  /**
    If true, the comparison this value was created from will not be
    satisfied if val has NULL 'value'.
  */
  bool null_rejecting;
  /*
    !NULL - This KEYUSE was created from an equality that was wrapped into
            an Item_func_trig_cond. This means the equality (and validity of
            this KEYUSE element) can be turned on and off. The on/off state
            is indicted by the pointed value:
              *cond_guard == true <=> equality condition is on
              *cond_guard == false <=> equality condition is off

    NULL  - Otherwise (the source equality can't be turned off)
  */
  bool *cond_guard;
  /*
     0..64    <=> This was created from semi-join IN-equality # sj_pred_no.
     MAX_UINT  Otherwise
  */
  uint32_t         sj_pred_no;
} KEYUSE;

class StoredKey;

typedef struct st_table_ref
{
  bool		key_err;
  uint32_t      key_parts;                ///< num of ...
  uint32_t      key_length;               ///< length of key_buff
  int32_t       key;                      ///< key no
  unsigned char *key_buff;                ///< value to look for with key
  unsigned char *key_buff2;               ///< key_buff+key_length
  StoredKey     **key_copy;               //
  Item          **items;                  ///< val()'s for each keypart
  /*
    Array of pointers to trigger variables. Some/all of the pointers may be
    NULL.  The ref access can be used iff

      for each used key part i, (!cond_guards[i] || *cond_guards[i])

    This array is used by subquery code. The subquery code may inject
    triggered conditions, i.e. conditions that can be 'switched off'. A ref
    access created from such condition is not valid when at least one of the
    underlying conditions is switched off (see subquery code for more details)
  */
  bool          **cond_guards;
  /**
    (null_rejecting & (1<<i)) means the condition is '=' and no matching
    rows will be produced if items[i] IS NULL (see add_not_null_conds())
  */
  key_part_map  null_rejecting;
  table_map	depend_map;		  ///< Table depends on these tables.
  /* null byte position in the key_buf. Used for REF_OR_NULL optimization */
  unsigned char *null_ref_key;

  /*
    true <=> disable the "cache" as doing lookup with the same key value may
    produce different results (because of Index Condition Pushdown)
  */
  bool          disable_cache;
} TABLE_REF;

class JOIN;

#include "drizzled/join_cache.h"

/** The states in which a nested loop join can be in */
enum enum_nested_loop_state
{
  NESTED_LOOP_KILLED= -2,
  NESTED_LOOP_ERROR= -1,
  NESTED_LOOP_OK= 0,
  NESTED_LOOP_NO_MORE_ROWS= 1,
  NESTED_LOOP_QUERY_LIMIT= 3,
  NESTED_LOOP_CURSOR_LIMIT= 4
};

/** Description of a join type */
enum join_type 
{ 
  JT_UNKNOWN,
  JT_SYSTEM,
  JT_CONST,
  JT_EQ_REF,
  JT_REF,
  JT_MAYBE_REF,
	JT_ALL,
  JT_RANGE,
  JT_NEXT,
  JT_REF_OR_NULL,
  JT_UNIQUE_SUBQUERY,
  JT_INDEX_SUBQUERY,
  JT_INDEX_MERGE
};

/* Values for JOIN_TAB::packed_info */
#define TAB_INFO_HAVE_VALUE 1
#define TAB_INFO_USING_INDEX 2
#define TAB_INFO_USING_WHERE 4
#define TAB_INFO_FULL_SCAN_ON_NULL 8

class SJ_TMP_TABLE;

typedef enum_nested_loop_state (*Next_select_func)(JOIN *, struct st_join_table *, bool);
typedef int (*Read_record_func)(struct st_join_table *tab);
Next_select_func setup_end_select_func(JOIN *join);

typedef struct st_join_table {
  st_join_table() {}                          /* Remove gcc warning */
  Table		*table;
  KEYUSE	*keyuse;			/**< pointer to first used key */
  SQL_SELECT	*select;
  COND		*select_cond;
  QUICK_SELECT_I *quick;
  /*
    The value of select_cond before we've attempted to do Index Condition
    Pushdown. We may need to restore everything back if we first choose one
    index but then reconsider (see test_if_skip_sort_order() for such
    scenarios).
    NULL means no index condition pushdown was performed.
  */
  Item          *pre_idx_push_select_cond;
  Item	       **on_expr_ref;   /**< pointer to the associated on expression   */
  COND_EQUAL    *cond_equal;    /**< multiple equalities for the on expression */
  st_join_table *first_inner;   /**< first inner table for including outerjoin */
  bool           found;         /**< true after all matches or null complement */
  bool           not_null_compl;/**< true before null complement is added      */
  st_join_table *last_inner;    /**< last table table for embedding outer join */
  st_join_table *first_upper;  /**< first inner table for embedding outer join */
  st_join_table *first_unmatched; /**< used for optimization purposes only     */

  /* Special content for EXPLAIN 'Extra' column or NULL if none */
  const char	*info;
  /*
    Bitmap of TAB_INFO_* bits that encodes special line for EXPLAIN 'Extra'
    column, or 0 if there is no info.
  */
  uint32_t          packed_info;

  Read_record_func read_first_record;
  Next_select_func next_select;
  READ_RECORD	read_record;
  /*
    Currently the following two fields are used only for a [NOT] IN subquery
    if it is executed by an alternative full table scan when the left operand of
    the subquery predicate is evaluated to NULL.
  */
  Read_record_func save_read_first_record;/* to save read_first_record */
  int (*save_read_record) (READ_RECORD *);/* to save read_record.read_record */
  double	worst_seeks;
  key_map	const_keys;			/**< Keys with constant part */
  key_map	checked_keys;			/**< Keys checked in find_best */
  key_map	needed_reg;
  key_map       keys;                           /**< all keys with can be used */

  /* Either #rows in the table or 1 for const table.  */
  ha_rows	records;
  /*
    Number of records that will be scanned (yes scanned, not returned) by the
    best 'independent' access method, i.e. table scan or QUICK_*_SELECT)
  */
  ha_rows       found_records;
  /*
    Cost of accessing the table using "ALL" or range/index_merge access
    method (but not 'index' for some reason), i.e. this matches method which
    E(#records) is in found_records.
  */
  ha_rows       read_time;

  table_map	dependent,key_dependent;
  uint		use_quick,index;
  uint		status;				///< Save status for cache
  uint		used_fields,used_fieldlength,used_blobs;
  enum join_type type;
  bool		cached_eq_ref_table,eq_ref_table,not_used_in_distinct;
  /* true <=> index-based access method must return records in order */
  bool		sorted;
  /*
    If it's not 0 the number stored this field indicates that the index
    scan has been chosen to access the table data and we expect to scan
    this number of rows for the table.
  */
  ha_rows       limit;
  TABLE_REF	ref;
  JOIN_CACHE	cache;
  JOIN		*join;
  /** Bitmap of nested joins this table is part of */

  /* SemiJoinDuplicateElimination variables: */
  /*
    Embedding SJ-nest (may be not the direct parent), or NULL if none.
    This variable holds the result of table pullout.
  */
  TableList    *emb_sj_nest;

  /* Variables for semi-join duplicate elimination */
  SJ_TMP_TABLE  *flush_weedout_table;
  SJ_TMP_TABLE  *check_weed_out_table;
  struct st_join_table  *do_firstmatch;

  /*
     ptr  - this join tab should do an InsideOut scan. Points
            to the tab for which we'll need to check tab->found_match.

     NULL - Not an insideout scan.
  */
  struct st_join_table *insideout_match_tab;
  unsigned char *insideout_buf; // Buffer to save index tuple to be able to skip dups

  /* Used by InsideOut scan. Just set to true when have found a row. */
  bool found_match;

  enum {
    /* If set, the rowid of this table must be put into the temptable. */
    KEEP_ROWID=1,
    /*
      If set, one should call h->position() to obtain the rowid,
      otherwise, the rowid is assumed to already be in h->ref
      (this is because join caching and filesort() save the rowid and then
      put it back into h->ref)
    */
    CALL_POSITION=2
  };
  /* A set of flags from the above enum */
  int  rowid_keep_flags;


  /* NestedOuterJoins: Bitmap of nested joins this table is part of */
  nested_join_map embedding_map;

  void cleanup();
  inline bool is_using_loose_index_scan()
  {
    return (select && select->quick &&
            (select->quick->get_type() ==
             QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX));
  }
} JOIN_TAB;

enum_nested_loop_state sub_select_cache(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
enum_nested_loop_state sub_select(JOIN *join,JOIN_TAB *join_tab, bool end_of_records);
enum_nested_loop_state end_send_group(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
enum_nested_loop_state end_write_group(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);

/**
  Information about a position of table within a join order. Used in join
  optimization.
*/
typedef struct st_position
{
  /*
    The "fanout": number of output rows that will be produced (after
    pushed down selection condition is applied) per each row combination of
    previous tables.
  */
  double records_read;

  /*
    Cost accessing the table in course of the entire complete join execution,
    i.e. cost of one access method use (e.g. 'range' or 'ref' scan ) times
    number the access method will be invoked.
  */
  double read_time;
  JOIN_TAB *table;

  /*
    NULL  -  'index' or 'range' or 'index_merge' or 'ALL' access is used.
    Other - [eq_]ref[_or_null] access is used. Pointer to {t.keypart1 = expr}
  */
  KEYUSE *key;

  /* If ref-based access is used: bitmap of tables this table depends on  */
  table_map ref_depend_map;

  bool use_insideout_scan;
} POSITION;

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

/// Used when finding key fields
typedef struct key_field_t {
  Field		*field;
  Item		*val;			///< May be empty if diff constant
  uint		level;
  uint		optimize; // KEY_OPTIMIZE_*
  bool		eq_func;
  /**
    If true, the condition this struct represents will not be satisfied
    when val IS NULL.
  */
  bool          null_rejecting;
  bool          *cond_guard; /* See KEYUSE::cond_guard */
  uint32_t          sj_pred_no; /* See KEYUSE::sj_pred_no */
} KEY_FIELD;

/*****************************************************************************
  Make som simple condition optimization:
  If there is a test 'field = const' change all refs to 'field' to 'const'
  Remove all dummy tests 'item = item', 'const op const'.
  Remove all 'item is NULL', when item can never be null!
  item->marker should be 0 for all items on entry
  Return in cond_value false if condition is impossible (1 = 2)
*****************************************************************************/
class COND_CMP :public ilink {
public:
  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint32_t) size);
  }
  static void operator delete(void *, size_t)
  { TRASH(ptr, size); }

  Item *and_level;
  Item_func *cmp_func;
  COND_CMP(Item *a,Item_func *b) :and_level(a),cmp_func(b) {}
};

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class I_List<COND_CMP>;
template class I_List_iterator<COND_CMP>;
#endif

extern const char *join_type_str[];
void TEST_join(JOIN *join);

/* Extern functions in sql_select.cc */
bool store_val_in_field(Field *field, Item *val, enum_check_fields check_flag);
Table *create_tmp_table(Session *session,Tmp_Table_Param *param,List<Item> &fields,
			order_st *group, bool distinct, bool save_sum_fields,
			uint64_t select_options, ha_rows rows_limit,
			char* alias);
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
bool test_if_skip_sort_order(JOIN_TAB *tab,order_st *order,ha_rows select_limit, bool no_changes, const key_map *map);
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
void advance_sj_state(const table_map remaining_tables, const JOIN_TAB *tab);
void restore_prev_sj_state(const table_map remaining_tables, const JOIN_TAB *tab);
void save_index_subquery_explain_info(JOIN_TAB *join_tab, Item* where);
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
int subq_sj_candidate_cmp(Item_in_subselect* const *el1, Item_in_subselect* const *el2);
bool convert_subq_to_sj(JOIN *parent_join, Item_in_subselect *subq_pred);
bool change_group_ref(Session *session, Item_func *expr, order_st *group_list, bool *changed);
bool check_interleaving_with_nj(JOIN_TAB *last, JOIN_TAB *next);

int join_read_const_table(JOIN_TAB *tab, POSITION *pos);
int join_read_system(JOIN_TAB *tab);
int join_read_const(JOIN_TAB *tab);
int join_read_key(JOIN_TAB *tab);
int join_read_always_key(JOIN_TAB *tab);
int join_read_last_key(JOIN_TAB *tab);
int join_no_more_records(READ_RECORD *info);
int join_read_next(READ_RECORD *info);
int join_read_next_different(READ_RECORD *info);
int join_init_quick_read_record(JOIN_TAB *tab);
int test_if_quick_select(JOIN_TAB *tab);
int join_init_read_record(JOIN_TAB *tab);
int join_read_first(JOIN_TAB *tab);
int join_read_next_same(READ_RECORD *info);
int join_read_next_same_diff(READ_RECORD *info);
int join_read_last(JOIN_TAB *tab);
int join_read_prev_same(READ_RECORD *info);
int join_read_prev(READ_RECORD *info);
int join_read_always_key_or_null(JOIN_TAB *tab);
int join_read_next_same_or_null(READ_RECORD *info);

void calc_used_field_length(Session *, JOIN_TAB *join_tab);
StoredKey *get_store_key(Session *session, 
                         KEYUSE *keyuse,
                         table_map used_tables,
                         KEY_PART_INFO *key_part,
                         unsigned char *key_buff,
                         uint32_t maybe_null);
extern "C" int join_tab_cmp(const void* ptr1, const void* ptr2);
extern "C" int join_tab_cmp_straight(const void* ptr1, const void* ptr2);
void push_index_cond(JOIN_TAB *tab, uint32_t keyno, bool other_tbls_ok);
void add_not_null_conds(JOIN *join);
uint32_t max_part_bit(key_part_map bits);
COND *add_found_match_trig_cond(JOIN_TAB *tab, COND *cond, JOIN_TAB *root_tab);
order_st *create_distinct_group(Session *session,
                                Item **ref_pointer_array,
                                order_st *order,
                                List<Item> &fields,
                                List<Item> &all_fields,
                                bool *all_order_by_fields_used);
bool eq_ref_table(JOIN *join, order_st *start_order, JOIN_TAB *tab);
uint64_t get_bound_sj_equalities(TableList *sj_nest, table_map remaining_tables);
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
                         JOIN_TAB *join_tab,
                         uint32_t tables,
                         COND *cond, 
                         COND_EQUAL *,
                         table_map normal_tables,
                         Select_Lex *select_lex,
                         SARGABLE_PARAM **sargables);
ha_rows get_quick_record_count(Session *session, SQL_SELECT *select, Table *table, const key_map *keys,ha_rows limit);
void optimize_keyuse(JOIN *join, DYNAMIC_ARRAY *keyuse_array);
void add_group_and_distinct_keys(JOIN *join, JOIN_TAB *join_tab);
int do_sj_reset(SJ_TMP_TABLE *sj_tbl);
void read_cached_record(JOIN_TAB *tab);
// Create list for using with tempory table
void init_tmptable_sum_functions(Item_sum **func);
void update_tmptable_sum_func(Item_sum **func,Table *tmp_table);
bool find_eq_ref_candidate(Table *table, table_map sj_inner_tables);
bool only_eq_ref_tables(JOIN *join, order_st *order, table_map tables);
bool create_ref_for_key(JOIN *join, JOIN_TAB *j, KEYUSE *org_keyuse, table_map used_tables);

/* functions from opt_sum.cc */
bool simple_pred(Item_func *func_item, Item **args, bool *inv_order);
int opt_sum_query(TableList *tables, List<Item> &all_fields,COND *conds);

/* from sql_delete.cc, used by opt_range.cc */
extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b);

#include "drizzled/stored_key.h"

bool cp_buffer_from_ref(Session *session, TABLE_REF *ref);
bool error_if_full_join(JOIN *join);
int safe_index_read(JOIN_TAB *tab);
COND *remove_eq_conds(Session *session, COND *cond, Item::cond_result *cond_value);
int test_if_item_cache_changed(List<Cached_item> &list);

#endif /* DRIZZLED_SQL_SELECT_H */
