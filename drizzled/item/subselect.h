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

#include <drizzled/comp_creator.h>
#include <drizzled/item/ref.h>
#include <drizzled/item/field.h>
#include <drizzled/item/bin_string.h>
#include <drizzled/util/test.h>

namespace drizzled {

class Item_subselect :public Item_result_field
{
  bool value_assigned; /* value already assigned to subselect */
public:
  /* thread handler, will be assigned in fix_fields only */
  Session *session;
  /* substitution instead of subselect in case of optimization */
  Item *substitution;
  /* unit of subquery */
  Select_Lex_Unit *unit;
protected:
  /* engine that perform execution of subselect (single select or union) */
  subselect_engine *engine;
  /* old engine if engine was changed */
  subselect_engine *old_engine;
  /* cache of used external tables */
  table_map used_tables_cache;
  /* allowed number of columns (1 for single value subqueries) */
  uint32_t max_columns;
  /* where subquery is placed */
  enum_parsing_place parsing_place;
  /* work with 'substitution' */
  bool have_to_be_excluded;
  /* cache of constant state */
  bool const_item_cache;

public:
  /* changed engine indicator */
  bool engine_changed;
  /* subquery is transformed */
  bool changed;

  /* TRUE <=> The underlying SELECT is correlated w.r.t some ancestor select */
  bool is_correlated;

  enum trans_res {RES_OK, RES_REDUCE, RES_ERROR};
  enum subs_type {UNKNOWN_SUBS, SINGLEROW_SUBS,
		  EXISTS_SUBS, IN_SUBS, ALL_SUBS, ANY_SUBS};

  Item_subselect();

  virtual subs_type substype() { return UNKNOWN_SUBS; }

  /*
    We need this method, because some compilers do not allow 'this'
    pointer in constructor initialization list, but we need to pass a pointer
    to subselect Item class to select_result_interceptor's constructor.
  */
  virtual void init (Select_Lex *select_lex,
		     select_result_interceptor *result);

  ~Item_subselect();
  void cleanup();
  virtual void reset()
  {
    null_value= 1;
  }
  virtual trans_res select_transformer(Join *join);
  bool assigned() { return value_assigned; }
  void assigned(bool a) { value_assigned= a; }
  enum Type type() const;
  bool is_null()
  {
    update_null_value();
    return null_value;
  }
  bool fix_fields(Session *session, Item **ref);
  virtual bool exec();
  virtual void fix_length_and_dec();
  table_map used_tables() const;
  table_map not_null_tables() const { return 0; }
  bool const_item() const;
  inline table_map get_used_tables_cache() { return used_tables_cache; }
  inline bool get_const_item_cache() { return const_item_cache; }
  Item *get_tmp_table_item(Session *session);
  void update_used_tables();
  virtual void print(String *str);
  virtual bool have_guarded_conds() { return false; }
  bool change_engine(subselect_engine *eng)
  {
    old_engine= engine;
    engine= eng;
    engine_changed= 1;
    return eng == 0;
  }
  /*
    True if this subquery has been already evaluated. Implemented only for
    single select and union subqueries only.
  */
  bool is_evaluated() const;
  bool is_uncacheable() const;

  /*
    Used by max/min subquery to initialize value presence registration
    mechanism. Engine call this method before rexecution query.
  */
  virtual void reset_value_registration() {}
  enum_parsing_place place() { return parsing_place; }
  bool walk(Item_processor processor, bool walk_subquery, unsigned char *arg);

  /**
    Get the Select_Lex structure associated with this Item.
    @return the Select_Lex structure associated with this Item
  */
  Select_Lex* get_select_lex();

  friend class select_result_interceptor;
  friend class Item_in_optimizer;
  friend bool Item_field::fix_fields(Session *, Item **);
  friend int  Item_field::fix_outer_field(Session *, Field **, Item **);
  friend bool Item_ref::fix_fields(Session *, Item **);
  friend void mark_select_range_as_dependent(Session*,
                                             Select_Lex*, Select_Lex*,
                                             Field*, Item*, Item_ident*);
};

/* single value subselect */

class Item_singlerow_subselect :public Item_subselect
{
protected:
  Item_cache *value, **row;
public:
  Item_singlerow_subselect(Select_Lex *select_lex);
  Item_singlerow_subselect() :Item_subselect(), value(0), row (0) {}

  void cleanup();
  subs_type substype() { return SINGLEROW_SUBS; }

  void reset();
  trans_res select_transformer(Join *join);
  void store(uint32_t i, Item* item);
  double val_real();
  int64_t val_int ();
  String *val_str (String *);
  type::Decimal *val_decimal(type::Decimal *);
  bool val_bool();
  enum Item_result result_type() const;
  enum_field_types field_type() const;
  void fix_length_and_dec();

  uint32_t cols();
  Item* element_index(uint32_t i) { return reinterpret_cast<Item*>(row[i]); }
  Item** addr(uint32_t i) { return (Item**)row + i; }
  bool check_cols(uint32_t c);
  bool null_inside();
  void bring_value();

  /**
    This method is used to implement a special case of semantic tree
    rewriting, mandated by a SQL:2003 exception in the specification.
    The only caller of this method is handle_sql2003_note184_exception(),
    see the code there for more details.
    Note that this method breaks the object internal integrity, by
    removing it's association with the corresponding Select_Lex,
    making this object orphan from the parse tree.
    No other method, beside the destructor, should be called on this
    object, as it is now invalid.
    @return the Select_Lex structure that was given in the constructor.
  */
  Select_Lex* invalidate_and_restore_select_lex();

  friend class select_singlerow_subselect;
};

/* used in static ALL/ANY optimization */
class Item_maxmin_subselect :public Item_singlerow_subselect
{
protected:
  bool max;
  bool was_values;  // Set if we have found at least one row
public:
  Item_maxmin_subselect(Session *session, Item_subselect *parent,
			Select_Lex *select_lex, bool max);
  virtual void print(String *str);
  void cleanup();
  bool any_value() { return was_values; }
  void register_value() { was_values= true; }
  void reset_value_registration() { was_values= false; }
};

/* exists subselect */

class Item_exists_subselect :public Item_subselect
{
protected:
  bool value; /* value of this item (boolean: exists/not-exists) */

public:
  Item_exists_subselect(Select_Lex *select_lex);
  Item_exists_subselect(): Item_subselect() {}

  subs_type substype() { return EXISTS_SUBS; }
  void reset()
  {
    value= 0;
  }

  enum Item_result result_type() const { return INT_RESULT;}
  int64_t val_int();
  double val_real();
  String *val_str(String*);
  type::Decimal *val_decimal(type::Decimal *);
  bool val_bool();
  void fix_length_and_dec();
  virtual void print(String *str);

  friend class select_exists_subselect;
  friend class subselect_uniquesubquery_engine;
  friend class subselect_indexsubquery_engine;
};


/**
  Representation of IN subquery predicates of the form
  "left_expr IN (SELECT ...)".

  @detail
  This class has:
   - A "subquery execution engine" (as a subclass of Item_subselect) that allows
     it to evaluate subqueries. (and this class participates in execution by
     having was_null variable where part of execution result is stored.
   - Transformation methods (todo: more on this).

  This class is not used directly, it is "wrapped" into Item_in_optimizer
  which provides some small bits of subquery evaluation.
*/

class Item_in_subselect :public Item_exists_subselect
{
public:
  Item *left_expr;
protected:
  /*
    Cache of the left operand of the subquery predicate. Allocated in the
    runtime memory root, for each execution, thus need not be freed.
  */
  List<Cached_item> *left_expr_cache;
  bool first_execution;

  /*
    expr & optimizer used in subselect rewriting to store Item for
    all JOIN in UNION
  */
  Item *expr;
  Item_in_optimizer *optimizer;
  bool was_null;
  bool abort_on_null;

public:
  /* Used to trigger on/off conditions that were pushed down to subselect */
  bool *pushed_cond_guards;

  /* Priority of this predicate in the convert-to-semi-join-nest process. */
  int sj_convert_priority;

  /*
    Location of the subquery predicate. It is either
     - pointer to join nest if the subquery predicate is in the ON expression
     - (TableList*)1 if the predicate is in the WHERE.
  */
  TableList *expr_join_nest;

  /* The method chosen to execute the IN predicate.  */
  enum enum_exec_method {
    NOT_TRANSFORMED, /* No execution method was chosen for this IN. */
    SEMI_JOIN,   /* IN was converted to semi-join nest and should be removed. */
    IN_TO_EXISTS, /* IN was converted to correlated EXISTS. */
    MATERIALIZATION /* IN will be executed via subquery materialization. */
  };
  enum_exec_method exec_method;

  bool *get_cond_guard(int i)
  {
    return pushed_cond_guards ? pushed_cond_guards + i : NULL;
  }
  void set_cond_guard_var(int i, bool v)
  {
    if ( pushed_cond_guards)
      pushed_cond_guards[i]= v;
  }
  bool have_guarded_conds() { return test(pushed_cond_guards); }

  Item_func_not_all *upper_item; // point on NOT/NOP before ALL/SOME subquery

  Item_in_subselect(Item * left_expr, Select_Lex *select_lex);
  Item_in_subselect()
    :
      Item_exists_subselect(),
      left_expr(NULL),
      left_expr_cache(NULL),
      first_execution(true),
      optimizer(NULL),
      abort_on_null(false),
      pushed_cond_guards(NULL),
      sj_convert_priority(0),
      expr_join_nest(NULL),
      exec_method(NOT_TRANSFORMED),
      upper_item(NULL)
  {}
  void cleanup();
  subs_type substype() { return IN_SUBS; }
  void reset()
  {
    value= 0;
    null_value= 0;
    was_null= 0;
  }
  trans_res select_transformer(Join *join);
  trans_res select_in_like_transformer(Join *join, const Comp_creator *func);
  trans_res single_value_transformer(Join *join, const Comp_creator *func);
  trans_res row_value_transformer(Join * join);
  trans_res single_value_in_to_exists_transformer(Join * join,
                                                  const Comp_creator *func);
  trans_res row_value_in_to_exists_transformer(Join * join);
  virtual bool exec();
  int64_t val_int();
  double val_real();
  String *val_str(String*);
  type::Decimal *val_decimal(type::Decimal *);
  void update_null_value () { (void) val_bool(); }
  bool val_bool();
  void top_level_item() { abort_on_null=1; }
  inline bool is_top_level_item() { return abort_on_null; }
  bool test_limit(Select_Lex_Unit *unit);
  virtual void print(String *str);
  bool fix_fields(Session *session, Item **ref);
  bool setup_engine();
  bool init_left_expr_cache();
  bool is_expensive_processor(unsigned char *arg);

  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
  friend class Item_in_optimizer;
  friend class subselect_indexsubquery_engine;
  friend class subselect_hash_sj_engine;
};


/* ALL/ANY/SOME subselect */
class Item_allany_subselect :public Item_in_subselect
{
public:
  chooser_compare_func_creator func_creator;
  Comp_creator *func;
  bool all;

  Item_allany_subselect(Item * left_expr, chooser_compare_func_creator fc,
                        Select_Lex *select_lex, bool all);

  // only ALL subquery has upper not
  subs_type substype() { return all?ALL_SUBS:ANY_SUBS; }
  trans_res select_transformer(Join *join);
  virtual void print(String *str);
};


class subselect_engine: public memory::SqlAlloc
{
protected:
  select_result_interceptor *result; /* results storage class */
  Session *session; /* pointer to current Session */
  Item_subselect *item; /* item, that use this engine */
  enum Item_result res_type; /* type of results */
  enum_field_types res_field_type; /* column type of the results */
  bool maybe_null; /* may be null (first item in select) */
public:

  enum enum_engine_type {ABSTRACT_ENGINE, SINGLE_SELECT_ENGINE,
                         UNION_ENGINE, UNIQUESUBQUERY_ENGINE,
                         INDEXSUBQUERY_ENGINE, HASH_SJ_ENGINE};

  subselect_engine(Item_subselect *si, select_result_interceptor *res)
    :session(NULL)
  {
    result= res;
    item= si;
    res_type= STRING_RESULT;
    res_field_type= DRIZZLE_TYPE_VARCHAR;
    maybe_null= 0;
  }
  virtual ~subselect_engine() {} // to satisfy compiler
  virtual void cleanup()= 0;

  /*
    Also sets "session" for subselect_engine::result.
    Should be called before prepare().
  */
  void set_session(Session *session_arg);
  Session * get_session() { return session; }
  virtual int prepare()= 0;
  virtual void fix_length_and_dec(Item_cache** row)= 0;
  /*
    Execute the engine

    SYNOPSIS
      exec()

    DESCRIPTION
      Execute the engine. The result of execution is subquery value that is
      either captured by previously set up select_result-based 'sink' or
      stored somewhere by the exec() method itself.

      A required side effect: If at least one pushed-down predicate is
      disabled, subselect_engine->no_rows() must return correct result after
      the exec() call.

    RETURN
      0 - OK
      1 - Either an execution error, or the engine was "changed", and the
          caller should call exec() again for the new engine.
  */
  virtual int exec()= 0;
  virtual uint32_t cols()= 0; /* return number of columns in select */
  virtual bool uncacheable()= 0; /* query is uncacheable */
  virtual bool uncacheable(uint32_t bit_pos)= 0; /* query is uncacheable */
  enum Item_result type() { return res_type; }
  enum_field_types field_type() { return res_field_type; }
  virtual void exclude()= 0;
  virtual bool may_be_null() { return maybe_null; }
  virtual table_map upper_select_const_tables()= 0;
  static table_map calc_const_tables(TableList *);
  virtual void print(String *str)= 0;
  virtual bool change_result(Item_subselect *si,
                             select_result_interceptor *result)= 0;
  virtual bool no_tables()= 0;
  virtual bool is_executed() const { return false; }
  /* Check if subquery produced any rows during last query execution */
  virtual bool no_rows() = 0;
  virtual enum_engine_type engine_type() { return ABSTRACT_ENGINE; }

protected:
  void set_row(List<Item> &item_list, Item_cache **row);
};


class subselect_single_select_engine: public subselect_engine
{
  bool prepared; /* simple subselect is prepared */
  bool optimized; /* simple subselect is optimized */
  bool executed; /* simple subselect is executed */
  Select_Lex *select_lex; /* corresponding select_lex */
  Join * join; /* corresponding JOIN structure */
public:
  subselect_single_select_engine(Select_Lex *select,
				 select_result_interceptor *result,
				 Item_subselect *item);
  void cleanup();
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint32_t cols();
  bool uncacheable();
  bool uncacheable(uint32_t bit_pos);
  void exclude();
  table_map upper_select_const_tables();
  virtual void print (String *str);
  bool change_result(Item_subselect *si, select_result_interceptor *result);
  bool no_tables();
  bool may_be_null();
  bool is_executed() const { return executed; }
  bool no_rows();
  virtual enum_engine_type engine_type() { return SINGLE_SELECT_ENGINE; }
  void save_join_if_explain();

  friend class subselect_hash_sj_engine;
  friend class Item_in_subselect;
};


class subselect_union_engine: public subselect_engine
{
  Select_Lex_Unit *unit;  /* corresponding unit structure */
public:
  subselect_union_engine(Select_Lex_Unit *u,
			 select_result_interceptor *result,
			 Item_subselect *item);
  void cleanup();
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint32_t cols();
  bool uncacheable();
  bool uncacheable(uint32_t bit_pos);
  void exclude();
  table_map upper_select_const_tables();
  virtual void print (String *str);
  bool change_result(Item_subselect *si, select_result_interceptor *result);
  bool no_tables();
  bool is_executed() const;
  bool no_rows();
  virtual enum_engine_type engine_type() { return UNION_ENGINE; }
};


/*
  A subquery execution engine that evaluates the subquery by doing one index
  lookup in a unique index.

  This engine is used to resolve subqueries in forms

    outer_expr IN (SELECT tbl.unique_key FROM tbl WHERE subq_where)

  or, tuple-based:

    (oe1, .. oeN) IN (SELECT uniq_key_part1, ... uniq_key_partK
                      FROM tbl WHERE subqwhere)

  i.e. the subquery is a single table SELECT without GROUP BY, aggregate
  functions, etc.
*/

class subselect_uniquesubquery_engine: public subselect_engine
{
protected:
  JoinTable *tab;
  Item *cond; /* The WHERE condition of subselect */
  /*
    TRUE<=> last execution produced empty set. Valid only when left
    expression is NULL.
  */
  bool empty_result_set;
  bool null_keypart; /* TRUE <=> constructed search tuple has a NULL */
public:

  // constructor can assign Session because it will be called after Join::prepare
  subselect_uniquesubquery_engine(Session *session_arg, JoinTable *tab_arg,
				  Item_subselect *subs, Item *where)
    :subselect_engine(subs, 0), tab(tab_arg), cond(where)
  {
    set_session(session_arg);
  }
  void cleanup();
  int prepare();
  void fix_length_and_dec(Item_cache** row);
  int exec();
  uint32_t cols() { return 1; }
  bool uncacheable() { return true; }
  bool uncacheable(uint32_t) { return true; }
  void exclude();
  table_map upper_select_const_tables() { return 0; }
  virtual void print (String *str);
  bool change_result(Item_subselect *si, select_result_interceptor *result);
  bool no_tables();
  int scan_table();
  bool copy_ref_key();
  bool no_rows() { return empty_result_set; }
  virtual enum_engine_type engine_type() { return UNIQUESUBQUERY_ENGINE; }
};


class subselect_indexsubquery_engine: public subselect_uniquesubquery_engine
{
  /* FALSE for 'ref', TRUE for 'ref-or-null'. */
  bool check_null;
  /*
    The "having" clause. This clause (further reffered to as "artificial
    having") was inserted by subquery transformation code. It contains
    Item(s) that have a side-effect: they record whether the subquery has
    produced a row with NULL certain components. We need to use it for cases
    like
      (oe1, oe2) IN (SELECT t.key, t.no_key FROM t1)
    where we do index lookup on t.key=oe1 but need also to check if there
    was a row such that t.no_key IS NULL.

    NOTE: This is currently here and not in the uniquesubquery_engine. Ideally
    it should have been in uniquesubquery_engine in order to allow execution of
    subqueries like

      (oe1, oe2) IN (SELECT primary_key, non_key_maybe_null_field FROM tbl)

    We could use uniquesubquery_engine for the first component and let
    Item_is_not_null_test( non_key_maybe_null_field) to handle the second.

    However, subqueries like the above are currently not handled by index
    lookup-based subquery engines, the engine applicability check misses
    them: it doesn't switch the engine for case of artificial having and
    [eq_]ref access (only for artifical having + ref_or_null or no having).
    The above example subquery is handled as a full-blown SELECT with eq_ref
    access to one table.

    Due to this limitation, the "artificial having" currently needs to be
    checked by only in indexsubquery_engine.
  */
  Item *having;
public:

  // constructor can assign Session because it will be called after Join::prepare
  subselect_indexsubquery_engine(Session *session_arg, JoinTable *tab_arg,
				 Item_subselect *subs, Item *where,
                                 Item *having_arg, bool chk_null)
    :subselect_uniquesubquery_engine(session_arg, tab_arg, subs, where),
     check_null(chk_null),
     having(having_arg)
  {}
  int exec();
  virtual void print (String *str);
  virtual enum_engine_type engine_type() { return INDEXSUBQUERY_ENGINE; }
};


inline bool Item_subselect::is_evaluated() const
{
  return engine->is_executed();
}


inline bool Item_subselect::is_uncacheable() const
{
  return engine->uncacheable();
}


/**
  Compute an IN predicate via a hash semi-join. The subquery is materialized
  during the first evaluation of the IN predicate. The IN predicate is executed
  via the functionality inherited from subselect_uniquesubquery_engine.
*/

class subselect_hash_sj_engine: public subselect_uniquesubquery_engine
{
protected:
  /* TRUE if the subquery was materialized into a temp table. */
  bool is_materialized;
  /*
    The old engine already chosen at parse time and stored in permanent memory.
    Through this member we can re-create and re-prepare materialize_join for
    each execution of a prepared statement. We akso resuse the functionality
    of subselect_single_select_engine::[prepare | cols].
  */
  subselect_single_select_engine *materialize_engine;
  /*
    QEP to execute the subquery and materialize its result into a
    temporary table. Created during the first call to exec().
  */
  Join *materialize_join;
  /* Temp table context of the outer select's JOIN. */
  Tmp_Table_Param *tmp_param;

public:
  subselect_hash_sj_engine(Session *session_in, Item_subselect *in_predicate,
                               subselect_single_select_engine *old_engine)
    :subselect_uniquesubquery_engine(session_in, NULL, in_predicate, NULL),
    is_materialized(false), materialize_engine(old_engine),
    materialize_join(NULL), tmp_param(NULL)
  {}
  ~subselect_hash_sj_engine();

  bool init_permanent(List<Item> *tmp_columns);
  void init_runtime();
  void cleanup();
  int prepare() { return 0; }
  int exec();
  virtual void print (String *str);
  uint32_t cols()
  {
    return materialize_engine->cols();
  }
  virtual enum_engine_type engine_type() { return HASH_SJ_ENGINE; }
};

} /* namespace drizzled */

