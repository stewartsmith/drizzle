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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DRIZZLE_SERVER_SQL_LEX_H
#define DRIZZLE_SERVER_SQL_LEX_H

/**
  @defgroup Semantic_Analysis Semantic Analysis
*/

/* YACC and LEX Definitions */

/* These may not be declared yet */
class Table_ident;
class sql_exchange;
class LEX_COLUMN;

#ifdef DRIZZLE_SERVER
/*
  The following hack is needed because mysql_yacc.cc does not define
  YYSTYPE before including this file
*/

#include "set_var.h"

#ifdef DRIZZLE_YACC
#define LEX_YYSTYPE void *
#else
#if defined(DRIZZLE_LEX)
#include "lex_symbol.h"
#include "sql_yacc.h"
#define LEX_YYSTYPE YYSTYPE *
#else
#define LEX_YYSTYPE void *
#endif
#endif
#endif

/*
  When a command is added here, be sure it's also added in mysqld.cc
  in "struct show_var_st status_vars[]= {" ...

  If the command returns a result set or is not allowed in stored
  functions or triggers, please also make sure that
  sp_get_flags_for_command (sp_head.cc) returns proper flags for the
  added SQLCOM_.
*/

enum enum_sql_command {
  SQLCOM_SELECT, SQLCOM_CREATE_TABLE, SQLCOM_CREATE_INDEX, SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE, SQLCOM_INSERT, SQLCOM_INSERT_SELECT,
  SQLCOM_DELETE, SQLCOM_TRUNCATE, SQLCOM_DROP_TABLE, SQLCOM_DROP_INDEX,
  SQLCOM_SHOW_DATABASES, SQLCOM_SHOW_TABLES, SQLCOM_SHOW_FIELDS,
  SQLCOM_SHOW_KEYS, SQLCOM_SHOW_VARIABLES, SQLCOM_SHOW_STATUS,
  SQLCOM_SHOW_ENGINE_LOGS, SQLCOM_SHOW_ENGINE_STATUS, SQLCOM_SHOW_ENGINE_MUTEX,
  SQLCOM_SHOW_PROCESSLIST, SQLCOM_SHOW_MASTER_STAT, SQLCOM_SHOW_SLAVE_STAT,
  SQLCOM_SHOW_CREATE, SQLCOM_SHOW_CHARSETS,
  SQLCOM_SHOW_COLLATIONS, SQLCOM_SHOW_CREATE_DB, SQLCOM_SHOW_TABLE_STATUS,
  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB, SQLCOM_ALTER_DB,
  SQLCOM_REPAIR, SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT,
  SQLCOM_OPTIMIZE, SQLCOM_CHECK,
  SQLCOM_ASSIGN_TO_KEYCACHE,
  SQLCOM_FLUSH, SQLCOM_KILL, SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK, SQLCOM_ROLLBACK_TO_SAVEPOINT,
  SQLCOM_COMMIT, SQLCOM_SAVEPOINT, SQLCOM_RELEASE_SAVEPOINT,
  SQLCOM_SLAVE_START, SQLCOM_SLAVE_STOP,
  SQLCOM_BEGIN, SQLCOM_CHANGE_MASTER,
  SQLCOM_RENAME_TABLE,  
  SQLCOM_RESET, SQLCOM_PURGE, SQLCOM_PURGE_BEFORE, SQLCOM_SHOW_BINLOGS,
  SQLCOM_SHOW_OPEN_TABLES,
  SQLCOM_SHOW_SLAVE_HOSTS, SQLCOM_DELETE_MULTI, SQLCOM_UPDATE_MULTI,
  SQLCOM_SHOW_BINLOG_EVENTS,
  SQLCOM_SHOW_WARNS, SQLCOM_EMPTY_QUERY, SQLCOM_SHOW_ERRORS,
  SQLCOM_CHECKSUM,
  SQLCOM_BINLOG_BASE64_EVENT,
  SQLCOM_SHOW_PLUGINS,
  /*
    When a command is added here, be sure it's also added in mysqld.cc
    in "struct show_var_st status_vars[]= {" ...
  */
  /* This should be the last !!! */
  SQLCOM_END
};

// describe/explain types
#define DESCRIBE_NORMAL		1
#define DESCRIBE_EXTENDED	2

#ifdef DRIZZLE_SERVER

#define DERIVED_NONE	0
#define DERIVED_SUBQUERY	1

typedef List<Item> List_item;

/* SERVERS CACHE CHANGES */
typedef struct st_lex_server_options
{
  int32_t port;
  uint32_t server_name_length;
  char *server_name, *host, *db, *username, *password, *scheme, *owner;
} LEX_SERVER_OPTIONS;

typedef struct st_lex_master_info
{
  char *host, *user, *password, *log_file_name;
  uint32_t port, connect_retry;
  float heartbeat_period;
  uint64_t pos;
  uint32_t server_id;
  /*
    Enum is used for making it possible to detect if the user
    changed variable or if it should be left at old value
   */
  enum {LEX_MI_UNCHANGED, LEX_MI_DISABLE, LEX_MI_ENABLE}
    ssl, ssl_verify_server_cert, heartbeat_opt;
  char *ssl_key, *ssl_cert, *ssl_ca, *ssl_capath, *ssl_cipher;
  char *relay_log_name;
  uint32_t relay_log_pos;
} LEX_MASTER_INFO;


enum sub_select_type
{
  UNSPECIFIED_TYPE,UNION_TYPE, INTERSECT_TYPE,
  EXCEPT_TYPE, GLOBAL_OPTIONS_TYPE, DERIVED_TABLE_TYPE, OLAP_TYPE
};

enum olap_type 
{
  UNSPECIFIED_OLAP_TYPE, CUBE_TYPE, ROLLUP_TYPE
};

enum tablespace_op_type
{
  NO_TABLESPACE_OP, DISCARD_TABLESPACE, IMPORT_TABLESPACE
};

/* 
  String names used to print a statement with index hints.
  Keep in sync with index_hint_type.
*/
extern const char * index_hint_type_name[];
typedef uchar index_clause_map;

/*
  Bits in index_clause_map : one for each possible FOR clause in
  USE/FORCE/IGNORE INDEX index hint specification
*/
#define INDEX_HINT_MASK_JOIN  (1)
#define INDEX_HINT_MASK_GROUP (1 << 1)
#define INDEX_HINT_MASK_ORDER (1 << 2)

#define INDEX_HINT_MASK_ALL (INDEX_HINT_MASK_JOIN | INDEX_HINT_MASK_GROUP | \
                             INDEX_HINT_MASK_ORDER)

/* Single element of an USE/FORCE/IGNORE INDEX list specified as a SQL hint  */
class Index_hint : public Sql_alloc
{
public:
  /* The type of the hint : USE/FORCE/IGNORE */
  enum index_hint_type type;
  /* Where the hit applies to. A bitmask of INDEX_HINT_MASK_<place> values */
  index_clause_map clause;
  /* 
    The index name. Empty (str=NULL) name represents an empty list 
    USE INDEX () clause 
  */ 
  LEX_STRING key_name;

  Index_hint (enum index_hint_type type_arg, index_clause_map clause_arg,
              char *str, uint32_t length) :
    type(type_arg), clause(clause_arg)
  {
    key_name.str= str;
    key_name.length= length;
  }

  void print(THD *thd, String *str);
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
    Base class for st_select_lex (SELECT_LEX) & 
    st_select_lex_unit (SELECT_LEX_UNIT)
*/
struct st_lex;
class st_select_lex;
class st_select_lex_unit;
class st_select_lex_node {
protected:
  st_select_lex_node *next, **prev,   /* neighbor list */
    *master, *slave,                  /* vertical links */
    *link_next, **link_prev;          /* list of whole SELECT_LEX */
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
  uint8_t uncacheable;
  enum sub_select_type linkage;
  bool no_table_names_allowed; /* used for global order by */
  bool no_error; /* suppress error message (convert it to warnings) */

  static void *operator new(size_t size)
  {
    return sql_alloc(size);
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint32_t) size); }
  static void operator delete(void *ptr __attribute__((unused)),
                              size_t size __attribute__((unused)))
  { TRASH(ptr, size); }
  static void operator delete(void *ptr __attribute__((unused)),
                              MEM_ROOT *mem_root __attribute__((unused)))
  {}
  st_select_lex_node(): linkage(UNSPECIFIED_TYPE) {}
  virtual ~st_select_lex_node() {}
  inline st_select_lex_node* get_master() { return master; }
  virtual void init_query();
  virtual void init_select();
  void include_down(st_select_lex_node *upper);
  void include_neighbour(st_select_lex_node *before);
  void include_standalone(st_select_lex_node *sel, st_select_lex_node **ref);
  void include_global(st_select_lex_node **plink);
  void exclude();

  virtual st_select_lex_unit* master_unit()= 0;
  virtual st_select_lex* outer_select()= 0;
  virtual st_select_lex* return_after_parsing()= 0;

  virtual bool set_braces(bool value);
  virtual bool inc_in_sum_expr();
  virtual uint32_t get_in_sum_expr();
  virtual TABLE_LIST* get_table_list();
  virtual List<Item>* get_item_list();
  virtual uint32_t get_table_join_options();
  virtual TABLE_LIST *add_table_to_list(THD *thd, Table_ident *table,
                                        LEX_STRING *alias,
                                        uint32_t table_options,
                                        thr_lock_type flags= TL_UNLOCK,
                                        List<Index_hint> *hints= 0,
                                        LEX_STRING *option= 0);
  virtual void set_lock_for_tables(thr_lock_type lock_type __attribute__((unused)))
  {}

  friend class st_select_lex_unit;
  friend bool mysql_new_select(struct st_lex *lex, bool move_down);
private:
  void fast_exclude();
};
typedef class st_select_lex_node SELECT_LEX_NODE;

/* 
   SELECT_LEX_UNIT - unit of selects (UNION, INTERSECT, ...) group 
   SELECT_LEXs
*/
class THD;
class select_result;
class JOIN;
class select_union;
class st_select_lex_unit: public st_select_lex_node {
protected:
  TABLE_LIST result_table_list;
  select_union *union_result;
  TABLE *table; /* temporary table using for appending UNION results */

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
  st_select_lex *global_parameters;
  //node on wich we should return current_select pointer after parsing subquery
  st_select_lex *return_to;
  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  /* not NULL if unit used in subselect, point to subselect item */
  Item_subselect *item;
  /* thread handler */
  THD *thd;
  /*
    SELECT_LEX for hidden SELECT in onion which process global
    ORDER BY and LIMIT
  */
  st_select_lex *fake_select_lex;

  st_select_lex *union_distinct; /* pointer to the last UNION DISTINCT */
  bool describe; /* union exec() called for EXPLAIN */

  void init_query();
  st_select_lex_unit* master_unit();
  st_select_lex* outer_select();
  st_select_lex* first_select()
  {
    return my_reinterpret_cast(st_select_lex*)(slave);
  }
  st_select_lex_unit* next_unit()
  {
    return my_reinterpret_cast(st_select_lex_unit*)(next);
  }
  st_select_lex* return_after_parsing() { return return_to; }
  void exclude_level();
  void exclude_tree();

  /* UNION methods */
  bool prepare(THD *thd, select_result *result, uint32_t additional_options);
  bool exec();
  bool cleanup();
  inline void unclean() { cleaned= 0; }
  void reinit_exec_mechanism();

  void print(String *str, enum_query_type query_type);

  bool add_fake_select_lex(THD *thd);
  void init_prepare_fake_select_lex(THD *thd);
  bool change_result(select_result_interceptor *result,
                     select_result_interceptor *old_result);
  void set_limit(st_select_lex *values);
  void set_thd(THD *thd_arg) { thd= thd_arg; }
  inline bool is_union (); 

  friend void lex_start(THD *thd);
  friend int subselect_union_engine::exec();

  List<Item> *get_unit_column_types();
};

typedef class st_select_lex_unit SELECT_LEX_UNIT;

/*
  SELECT_LEX - store information of parsed SELECT statment
*/
class st_select_lex: public st_select_lex_node
{
public:
  Name_resolution_context context;
  char *db;
  Item *where, *having;                         /* WHERE & HAVING clauses */
  Item *prep_where; /* saved WHERE clause for prepared statement processing */
  Item *prep_having;/* saved HAVING clause for prepared statement processing */
  /* Saved values of the WHERE and HAVING clauses*/
  Item::cond_result cond_value, having_value;
  /* point on lex in which it was created, used in view subquery detection */
  st_lex *parent_lex;
  enum olap_type olap;
  /* FROM clause - points to the beginning of the TABLE_LIST::next_local list. */
  SQL_LIST	      table_list;
  SQL_LIST	      group_list; /* GROUP BY clause. */
  List<Item>          item_list;  /* list of fields & expressions */
  List<String>        interval_list;
  bool	              is_item_list_lookup;
  /* 
    Despite their names, the following are used in unions. This should 
    be rewritten. -Brian
  */
  List<Item_real_func> *ftfunc_list;
  List<Item_real_func> ftfunc_list_alloc;
  JOIN *join; /* after JOIN::prepare it is pointer to corresponding JOIN */
  List<TABLE_LIST> top_join_list; /* join list of the top level          */
  List<TABLE_LIST> *join_list;    /* list for the currently parsed join  */
  TABLE_LIST *embedding;          /* table embedding to the above list   */
  List<TABLE_LIST> sj_nests;
  /*
    Beginning of the list of leaves in a FROM clause, where the leaves
    inlcude all base tables including view tables. The tables are connected
    by TABLE_LIST::next_leaf, so leaf_tables points to the left-most leaf.
  */
  TABLE_LIST *leaf_tables;
  const char *type;               /* type of select for EXPLAIN          */

  SQL_LIST order_list;                /* ORDER clause */
  SQL_LIST *gorder_list;
  Item *select_limit, *offset_limit;  /* LIMIT clause parameters */
  // Arrays of pointers to top elements of all_fields list
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
  /* 
    PS or SP cond natural joins was alredy processed with permanent
    arena and all additional items which we need alredy stored in it
  */
  bool conds_processed_with_permanent_arena;

  uint32_t table_join_options;
  uint32_t in_sum_expr;
  uint32_t select_number; /* number of select (used for EXPLAIN) */
  int nest_level;     /* nesting level of select */
  Item_sum *inner_sum_func_list; /* list of sum func in nested selects */ 
  uint32_t with_wild; /* item list contain '*' */
  bool  braces;   	/* SELECT ... UNION (SELECT ... ) <- this braces */
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
  /*
    there are subquery in HAVING clause => we can't close tables before
    query processing end even if we use temporary table
  */
  bool subquery_in_having;
  /* true <=> this SELECT is correlated w.r.t. some ancestor select */
  bool is_correlated;
  /*
    This variable is required to ensure proper work of subqueries and
    stored procedures. Generally, one should use the states of
    Query_arena to determine if it's a statement prepare or first
    execution of a stored procedure. However, in case when there was an
    error during the first execution of a stored procedure, the SP body
    is not expelled from the SP cache. Therefore, a deeply nested
    subquery might be left unoptimized. So we need this per-subquery
    variable to inidicate the optimization/execution state of every
    subquery. Prepared statements work OK in that regard, as in
    case of an error during prepare the PS is not created.
  */
  bool first_execution;
  bool first_cond_optimization;
  /* exclude this select from check of unique_table() */
  bool exclude_from_table_unique_test;
  /* List of fields that aren't under an aggregate function */
  List<Item_field> non_agg_fields;
  /* index in the select list of the expression currently being fixed */
  int cur_pos_in_select_list;

  List<udf_func>     udf_list;                  /* udf function calls stack */
  /* 
    This is a copy of the original JOIN USING list that comes from
    the parser. The parser :
      1. Sets the natural_join of the second TABLE_LIST in the join
         and the st_select_lex::prev_join_using.
      2. Makes a parent TABLE_LIST and sets its is_natural_join/
       join_using_fields members.
      3. Uses the wrapper TABLE_LIST as a table in the upper level.
    We cannot assign directly to join_using_fields in the parser because
    at stage (1.) the parent TABLE_LIST is not constructed yet and
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
  uint8_t full_group_by_flag;
  void init_query();
  void init_select();
  st_select_lex_unit* master_unit();
  st_select_lex_unit* first_inner_unit() 
  { 
    return (st_select_lex_unit*) slave; 
  }
  st_select_lex* outer_select();
  st_select_lex* next_select() { return (st_select_lex*) next; }
  st_select_lex* next_select_in_list() 
  {
    return (st_select_lex*) link_next;
  }
  st_select_lex_node** next_select_in_list_addr()
  {
    return &link_next;
  }
  st_select_lex* return_after_parsing()
  {
    return master_unit()->return_after_parsing();
  }

  void mark_as_dependent(st_select_lex *last);

  bool set_braces(bool value);
  bool inc_in_sum_expr();
  uint32_t get_in_sum_expr();

  bool add_item_to_list(THD *thd, Item *item);
  bool add_group_to_list(THD *thd, Item *item, bool asc);
  bool add_order_to_list(THD *thd, Item *item, bool asc);
  TABLE_LIST* add_table_to_list(THD *thd, Table_ident *table,
				LEX_STRING *alias,
				uint32_t table_options,
				thr_lock_type flags= TL_UNLOCK,
				List<Index_hint> *hints= 0,
                                LEX_STRING *option= 0);
  TABLE_LIST* get_table_list();
  bool init_nested_join(THD *thd);
  TABLE_LIST *end_nested_join(THD *thd);
  TABLE_LIST *nest_last_join(THD *thd);
  void add_joined_table(TABLE_LIST *table);
  TABLE_LIST *convert_right_join();
  List<Item>* get_item_list();
  uint32_t get_table_join_options();
  void set_lock_for_tables(thr_lock_type lock_type);
  inline void init_order()
  {
    order_list.elements= 0;
    order_list.first= 0;
    order_list.next= (uchar**) &order_list.first;
  }
  /*
    This method created for reiniting LEX in mysql_admin_table() and can be
    used only if you are going remove all SELECT_LEX & units except belonger
    to LEX (LEX::unit & LEX::select, for other purposes there are
    SELECT_LEX_UNIT::exclude_level & SELECT_LEX_UNIT::exclude_tree
  */
  void cut_subtree() { slave= 0; }
  bool test_limit();

  friend void lex_start(THD *thd);
  st_select_lex() : n_sum_items(0), n_child_sum_items(0) {}
  void make_empty_select()
  {
    init_query();
    init_select();
  }
  bool setup_ref_array(THD *thd, uint32_t order_group_num);
  void print(THD *thd, String *str, enum_query_type query_type);
  static void print_order(String *str,
                          ORDER *order,
                          enum_query_type query_type);
  void print_limit(THD *thd, String *str, enum_query_type query_type);
  void fix_prepare_information(THD *thd, Item **conds, Item **having_conds);
  /*
    Destroy the used execution plan (JOIN) of this subtree (this
    SELECT_LEX and all nested SELECT_LEXes and SELECT_LEX_UNITs).
  */
  bool cleanup();
  /*
    Recursively cleanup the join of this select lex and of all nested
    select lexes.
  */
  void cleanup_all_joins(bool full);

  void set_index_hint_type(enum index_hint_type type, index_clause_map clause);

  /* 
   Add a index hint to the tagged list of hints. The type and clause of the
   hint will be the current ones (set by set_index_hint()) 
  */
  bool add_index_hint (THD *thd, char *str, uint32_t length);

  /* make a list to hold index hints */
  void alloc_index_hints (THD *thd);
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
  enum index_hint_type current_index_hint_type;
  index_clause_map current_index_hint_clause;
  /* a list of USE/FORCE/IGNORE INDEX */
  List<Index_hint> *index_hints;
};
typedef class st_select_lex SELECT_LEX;

inline bool st_select_lex_unit::is_union ()
{ 
  return first_select()->next_select() && 
    first_select()->next_select()->linkage == UNION_TYPE;
}

#define ALTER_ADD_COLUMN	(1L << 0)
#define ALTER_DROP_COLUMN	(1L << 1)
#define ALTER_CHANGE_COLUMN	(1L << 2)
#define ALTER_COLUMN_STORAGE	(1L << 3)
#define ALTER_COLUMN_FORMAT	(1L << 4)
#define ALTER_COLUMN_ORDER      (1L << 5)
#define ALTER_ADD_INDEX		(1L << 6)
#define ALTER_DROP_INDEX	(1L << 7)
#define ALTER_RENAME		(1L << 8)
#define ALTER_ORDER		(1L << 9)
#define ALTER_OPTIONS		(1L << 10)
#define ALTER_COLUMN_DEFAULT    (1L << 11)
#define ALTER_KEYS_ONOFF        (1L << 12)
#define ALTER_STORAGE	        (1L << 13)
#define ALTER_ROW_FORMAT        (1L << 14)
#define ALTER_CONVERT           (1L << 15)
#define ALTER_FORCE		(1L << 16)
#define ALTER_RECREATE          (1L << 17)
#define ALTER_TABLE_REORG        (1L << 24)
#define ALTER_FOREIGN_KEY         (1L << 31)

/**
  @brief Parsing data for CREATE or ALTER TABLE.

  This structure contains a list of columns or indexes to be created,
  altered or dropped.
*/

class Alter_info
{
public:
  List<Alter_drop>              drop_list;
  List<Alter_column>            alter_list;
  List<Key>                     key_list;
  List<Create_field>            create_list;
  uint32_t                          flags;
  enum enum_enable_or_disable   keys_onoff;
  enum tablespace_op_type       tablespace_op;
  uint32_t                          no_parts;
  enum ha_build_method          build_method;
  Create_field                 *datetime_field;
  bool                          error_if_not_empty;


  Alter_info() :
    flags(0),
    keys_onoff(LEAVE_AS_IS),
    tablespace_op(NO_TABLESPACE_OP),
    no_parts(0),
    build_method(HA_BUILD_DEFAULT),
    datetime_field(NULL),
    error_if_not_empty(false)
  {}

  void reset()
  {
    drop_list.empty();
    alter_list.empty();
    key_list.empty();
    create_list.empty();
    flags= 0;
    keys_onoff= LEAVE_AS_IS;
    tablespace_op= NO_TABLESPACE_OP;
    no_parts= 0;
    build_method= HA_BUILD_DEFAULT;
    datetime_field= 0;
    error_if_not_empty= false;
  }
  Alter_info(const Alter_info &rhs, MEM_ROOT *mem_root);
private:
  Alter_info &operator=(const Alter_info &rhs); // not implemented
  Alter_info(const Alter_info &rhs);            // not implemented
};

enum xa_option_words {XA_NONE, XA_JOIN, XA_RESUME, XA_ONE_PHASE,
                      XA_SUSPEND, XA_FOR_MIGRATE};

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
  TABLE_LIST *query_tables;
  /* Pointer to next_global member of last element in the previous list. */
  TABLE_LIST **query_tables_last;
  /*
    If non-0 then indicates that query requires prelocking and points to
    next_global member of last own element in query table list (i.e. last
    table which was not added to it as part of preparation to prelocking).
    0 - indicates that this query does not need prelocking.
  */
  TABLE_LIST **query_tables_own_last;
  /*
    Set of stored routines called by statement.
    (Note that we use lazy-initialization for this hash).
  */
  enum { START_SROUTINES_HASH_SIZE= 16 };
  HASH sroutines;
  /*
    List linking elements of 'sroutines' set. Allows you to add new elements
    to this set as you iterate through the list of existing elements.
    'sroutines_list_own_last' is pointer to ::next member of last element of
    this list which represents routine which is explicitly used by query.
    'sroutines_list_own_elements' number of explicitly used routines.
    We use these two members for restoring of 'sroutines_list' to the state
    in which it was right after query parsing.
  */
  SQL_LIST sroutines_list;
  uchar    **sroutines_list_own_last;
  uint32_t     sroutines_list_own_elements;

  /*
    These constructor and destructor serve for creation/destruction
    of Query_tables_list instances which are used as backup storage.
  */
  Query_tables_list() {}
  ~Query_tables_list() {}

  /* Initializes (or resets) Query_tables_list object for "real" use. */
  void reset_query_tables_list(bool init);
  void destroy_query_tables_list();
  void set_query_tables_list(Query_tables_list *state)
  {
    *this= *state;
  }

  /*
    Direct addition to the list of query tables.
    If you are using this function, you must ensure that the table
    object, in particular table->db member, is initialized.
  */
  void add_to_query_tables(TABLE_LIST *table)
  {
    *(table->prev_global= query_tables_last)= table;
    query_tables_last= &table->next_global;
  }
  /* Return pointer to first not-own table in query-tables or 0 */
  TABLE_LIST* first_not_own_table()
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

  /**
     Has the parser/scanner detected that this statement is unsafe?
   */
  inline bool is_stmt_unsafe() const {
    return binlog_stmt_flags & (1U << BINLOG_STMT_FLAG_UNSAFE);
  }

  /**
     Flag the current (top-level) statement as unsafe.

     The flag will be reset after the statement has finished.

   */
  inline void set_stmt_unsafe() {
    binlog_stmt_flags|= (1U << BINLOG_STMT_FLAG_UNSAFE);
  }

  inline void clear_stmt_unsafe() {
    binlog_stmt_flags&= ~(1U << BINLOG_STMT_FLAG_UNSAFE);
  }

  /**
    true if the parsed tree contains references to stored procedures
    or functions, false otherwise
  */
  bool uses_stored_routines() const
  { return sroutines_list.elements != 0; }

private:
  enum enum_binlog_stmt_flag {
    BINLOG_STMT_FLAG_UNSAFE,
    BINLOG_STMT_FLAG_COUNT
  };

  /*
    Tells if the parsing stage detected properties of the statement,
    for example: that some items require row-based binlogging to give
    a reliable binlog/replication, or if we will use stored functions
    or triggers which themselves need require row-based binlogging.
  */
  uint32_t binlog_stmt_flags;
};


/*
  st_parsing_options contains the flags for constructions that are
  allowed in the current statement.
*/

struct st_parsing_options
{
  bool allows_variable;
  bool allows_select_into;
  bool allows_select_procedure;
  bool allows_derived;

  st_parsing_options() { reset(); }
  void reset();
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


/**
  @brief This class represents the character input stream consumed during
  lexical analysis.

  In addition to consuming the input stream, this class performs some
  comment pre processing, by filtering out out of bound special text
  from the query input stream.
  Two buffers, with pointers inside each buffers, are maintained in
  parallel. The 'raw' buffer is the original query text, which may
  contain out-of-bound comments. The 'cpp' (for comments pre processor)
  is the pre-processed buffer that contains only the query text that
  should be seen once out-of-bound data is removed.
*/

class Lex_input_stream
{
public:
  Lex_input_stream(THD *thd, const char* buff, unsigned int length);
  ~Lex_input_stream();

  /**
    Set the echo mode.

    When echo is true, characters parsed from the raw input stream are
    preserved. When false, characters parsed are silently ignored.
    @param echo the echo mode.
  */
  void set_echo(bool echo)
  {
    m_echo= echo;
  }

  /**
    Skip binary from the input stream.
    @param n number of bytes to accept.
  */
  void skip_binary(int n)
  {
    if (m_echo)
    {
      memcpy(m_cpp_ptr, m_ptr, n);
      m_cpp_ptr += n;
    }
    m_ptr += n;
  }

  /**
    Get a character, and advance in the stream.
    @return the next character to parse.
  */
  char yyGet()
  {
    char c= *m_ptr++;
    if (m_echo)
      *m_cpp_ptr++ = c;
    return c;
  }

  /**
    Get the last character accepted.
    @return the last character accepted.
  */
  char yyGetLast()
  {
    return m_ptr[-1];
  }

  /**
    Look at the next character to parse, but do not accept it.
  */
  char yyPeek()
  {
    return m_ptr[0];
  }

  /**
    Look ahead at some character to parse.
    @param n offset of the character to look up
  */
  char yyPeekn(int n)
  {
    return m_ptr[n];
  }

  /**
    Cancel the effect of the last yyGet() or yySkip().
    Note that the echo mode should not change between calls to yyGet / yySkip
    and yyUnget. The caller is responsible for ensuring that.
  */
  void yyUnget()
  {
    m_ptr--;
    if (m_echo)
      m_cpp_ptr--;
  }

  /**
    Accept a character, by advancing the input stream.
  */
  void yySkip()
  {
    if (m_echo)
      *m_cpp_ptr++ = *m_ptr++;
    else
      m_ptr++;
  }

  /**
    Accept multiple characters at once.
    @param n the number of characters to accept.
  */
  void yySkipn(int n)
  {
    if (m_echo)
    {
      memcpy(m_cpp_ptr, m_ptr, n);
      m_cpp_ptr += n;
    }
    m_ptr += n;
  }

  /**
    End of file indicator for the query text to parse.
    @return true if there are no more characters to parse
  */
  bool eof()
  {
    return (m_ptr >= m_end_of_query);
  }

  /**
    End of file indicator for the query text to parse.
    @param n number of characters expected
    @return true if there are less than n characters to parse
  */
  bool eof(int n)
  {
    return ((m_ptr + n) >= m_end_of_query);
  }

  /** Get the raw query buffer. */
  const char *get_buf()
  {
    return m_buf;
  }

  /** Get the pre-processed query buffer. */
  const char *get_cpp_buf()
  {
    return m_cpp_buf;
  }

  /** Get the end of the raw query buffer. */
  const char *get_end_of_query()
  {
    return m_end_of_query;
  }

  /** Mark the stream position as the start of a new token. */
  void start_token()
  {
    m_tok_start_prev= m_tok_start;
    m_tok_start= m_ptr;
    m_tok_end= m_ptr;

    m_cpp_tok_start_prev= m_cpp_tok_start;
    m_cpp_tok_start= m_cpp_ptr;
    m_cpp_tok_end= m_cpp_ptr;
  }

  /**
    Adjust the starting position of the current token.
    This is used to compensate for starting whitespace.
  */
  void restart_token()
  {
    m_tok_start= m_ptr;
    m_cpp_tok_start= m_cpp_ptr;
  }

  /** Get the token start position, in the raw buffer. */
  const char *get_tok_start()
  {
    return m_tok_start;
  }

  /** Get the token start position, in the pre-processed buffer. */
  const char *get_cpp_tok_start()
  {
    return m_cpp_tok_start;
  }

  /** Get the token end position, in the raw buffer. */
  const char *get_tok_end()
  {
    return m_tok_end;
  }

  /** Get the token end position, in the pre-processed buffer. */
  const char *get_cpp_tok_end()
  {
    return m_cpp_tok_end;
  }

  /** Get the previous token start position, in the raw buffer. */
  const char *get_tok_start_prev()
  {
    return m_tok_start_prev;
  }

  /** Get the current stream pointer, in the raw buffer. */
  const char *get_ptr()
  {
    return m_ptr;
  }

  /** Get the current stream pointer, in the pre-processed buffer. */
  const char *get_cpp_ptr()
  {
    return m_cpp_ptr;
  }

  /** Get the length of the current token, in the raw buffer. */
  uint32_t yyLength()
  {
    /*
      The assumption is that the lexical analyser is always 1 character ahead,
      which the -1 account for.
    */
    assert(m_ptr > m_tok_start);
    return (uint32_t) ((m_ptr - m_tok_start) - 1);
  }

  /** Get the utf8-body string. */
  const char *get_body_utf8_str()
  {
    return m_body_utf8;
  }

  /** Get the utf8-body length. */
  uint32_t get_body_utf8_length()
  {
    return m_body_utf8_ptr - m_body_utf8;
  }

  void body_utf8_start(THD *thd, const char *begin_ptr);
  void body_utf8_append(const char *ptr);
  void body_utf8_append(const char *ptr, const char *end_ptr);
  void body_utf8_append_literal(THD *thd,
                                const LEX_STRING *txt,
                                const CHARSET_INFO * const txt_cs,
                                const char *end_ptr);

  /** Current thread. */
  THD *m_thd;

  /** Current line number. */
  uint32_t yylineno;

  /** Length of the last token parsed. */
  uint32_t yytoklen;

  /** Interface with bison, value of the last token parsed. */
  LEX_YYSTYPE yylval;

  /** LALR(2) resolution, look ahead token.*/
  int lookahead_token;

  /** LALR(2) resolution, value of the look ahead token.*/
  LEX_YYSTYPE lookahead_yylval;

private:
  /** Pointer to the current position in the raw input stream. */
  const char *m_ptr;

  /** Starting position of the last token parsed, in the raw buffer. */
  const char *m_tok_start;

  /** Ending position of the previous token parsed, in the raw buffer. */
  const char *m_tok_end;

  /** End of the query text in the input stream, in the raw buffer. */
  const char *m_end_of_query;

  /** Starting position of the previous token parsed, in the raw buffer. */
  const char *m_tok_start_prev;

  /** Begining of the query text in the input stream, in the raw buffer. */
  const char *m_buf;

  /** Length of the raw buffer. */
  uint32_t m_buf_length;

  /** Echo the parsed stream to the pre-processed buffer. */
  bool m_echo;

  /** Pre-processed buffer. */
  char *m_cpp_buf;

  /** Pointer to the current position in the pre-processed input stream. */
  char *m_cpp_ptr;

  /**
    Starting position of the last token parsed,
    in the pre-processed buffer.
  */
  const char *m_cpp_tok_start;

  /**
    Starting position of the previous token parsed,
    in the pre-procedded buffer.
  */
  const char *m_cpp_tok_start_prev;

  /**
    Ending position of the previous token parsed,
    in the pre-processed buffer.
  */
  const char *m_cpp_tok_end;

  /** UTF8-body buffer created during parsing. */
  char *m_body_utf8;

  /** Pointer to the current position in the UTF8-body buffer. */
  char *m_body_utf8_ptr;

  /**
    Position in the pre-processed buffer. The query from m_cpp_buf to
    m_cpp_utf_processed_ptr is converted to UTF8-body.
  */
  const char *m_cpp_utf8_processed_ptr;

public:

  /** Current state of the lexical analyser. */
  enum my_lex_states next_state;

  /**
    Position of ';' in the stream, to delimit multiple queries.
    This delimiter is in the raw buffer.
  */
  const char *found_semicolon;

  /** Token character bitmaps, to detect 7bit strings. */
  uchar tok_bitmap;

  /** SQL_MODE = IGNORE_SPACE. */
  bool ignore_space;

  /** State of the lexical analyser for comments. */
  enum_comment_state in_comment;

  /**
    Starting position of the TEXT_STRING or IDENT in the pre-processed
    buffer.

    NOTE: this member must be used within MYSQLlex() function only.
  */
  const char *m_cpp_text_start;

  /**
    Ending position of the TEXT_STRING or IDENT in the pre-processed
    buffer.

    NOTE: this member must be used within MYSQLlex() function only.
    */
  const char *m_cpp_text_end;

  /**
    Character set specified by the character-set-introducer.

    NOTE: this member must be used within MYSQLlex() function only.
  */
  const CHARSET_INFO *m_underscore_cs;
};


/* The state of the lex parsing. This is saved in the THD struct */

typedef struct st_lex : public Query_tables_list
{
  SELECT_LEX_UNIT unit;                         /* most upper unit */
  SELECT_LEX select_lex;                        /* first SELECT_LEX */
  /* current SELECT_LEX in parsing */
  SELECT_LEX *current_select;
  /* list of all SELECT_LEX */
  SELECT_LEX *all_selects_list;

  char *length,*dec,*change;
  LEX_STRING name;
  char *help_arg;
  char* to_log;                                 /* For PURGE MASTER LOGS TO */
  char* x509_subject,*x509_issuer,*ssl_cipher;
  String *wild;
  sql_exchange *exchange;
  select_result *result;
  Item *default_value, *on_update_value;
  LEX_STRING comment, ident;
  XID *xid;
  uchar* yacc_yyss, *yacc_yyvs;
  THD *thd;

  /* maintain a list of used plugins for this LEX */
  DYNAMIC_ARRAY plugins;
  plugin_ref plugins_static_buffer[INITIAL_LEX_PLUGIN_LIST_SIZE];

  const CHARSET_INFO *charset;
  bool text_string_is_7bit;
  /* store original leaf_tables for INSERT SELECT and PS/SP */
  TABLE_LIST *leaf_tables_insert;

  List<Key_part_spec> col_list;
  List<Key_part_spec> ref_list;
  List<String>	      interval_list;
  List<LEX_COLUMN>    columns;
  List<Item>	      *insert_list,field_list,value_list,update_list;
  List<List_item>     many_values;
  List<set_var_base>  var_list;
  List<Item_param>    param_list;
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
  List<LEX_STRING>     db_list;

  SQL_LIST	      proc_list, auxiliary_table_list, save_list;
  Create_field	      *last_field;
  Item_sum *in_sum_func;
  udf_func udf;
  HA_CHECK_OPT   check_opt;			// check/repair options
  HA_CREATE_INFO create_info;
  KEY_CREATE_INFO key_create_info;
  LEX_MASTER_INFO mi;				// used by CHANGE MASTER
  LEX_SERVER_OPTIONS server_options;
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
  /*
    Usually `expr` rule of yacc is quite reused but some commands better
    not support subqueries which comes standard with this rule, like
    KILL, HA_READ, CREATE/ALTER EVENT etc. Set this to `false` to get
    syntax error back.
  */
  bool expr_allows_subselect;

  thr_lock_type lock_option;
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  enum enum_ha_read_modes ha_read_mode;
  union {
    enum ha_rkey_function ha_rkey_mode;
    enum xa_option_words xa_opt;
    bool lock_transactional;            /* For LOCK TABLE ... IN ... MODE */
  };
  enum enum_var_type option_type;

  uint32_t profile_query_id;
  uint32_t profile_options;
  enum column_format_type column_format;
  uint32_t which_columns;
  enum Foreign_key::fk_match_opt fk_match_option;
  enum Foreign_key::fk_option fk_update_opt;
  enum Foreign_key::fk_option fk_delete_opt;
  uint32_t slave_thd_opt, start_transaction_opt;
  int nest_level;
  /*
    In LEX representing update which were transformed to multi-update
    stores total number of tables. For LEX representing multi-delete
    holds number of tables from which we will delete records.
  */
  uint32_t table_count;
  uint8_t describe;
  /*
    A flag that indicates what kinds of derived tables are present in the
    query (0 if no derived tables, otherwise DERIVED_SUBQUERY).
  */
  uint8_t derived_tables;
  bool drop_if_exists, drop_temporary, local_file, one_shot_set;
  bool autocommit;
  bool verbose, no_write_to_binlog;

  bool tx_chain, tx_release;
  bool subqueries, ignore;
  st_parsing_options parsing_options;
  Alter_info alter_info;

  /*
    field_list was created for view and should be removed before PS/SP
    rexecuton
  */
  bool empty_field_list_on_rset;

  /*
    Pointers to part of LOAD DATA statement that should be rewritten
    during replication ("LOCAL 'filename' REPLACE INTO" part).
  */
  const char *fname_start;
  const char *fname_end;
  
  /**
    During name resolution search only in the table list given by 
    Name_resolution_context::first_name_resolution_table and
    Name_resolution_context::last_name_resolution_table
    (see Item_field::fix_fields()). 
  */
  bool use_only_table_context;
  
  bool escape_used;
  bool is_lex_started; /* If lex_start() did run. For debugging. */

  st_lex();

  virtual ~st_lex()
  {
    destroy_query_tables_list();
    plugin_unlock_list(NULL, (plugin_ref *)plugins.buffer, plugins.elements);
    delete_dynamic(&plugins);
  }

  TABLE_LIST *unlink_first_table(bool *link_to_local);
  void link_first_table_back(TABLE_LIST *first, bool link_to_local);
  void first_lists_tables_same();

  bool can_be_merged();
  bool can_use_merged();
  bool can_not_use_merged();
  bool only_view_structure();
  bool need_correct_ident();

  void cleanup_after_one_table_open();

  bool push_context(Name_resolution_context *context)
  {
    return context_stack.push_front(context);
  }

  void pop_context()
  {
    context_stack.pop();
  }

  bool copy_db_to(char **p_db, size_t *p_db_length) const;

  Name_resolution_context *current_context()
  {
    return context_stack.head();
  }
  /*
    Restore the LEX and THD in case of a parse error.
  */
  static void cleanup_lex_after_parse_error(THD *thd);

  void reset_n_backup_query_tables_list(Query_tables_list *backup);
  void restore_backup_query_tables_list(Query_tables_list *backup);

  bool table_or_sp_used();

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
    if (&select_lex == all_selects_list && !sroutines.records)
    {
      assert(!all_selects_list->next_select_in_list());
      return true;
    }
    return false;
  }
} LEX;

struct st_lex_local: public st_lex
{
  static void *operator new(size_t size) throw()
  {
    return sql_alloc(size);
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw()
  {
    return (void*) alloc_root(mem_root, (uint32_t) size);
  }
  static void operator delete(void *ptr __attribute__((unused)),
                              size_t size __attribute__((unused)))
  { TRASH(ptr, size); }
  static void operator delete(void *ptr __attribute__((unused)),
                              MEM_ROOT *mem_root __attribute__((unused)))
  { /* Never called */ }
};

extern void lex_init(void);
extern void lex_free(void);
extern void lex_start(THD *thd);
extern void lex_end(LEX *lex);

extern void trim_whitespace(const CHARSET_INFO * const cs, LEX_STRING *str);

extern bool is_lex_native_function(const LEX_STRING *name);

/**
  @} (End of group Semantic_Analysis)
*/

#endif /* DRIZZLE_SERVER */
#endif /* DRIZZLE_SERVER_SQL_LEX_H */
