/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

/**
  @defgroup Semantic_Analysis Semantic Analysis
*/
#include <drizzled/message/table.pb.h>
#include <drizzled/name_resolution_context.h>
#include <drizzled/table_list.h>
#include <drizzled/function/math/real.h>
#include <drizzled/key_part_spec.h>
#include <drizzled/index_hint.h>
#include <drizzled/optimizer/explain_plan.h>

#include <bitset>
#include <string>

/*
  The following hack is needed because mysql_yacc.cc does not define
  YYSTYPE before including this file
*/

#ifdef DRIZZLE_SERVER
/* set_var should change to set_var here ... */
# include <drizzled/sys_var.h>
# include <drizzled/item/func.h>
# ifdef DRIZZLE_YACC
#  define LEX_YYSTYPE void *
# else
#  if defined(DRIZZLE_LEX)
#   include <drizzled/foreign_key.h>
#   include <drizzled/lex_symbol.h>
#   include <drizzled/comp_creator.h>
#   include <drizzled/sql_yacc.h>
#   define LEX_YYSTYPE YYSTYPE *
#  else
#   define LEX_YYSTYPE void *
#  endif /* defined(DRIZZLE_LEX) */
# endif /* DRIZZLE_YACC */
#endif /* DRIZZLE_SERVER */

// describe/explain types
#define DESCRIBE_NORMAL		1
#define DESCRIBE_EXTENDED	2

#ifdef DRIZZLE_SERVER

#define DERIVED_NONE	0
#define DERIVED_SUBQUERY	1

namespace drizzled
{

typedef List<Item> List_item;

enum sub_select_type
{
  UNSPECIFIED_TYPE,
  UNION_TYPE,
  INTERSECT_TYPE,
  EXCEPT_TYPE,
  GLOBAL_OPTIONS_TYPE,
  DERIVED_TABLE_TYPE,
  OLAP_TYPE
};

enum olap_type
{
  UNSPECIFIED_OLAP_TYPE,
  CUBE_TYPE,
  ROLLUP_TYPE
};

/*
  The state of the lex parsing for selects

   master and slaves are pointers to select_lex.
   master is pointer to upper level node.
   slave is pointer to lower level node
   select_lex is a SELECT without union
   unit is container of either
     - One SELECT
     - UNION of selects
   select_lex and unit are both inherited form select_lex_node
   neighbors are two select_lex or units on the same level

   All select describing structures linked with following pointers:
   - list of neighbors (next/prev) (prev of first element point to slave
     pointer of upper structure)
     - For select this is a list of UNION's (or one element list)
     - For units this is a list of sub queries for the upper level select

   - pointer to master (master), which is
     If this is a unit
       - pointer to outer select_lex
     If this is a select_lex
       - pointer to outer unit structure for select

   - pointer to slave (slave), which is either:
     If this is a unit:
       - first SELECT that belong to this unit
     If this is a select_lex
       - first unit that belong to this SELECT (subquries or derived tables)

   - list of all select_lex (link_next/link_prev)
     This is to be used for things like derived tables creation, where we
     go through this list and create the derived tables.

   If unit contain several selects (UNION now, INTERSECT etc later)
   then it have special select_lex called fake_select_lex. It used for
   storing global parameters (like ORDER BY, LIMIT) and executing union.
   Subqueries used in global ORDER BY clause will be attached to this
   fake_select_lex, which will allow them correctly resolve fields of
   'upper' UNION and outer selects.

   For example for following query:

   select *
     from table1
     where table1.field IN (select * from table1_1_1 union
                            select * from table1_1_2)
     union
   select *
     from table2
     where table2.field=(select (select f1 from table2_1_1_1_1
                                   where table2_1_1_1_1.f2=table2_1_1.f3)
                           from table2_1_1
                           where table2_1_1.f1=table2.f2)
     union
   select * from table3;

   we will have following structure:

   select1: (select * from table1 ...)
   select2: (select * from table2 ...)
   select3: (select * from table3)
   select1.1.1: (select * from table1_1_1)
   ...

     main unit
     fake0
     select1 select2 select3
     |^^     |^
    s|||     ||master
    l|||     |+---------------------------------+
    a|||     +---------------------------------+|
    v|||master                         slave   ||
    e||+-------------------------+             ||
     V|            neighbor      |             V|
     unit1.1<+==================>unit1.2       unit2.1
     fake1.1
     select1.1.1 select 1.1.2    select1.2.1   select2.1.1
                                               |^
                                               ||
                                               V|
                                               unit2.1.1.1
                                               select2.1.1.1.1


   relation in main unit will be following:
   (bigger picture for:
      main unit
      fake0
      select1 select2 select3
   in the above picture)

         main unit
         |^^^^|fake_select_lex
         |||||+--------------------------------------------+
         ||||+--------------------------------------------+|
         |||+------------------------------+              ||
         ||+--------------+                |              ||
    slave||master         |                |              ||
         V|      neighbor |       neighbor |        master|V
         select1<========>select2<========>select3        fake0

    list of all select_lex will be following (as it will be constructed by
    parser):

    select1->select2->select3->select2.1.1->select 2.1.2->select2.1.1.1.1-+
                                                                          |
    +---------------------------------------------------------------------+
    |
    +->select1.1.1->select1.1.2

*/

/*
    Base class for Select_Lex (Select_Lex) &
    Select_Lex_Unit (Select_Lex_Unit)
*/
class Select_Lex_Node {
protected:
  Select_Lex_Node *next, **prev,   /* neighbor list */
    *master, *slave,                  /* vertical links */
    *link_next, **link_prev;          /* list of whole Select_Lex */
public:

  uint64_t options;

  /*
    result of this query can't be cached, bit field, can be :
      UNCACHEABLE_DEPENDENT
      UNCACHEABLE_RAND
      UNCACHEABLE_SIDEEFFECT
      UNCACHEABLE_EXPLAIN
      UNCACHEABLE_PREPARE
  */
  std::bitset<8> uncacheable;
  sub_select_type linkage;
  bool no_table_names_allowed; /* used for global order by */
  bool no_error; /* suppress error message (convert it to warnings) */

  static void *operator new(size_t size)
  {
    return memory::sql_alloc(size);
  }
  static void *operator new(size_t size, memory::Root *mem_root)
  { return mem_root->alloc(size); }
  static void operator delete(void*, size_t)
  {  }
  static void operator delete(void*, memory::Root*)
  {}
  Select_Lex_Node(): linkage(UNSPECIFIED_TYPE) {}
  virtual ~Select_Lex_Node() {}
  inline Select_Lex_Node* get_master() { return master; }
  virtual void init_query();
  virtual void init_select();
  void include_down(Select_Lex_Node *upper);
  void include_neighbour(Select_Lex_Node *before);
  void include_standalone(Select_Lex_Node *sel, Select_Lex_Node **ref);
  void include_global(Select_Lex_Node **plink);
  void exclude();

  virtual Select_Lex_Unit* master_unit()= 0;
  virtual Select_Lex* outer_select()= 0;
  virtual Select_Lex* return_after_parsing()= 0;

  virtual bool set_braces(bool value);
  virtual bool inc_in_sum_expr();
  virtual uint32_t get_in_sum_expr();
  virtual TableList* get_table_list();
  virtual List<Item>* get_item_list();
  virtual TableList *add_table_to_list(Session *session, Table_ident *table,
                                       LEX_STRING *alias,
                                       const std::bitset<NUM_OF_TABLE_OPTIONS>& table_options,
                                       thr_lock_type flags= TL_UNLOCK,
                                       List<Index_hint> *hints= 0,
                                       LEX_STRING *option= 0);
  virtual void set_lock_for_tables(thr_lock_type)
  {}

  friend class Select_Lex_Unit;
  friend bool new_select(LEX *lex, bool move_down);
private:
  void fast_exclude();
};

/*
   Select_Lex_Unit - unit of selects (UNION, INTERSECT, ...) group
   Select_Lexs
*/
class Select_Lex_Unit: public Select_Lex_Node {
protected:
  TableList result_table_list;
  select_union *union_result;
  Table *table; /* temporary table using for appending UNION results */

  select_result *result;
  uint64_t found_rows_for_union;
  bool saved_error;

public:
  bool  prepared, // prepare phase already performed for UNION (unit)
    optimized, // optimize phase already performed for UNION (unit)
    executed, // already executed
    cleaned;

  // list of fields which points to temporary table for union
  List<Item> item_list;
  /*
    list of types of items inside union (used for union & derived tables)

    Item_type_holders from which this list consist may have pointers to Field,
    pointers is valid only after preparing SELECTS of this unit and before
    any SELECT of this unit execution

    TODO:
    Possibly this member should be protected, and its direct use replaced
    by get_unit_column_types(). Check the places where it is used.
  */
  List<Item> types;
  /*
    Pointer to 'last' select or pointer to unit where stored
    global parameters for union
  */
  Select_Lex *global_parameters;
  //node on wich we should return current_select pointer after parsing subquery
  Select_Lex *return_to;
  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  /* not NULL if unit used in subselect, point to subselect item */
  Item_subselect *item;
  /* thread handler */
  Session *session;
  /*
    Select_Lex for hidden SELECT in onion which process global
    ORDER BY and LIMIT
  */
  Select_Lex *fake_select_lex;

  Select_Lex *union_distinct; /* pointer to the last UNION DISTINCT */
  bool describe; /* union exec() called for EXPLAIN */

  void init_query();
  Select_Lex_Unit* master_unit();
  Select_Lex* outer_select();
  Select_Lex* first_select()
  {
    return reinterpret_cast<Select_Lex*>(slave);
  }
  Select_Lex_Unit* next_unit()
  {
    return reinterpret_cast<Select_Lex_Unit*>(next);
  }
  Select_Lex* return_after_parsing() { return return_to; }
  void exclude_level();
  void exclude_tree();

  /* UNION methods */
  bool prepare(Session *session, select_result *result,
               uint64_t additional_options);
  bool exec();
  bool cleanup();
  inline void unclean() { cleaned= 0; }
  void reinit_exec_mechanism();

  void print(String *str);

  bool add_fake_select_lex(Session *session);
  void init_prepare_fake_select_lex(Session *session);
  bool change_result(select_result_interceptor *result,
                     select_result_interceptor *old_result);
  void set_limit(Select_Lex *values);
  void set_session(Session *session_arg) { session= session_arg; }
  inline bool is_union ();

  friend void lex_start(Session *session);

  List<Item> *get_unit_column_types();
};

/*
  Select_Lex - store information of parsed SELECT statment
*/
class Select_Lex: public Select_Lex_Node
{
public:

  Select_Lex() :
    context(),
    db(0),
    where(0),
    having(0),
    cond_value(),
    having_value(),
    parent_lex(0),
    olap(UNSPECIFIED_OLAP_TYPE),
    table_list(),
    group_list(),
    item_list(),
    interval_list(),
    is_item_list_lookup(false),
    join(0),
    top_join_list(),
    join_list(0),
    embedding(0),
    sj_nests(),
    leaf_tables(0),
    type(optimizer::ST_PRIMARY),
    order_list(),
    gorder_list(0),
    select_limit(0),
    offset_limit(0),
    ref_pointer_array(0),
    select_n_having_items(0),
    cond_count(0),
    between_count(0),
    max_equal_elems(0),
    select_n_where_fields(0),
    parsing_place(NO_MATTER),
    with_sum_func(0),
    in_sum_expr(0),
    select_number(0),
    nest_level(0),
    inner_sum_func_list(0),
    with_wild(0),
    braces(0),
    having_fix_field(0),
    inner_refs_list(),
    n_sum_items(0),
    n_child_sum_items(0),
    explicit_limit(0),
    is_cross(false),
    subquery_in_having(0),
    is_correlated(0),
    exclude_from_table_unique_test(0),
    non_agg_fields(),
    cur_pos_in_select_list(0),
    prev_join_using(0),
    full_group_by_flag(),
    current_index_hint_type(INDEX_HINT_IGNORE),
    current_index_hint_clause(),
    index_hints(0)
  {
  }

  Name_resolution_context context;
  char *db;
  /* An Item representing the WHERE clause */
  Item *where;
  /* An Item representing the HAVING clause */
  Item *having;
  /* Saved values of the WHERE and HAVING clauses*/
  Item::cond_result cond_value;
  Item::cond_result having_value;
  /* point on lex in which it was created, used in view subquery detection */
  LEX *parent_lex;
  olap_type olap;
  /* FROM clause - points to the beginning of the TableList::next_local list. */
  SQL_LIST table_list;
  SQL_LIST group_list; /* GROUP BY clause. */
  List<Item> item_list;  /* list of fields & expressions */
  List<String> interval_list;
  bool is_item_list_lookup;
  Join *join; /* after Join::prepare it is pointer to corresponding JOIN */
  List<TableList> top_join_list; /* join list of the top level          */
  List<TableList> *join_list;    /* list for the currently parsed join  */
  TableList *embedding;          /* table embedding to the above list   */
  List<TableList> sj_nests;
  /*
    Beginning of the list of leaves in a FROM clause, where the leaves
    inlcude all base tables including view tables. The tables are connected
    by TableList::next_leaf, so leaf_tables points to the left-most leaf.
  */
  TableList *leaf_tables;
  drizzled::optimizer::select_type type; /* type of select for EXPLAIN */

  SQL_LIST order_list;                /* ORDER clause */
  SQL_LIST *gorder_list;
  Item *select_limit, *offset_limit;  /* LIMIT clause parameters */
  /* Arrays of pointers to top elements of all_fields list */
  Item **ref_pointer_array;

  /*
    number of items in select_list and HAVING clause used to get number
    bigger then can be number of entries that will be added to all item
    list during split_sum_func
  */
  uint32_t select_n_having_items;
  uint32_t cond_count;    /* number of arguments of and/or/xor in where/having/on */
  uint32_t between_count; /* number of between predicates in where/having/on      */
  uint32_t max_equal_elems; /* maximal number of elements in multiple equalities  */
  /*
    Number of fields used in select list or where clause of current select
    and all inner subselects.
  */
  uint32_t select_n_where_fields;
  enum_parsing_place parsing_place; /* where we are parsing expression */
  bool with_sum_func;   /* sum function indicator */

  uint32_t in_sum_expr;
  uint32_t select_number; /* number of select (used for EXPLAIN) */
  int8_t nest_level;     /* nesting level of select */
  Item_sum *inner_sum_func_list; /* list of sum func in nested selects */
  uint32_t with_wild; /* item list contain '*' */
  bool braces;   	/* SELECT ... UNION (SELECT ... ) <- this braces */
  /* true when having fix field called in processing of this SELECT */
  bool having_fix_field;
  /* List of references to fields referenced from inner selects */
  List<Item_outer_ref> inner_refs_list;
  /* Number of Item_sum-derived objects in this SELECT */
  uint32_t n_sum_items;
  /* Number of Item_sum-derived objects in children and descendant SELECTs */
  uint32_t n_child_sum_items;

  /* explicit LIMIT clause was used */
  bool explicit_limit;

  /* explicit CROSS JOIN was used */
  bool is_cross;

  /*
    there are subquery in HAVING clause => we can't close tables before
    query processing end even if we use temporary table
  */
  bool subquery_in_having;
  /* true <=> this SELECT is correlated w.r.t. some ancestor select */
  bool is_correlated;
  /* exclude this select from check of unique_table() */
  bool exclude_from_table_unique_test;
  /* List of fields that aren't under an aggregate function */
  List<Item_field> non_agg_fields;
  /* index in the select list of the expression currently being fixed */
  int cur_pos_in_select_list;

  /*
    This is a copy of the original JOIN USING list that comes from
    the parser. The parser :
      1. Sets the natural_join of the second TableList in the join
         and the Select_Lex::prev_join_using.
      2. Makes a parent TableList and sets its is_natural_join/
       join_using_fields members.
      3. Uses the wrapper TableList as a table in the upper level.
    We cannot assign directly to join_using_fields in the parser because
    at stage (1.) the parent TableList is not constructed yet and
    the assignment will override the JOIN USING fields of the lower level
    joins on the right.
  */
  List<String> *prev_join_using;
  /*
    Bitmap used in the ONLY_FULL_GROUP_BY_MODE to prevent mixture of aggregate
    functions and non aggregated fields when GROUP BY list is absent.
    Bits:
      0 - non aggregated fields are used in this select,
          defined as NON_AGG_FIELD_USED.
      1 - aggregate functions are used in this select,
          defined as SUM_FUNC_USED.
  */
  std::bitset<2> full_group_by_flag;

  void init_query();
  void init_select();
  Select_Lex_Unit* master_unit();
  Select_Lex_Unit* first_inner_unit()
  {
    return (Select_Lex_Unit*) slave;
  }
  Select_Lex* outer_select();
  Select_Lex* next_select()
  {
    return (Select_Lex*) next;
  }
  Select_Lex* next_select_in_list()
  {
    return (Select_Lex*) link_next;
  }
  Select_Lex_Node** next_select_in_list_addr()
  {
    return &link_next;
  }
  Select_Lex* return_after_parsing()
  {
    return master_unit()->return_after_parsing();
  }

  void mark_as_dependent(Select_Lex *last);

  bool set_braces(bool value);
  bool inc_in_sum_expr();
  uint32_t get_in_sum_expr();

  void add_item_to_list(Session *session, Item *item);
  void add_group_to_list(Session *session, Item *item, bool asc);
  void add_order_to_list(Session *session, Item *item, bool asc);
  TableList* add_table_to_list(Session *session,
                               Table_ident *table,
                               LEX_STRING *alias,
                               const std::bitset<NUM_OF_TABLE_OPTIONS>& table_options,
                               thr_lock_type flags= TL_UNLOCK,
                               List<Index_hint> *hints= 0,
                               LEX_STRING *option= 0);
  TableList* get_table_list();
  void init_nested_join(Session&);
  TableList *end_nested_join(Session *session);
  TableList *nest_last_join(Session *session);
  void add_joined_table(TableList *table);
  TableList *convert_right_join();
  List<Item>* get_item_list();
  void set_lock_for_tables(thr_lock_type lock_type);
  inline void init_order()
  {
    order_list.elements= 0;
    order_list.first= 0;
    order_list.next= (unsigned char**) &order_list.first;
  }
  /*
    This method created for reiniting LEX in admin_table() and can be
    used only if you are going remove all Select_Lex & units except belonger
    to LEX (LEX::unit & LEX::select, for other purposes there are
    Select_Lex_Unit::exclude_level & Select_Lex_Unit::exclude_tree
  */
  void cut_subtree()
  {
    slave= 0;
  }
  bool test_limit();

  friend void lex_start(Session *session);
  void make_empty_select()
  {
    init_query();
    init_select();
  }
  void setup_ref_array(Session *session, uint32_t order_group_num);
  void print(Session *session, String *str);
  static void print_order(String *str, Order *order);

  void print_limit(Session *session, String *str);
  void fix_prepare_information(Session *session, Item **conds, Item **having_conds);
  /*
    Destroy the used execution plan (JOIN) of this subtree (this
    Select_Lex and all nested Select_Lexes and Select_Lex_Units).
  */
  bool cleanup();
  /*
    Recursively cleanup the join of this select lex and of all nested
    select lexes.
  */
  void cleanup_all_joins(bool full);

  void set_index_hint_type(index_hint_type type, index_clause_map clause);

  /*
   Add a index hint to the tagged list of hints. The type and clause of the
   hint will be the current ones (set by set_index_hint())
  */
  void add_index_hint(Session *session, char *str, uint32_t length);

  /* make a list to hold index hints */
  void alloc_index_hints (Session *session);
  /* read and clear the index hints */
  List<Index_hint>* pop_index_hints(void)
  {
    List<Index_hint> *hints= index_hints;
    index_hints= NULL;
    return hints;
  }

  void clear_index_hints(void) { index_hints= NULL; }

private:
  /* current index hint kind. used in filling up index_hints */
  index_hint_type current_index_hint_type;
  index_clause_map current_index_hint_clause;
  /* a list of USE/FORCE/IGNORE INDEX */
  List<Index_hint> *index_hints;
};

inline bool Select_Lex_Unit::is_union ()
{
  return first_select()->next_select() &&
    first_select()->next_select()->linkage == UNION_TYPE;
}

enum xa_option_words
{
  XA_NONE
, XA_JOIN
, XA_RESUME
, XA_ONE_PHASE
, XA_SUSPEND
, XA_FOR_MIGRATE
};

extern const LEX_STRING null_lex_str;

/*
  Class representing list of all tables used by statement.
  It also contains information about stored functions used by statement
  since during its execution we may have to add all tables used by its
  stored functions/triggers to this list in order to pre-open and lock
  them.

  Also used by st_lex::reset_n_backup/restore_backup_query_tables_list()
  methods to save and restore this information.
*/
class Query_tables_list
{
public:
  /* Global list of all tables used by this statement */
  TableList *query_tables;
  /* Pointer to next_global member of last element in the previous list. */
  TableList **query_tables_last;
  /*
    If non-0 then indicates that query requires prelocking and points to
    next_global member of last own element in query table list (i.e. last
    table which was not added to it as part of preparation to prelocking).
    0 - indicates that this query does not need prelocking.
  */
  TableList **query_tables_own_last;

  /*
    These constructor and destructor serve for creation/destruction
    of Query_tables_list instances which are used as backup storage.
  */
  Query_tables_list() {}
  virtual ~Query_tables_list() {}

  /* Initializes (or resets) Query_tables_list object for "real" use. */
  void reset_query_tables_list(bool init);

  /*
    Direct addition to the list of query tables.
    If you are using this function, you must ensure that the table
    object, in particular table->db member, is initialized.
  */
  void add_to_query_tables(TableList *table)
  {
    *(table->prev_global= query_tables_last)= table;
    query_tables_last= &table->next_global;
  }
  /* Return pointer to first not-own table in query-tables or 0 */
  TableList* first_not_own_table()
  {
    return ( query_tables_own_last ? *query_tables_own_last : 0);
  }
  void chop_off_not_own_tables()
  {
    if (query_tables_own_last)
    {
      *query_tables_own_last= 0;
      query_tables_last= query_tables_own_last;
      query_tables_own_last= 0;
    }
  }
};

/**
  The state of the lexical parser, when parsing comments.
*/
enum enum_comment_state
{
  /**
    Not parsing comments.
  */
  NO_COMMENT,
  /**
    Parsing comments that need to be preserved.
    Typically, these are user comments '/' '*' ... '*' '/'.
  */
  PRESERVE_COMMENT,
  /**
    Parsing comments that need to be discarded.
    Typically, these are special comments '/' '*' '!' ... '*' '/',
    or '/' '*' '!' 'M' 'M' 'm' 'm' 'm' ... '*' '/', where the comment
    markers should not be expanded.
  */
  DISCARD_COMMENT
};

} /* namespace drizzled */

#include <drizzled/lex_input_stream.h>

namespace drizzled
{

/* The state of the lex parsing. This is saved in the Session struct */
class LEX : public Query_tables_list
{
public:
  Select_Lex_Unit unit;                         /* most upper unit */
  Select_Lex select_lex;                        /* first Select_Lex */
  /* current Select_Lex in parsing */
  Select_Lex *current_select;
  /* list of all Select_Lex */
  Select_Lex *all_selects_list;

  /* This is the "scale" for DECIMAL (S,P) notation */
  char *length;
  /* This is the decimal precision in DECIMAL(S,P) notation */
  char *dec;

  /**
   * This is used kind of like the "ident" member variable below, as
   * a place to store certain names of identifiers.  Unfortunately, it
   * is used differently depending on the Command (SELECT on a derived
   * table vs CREATE)
   */
  LEX_STRING name;
  /* The string literal used in a LIKE expression */
  String *wild;
  file_exchange *exchange;
  select_result *result;

  /**
   * This is current used to store the name of a named key cache
   * or a named savepoint.  It should probably be refactored out into
   * the eventual Command class built for the Keycache and Savepoint
   * commands.
   */
  LEX_STRING ident;

  unsigned char* yacc_yyss, *yacc_yyvs;
  /* The owning Session of this LEX */
  Session *session;
  const charset_info_st *charset;
  bool text_string_is_7bit;
  /* store original leaf_tables for INSERT SELECT and PS/SP */
  TableList *leaf_tables_insert;

  List<Key_part_spec> col_list;
  List<Key_part_spec> ref_list;
  List<String>	      interval_list;
  List<Lex_Column>    columns;
  List<Item>	      *insert_list,field_list,value_list,update_list;
  List<List_item>     many_values;
  SetVarVector  var_list;
  /*
    A stack of name resolution contexts for the query. This stack is used
    at parse time to set local name resolution contexts for various parts
    of a query. For example, in a JOIN ... ON (some_condition) clause the
    Items in 'some_condition' must be resolved only against the operands
    of the the join, and not against the whole clause. Similarly, Items in
    subqueries should be resolved against the subqueries (and outer queries).
    The stack is used in the following way: when the parser detects that
    all Items in some clause need a local context, it creates a new context
    and pushes it on the stack. All newly created Items always store the
    top-most context in the stack. Once the parser leaves the clause that
    required a local context, the parser pops the top-most context.
  */
  List<Name_resolution_context> context_stack;

  SQL_LIST auxiliary_table_list;
  SQL_LIST save_list;
  CreateField *last_field;
  Item_sum *in_sum_func;
  plugin::Function *udf;
  uint32_t type;
  /*
    This variable is used in post-parse stage to declare that sum-functions,
    or functions which have sense only if GROUP BY is present, are allowed.
    For example in a query
    SELECT ... FROM ...WHERE MIN(i) == 1 GROUP BY ... HAVING MIN(i) > 2
    MIN(i) in the WHERE clause is not allowed in the opposite to MIN(i)
    in the HAVING clause. Due to possible nesting of select construct
    the variable can contain 0 or 1 for each nest level.
  */
  nesting_map allow_sum_func;
  enum_sql_command sql_command;
  statement::Statement *statement;
  /*
    Usually `expr` rule of yacc is quite reused but some commands better
    not support subqueries which comes standard with this rule, like
    KILL, HA_READ, CREATE/ALTER EVENT etc. Set this to `false` to get
    syntax error back.
  */
  bool expr_allows_subselect;

  thr_lock_type lock_option;
  enum enum_duplicates duplicates;
  union {
    enum ha_rkey_function ha_rkey_mode;
    enum xa_option_words xa_opt;
  };
  sql_var_t option_type;

  int nest_level;
  uint8_t describe;
  /*
    A flag that indicates what kinds of derived tables are present in the
    query (0 if no derived tables, otherwise DERIVED_SUBQUERY).
  */
  uint8_t derived_tables;

  /* Was the IGNORE symbol found in statement */
  bool ignore;

  /**
    During name resolution search only in the table list given by
    Name_resolution_context::first_name_resolution_table and
    Name_resolution_context::last_name_resolution_table
    (see Item_field::fix_fields()).
  */
  bool use_only_table_context;

  /* Was the ESCAPE keyword used? */
  bool escape_used;
  bool is_lex_started; /* If lex_start() did run. For debugging. */

  LEX();

  /* Note that init and de-init mostly happen in lex_start and lex_end
     and not here. This is because LEX isn't delete/new for each new
     statement in a session. It's re-used by doing lex_end, lex_start
     in sql_lex.cc
  */
  virtual ~LEX();

  TableList *unlink_first_table(bool *link_to_local);
  void link_first_table_back(TableList *first, bool link_to_local);
  void first_lists_tables_same();

  void cleanup_after_one_table_open();

  void push_context(Name_resolution_context *context)
  {
    context_stack.push_front(context);
  }

  void pop_context()
  {
    context_stack.pop();
  }

  bool copy_db_to(char **p_db, size_t *p_db_length) const;

  Name_resolution_context *current_context()
  {
    return &context_stack.front();
  }

  /**
    @brief check if the statement is a single-level join
    @return result of the check
      @retval true  The statement doesn't contain subqueries, unions and
                    stored procedure calls.
      @retval false There are subqueries, UNIONs or stored procedure calls.
  */
  bool is_single_level_stmt()
  {
    /*
      This check exploits the fact that the last added to all_select_list is
      on its top. So select_lex (as the first added) will be at the tail
      of the list.
    */
    if (&select_lex == all_selects_list)
    {
      assert(!all_selects_list->next_select_in_list());
      return true;
    }
    return false;
  }
  bool is_cross; // CROSS keyword was used
  bool isCacheable()
  {
    return cacheable;
  }
  void setCacheable(bool val)
  {
    cacheable= val;
  }

  void reset()
  {
    sum_expr_used= false;
    _exists= false;
  }

  void setSumExprUsed()
  {
    sum_expr_used= true;
  }

  bool isSumExprUsed()
  {
    return sum_expr_used;
  }

  void start(Session *session);
  void end();

  message::Table *table()
  {
    if (not _create_table)
      _create_table= new message::Table;

    return _create_table;
  }

  message::AlterTable *alter_table();

  message::Table::Field *field()
  {
    return _create_field;
  }

  void setField(message::Table::Field *arg)
  {
    _create_field= arg;
  }

  void setExists()
  {
    _exists= true;
  }

  bool exists() const
  {
    return _exists;
  }

private:
  bool cacheable;
  bool sum_expr_used;
  message::Table *_create_table;
  message::AlterTable *_alter_table;
  message::Table::Field *_create_field;
  bool _exists;
};

extern void lex_start(Session *session);

/**
  @} (End of group Semantic_Analysis)
*/

} /* namespace drizzled */

#endif /* DRIZZLE_SERVER */
