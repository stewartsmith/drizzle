/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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


#ifndef DRIZZLED_TMP_TABLE_H
#define DRIZZLED_TMP_TABLE_H

/*
  Table reference in the FROM clause.

  These table references can be of several types that correspond to
  different SQL elements. Below we list all types of TableLists with
  the necessary conditions to determine when a TableList instance
  belongs to a certain type.

  1) table (TableList::view == NULL)
     - base table
       (TableList::derived == NULL)
     - subquery - TableList::table is a temp table
       (TableList::derived != NULL)
     - information schema table
       (TableList::schema_table != NULL)
       NOTICE: for schema tables TableList::field_translation may be != NULL
  2) view (TableList::view != NULL)
     - merge    (TableList::effective_algorithm == VIEW_ALGORITHM_MERGE)
           also (TableList::field_translation != NULL)
     - tmptable (TableList::effective_algorithm == VIEW_ALGORITHM_TMPTABLE)
           also (TableList::field_translation == NULL)
  3) nested table reference (TableList::nested_join != NULL)
     - table sequence - e.g. (t1, t2, t3)
       TODO: how to distinguish from a JOIN?
     - general JOIN
       TODO: how to distinguish from a table sequence?
     - NATURAL JOIN
       (TableList::natural_join != NULL)
       - JOIN ... USING
         (TableList::join_using_fields != NULL)
     - semi-join
       ;
*/


#include <drizzled/table.h>

class Index_hint;
class COND_EQUAL;
class Natural_join_column;
class select_union;
class st_select_lex_unit;
class ST_SCHEMA_TABLE;
class st_select_lex;
class TMP_TABLE_PARAM;
class Field_translator;
class Item_subselect;
class Table;

struct nested_join_st;

enum enum_schema_table_state
{
  NOT_PROCESSED= 0,
  PROCESSED_BY_CREATE_SORT_INDEX,
  PROCESSED_BY_JOIN_EXEC
};


class TableList
{
public:
  TableList() {}                          /* Remove gcc warning */

  /**
    Prepare TableList that consists of one table instance to use in
    simple_open_and_lock_tables
  */
  inline void init_one_table(const char *db_name_arg,
                             const char *table_name_arg,
                             enum thr_lock_type lock_type_arg)
  {
    memset(this, 0, sizeof(*this));
    db= (char*) db_name_arg;
    table_name= alias= (char*) table_name_arg;
    lock_type= lock_type_arg;
  }

  /*
    List of tables local to a subquery (used by SQL_LIST). Considers
    views as leaves (unlike 'next_leaf' below). Created at parse time
    in st_select_lex::add_table_to_list() -> table_list.link_in_list().
  */
  TableList *next_local;
  /* link in a global list of all queries tables */
  TableList *next_global, **prev_global;
  char		*db, *alias, *table_name, *schema_table_name;
  char          *option;                /* Used by cache index  */
  Item		*on_expr;		/* Used with outer join */
  Item          *sj_on_expr;
  /*
    (Valid only for semi-join nests) Bitmap of tables that are within the
    semi-join (this is different from bitmap of all nest's children because
    tables that were pulled out of the semi-join nest remain listed as
    nest's children).
  */
  table_map     sj_inner_tables;
  /* Number of IN-compared expressions */
  uint32_t          sj_in_exprs; 
  /*
    The structure of ON expression presented in the member above
    can be changed during certain optimizations. This member
    contains a snapshot of AND-OR structure of the ON expression
    made after permanent transformations of the parse tree, and is
    used to restore ON clause before every reexecution of a prepared
    statement or stored procedure.
  */
  Item          *prep_on_expr;
  COND_EQUAL    *cond_equal;            /* Used with outer join */
  /*
    During parsing - left operand of NATURAL/USING join where 'this' is
    the right operand. After parsing (this->natural_join == this) iff
    'this' represents a NATURAL or USING join operation. Thus after
    parsing 'this' is a NATURAL/USING join iff (natural_join != NULL).
  */
  TableList *natural_join;
  /*
    True if 'this' represents a nested join that is a NATURAL JOIN.
    For one of the operands of 'this', the member 'natural_join' points
    to the other operand of 'this'.
  */
  bool is_natural_join;
  /* Field names in a USING clause for JOIN ... USING. */
  List<String> *join_using_fields;
  /*
    Explicitly store the result columns of either a NATURAL/USING join or
    an operand of such a join.
  */
  List<Natural_join_column> *join_columns;
  /* true if join_columns contains all columns of this table reference. */
  bool is_join_columns_complete;

  /*
    List of nodes in a nested join tree, that should be considered as
    leaves with respect to name resolution. The leaves are: views,
    top-most nodes representing NATURAL/USING joins, subqueries, and
    base tables. All of these TableList instances contain a
    materialized list of columns. The list is local to a subquery.
  */
  TableList *next_name_resolution_table;
  /* Index names in a "... JOIN ... USE/IGNORE INDEX ..." clause. */
  List<Index_hint> *index_hints;
  Table        *table;    /* opened table */
  uint32_t          table_id; /* table id (from binlog) for opened table */
  /*
    select_result for derived table to pass it from table creation to table
    filling procedure
  */
  select_union  *derived_result;
  /*
    Reference from aux_tables to local list entry of main select of
    multi-delete statement:
    delete t1 from t2,t1 where t1.a<'B' and t2.b=t1.b;
    here it will be reference of first occurrence of t1 to second (as you
    can see this lists can't be merged)
  */
  TableList	*correspondent_table;
  st_select_lex_unit *derived;		/* SELECT_LEX_UNIT of derived table */
  ST_SCHEMA_TABLE *schema_table;        /* Information_schema table */
  st_select_lex	*schema_select_lex;
  /*
    True when the view field translation table is used to convert
    schema table fields for backwards compatibility with SHOW command.
  */
  bool schema_table_reformed;
  TMP_TABLE_PARAM *schema_table_param;
  /* link to select_lex where this table was used */
  st_select_lex	*select_lex;
  Field_translator *field_translation;	/* array of VIEW fields */
  /* pointer to element after last one in translation table above */
  Field_translator *field_translation_end;
  /*
    List (based on next_local) of underlying tables of this view. I.e. it
    does not include the tables of subqueries used in the view. Is set only
    for merged views.
  */
  TableList	*merge_underlying_list;
  /*
    List of all base tables local to a subquery including all view
    tables. Unlike 'next_local', this in this list views are *not*
    leaves. Created in setup_tables() -> make_leaves_list().
  */
  TableList	*next_leaf;
  thr_lock_type lock_type;
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  size_t        db_length;
  size_t        table_name_length;
  bool		straight;		/* optimize with prev table */
  bool          updating;               /* for replicate-do/ignore table */
  bool		force_index;		/* prefer index over table scan */
  bool          ignore_leaves;          /* preload only non-leaf nodes */
  table_map     dep_tables;             /* tables the table depends on      */
  table_map     on_expr_dep_tables;     /* tables on expression depends on  */
  nested_join_st *nested_join;   /* if the element is a nested join  */
  TableList *embedding;             /* nested join containing the table */
  List<TableList> *join_list;/* join list the table belongs to   */
  bool		cacheable_table;	/* stop PS caching */
  handlerton	*db_type;		/* table_type for handler */
  char		timestamp_buffer[20];	/* buffer for timestamp (19+1) */
  /*
    This TableList object corresponds to the table to be created
    so it is possible that it does not exist (used in CREATE TABLE
    ... SELECT implementation).
  */
  bool          create;
  /* For transactional locking. */
  int           lock_timeout;           /* NOWAIT or WAIT [X]               */
  bool          lock_transactional;     /* If transactional lock requested. */
  bool          internal_tmp_table;
  /** true if an alias for this table was specified in the SQL. */
  bool          is_alias;
  /** true if the table is referred to in the statement using a fully
      qualified name (<db_name>.<table_name>).
  */
  bool          is_fqtn;

  uint32_t i_s_requested_object;
  bool has_db_lookup_value;
  bool has_table_lookup_value;
  uint32_t table_open_method;
  enum enum_schema_table_state schema_table_state;
  void set_underlying_merge();
  bool setup_underlying(Session *session);
  void cleanup_items();
  /*
    If you change placeholder(), please check the condition in
    check_transactional_lock() too.
  */
  bool placeholder()
  {
    return derived || schema_table || (create && !table->getDBStat()) || !table;
  }
  void print(Session *session, String *str, enum_query_type query_type);
  bool set_insert_values(MEM_ROOT *mem_root);
  TableList *find_underlying_table(Table *table);
  TableList *first_leaf_for_name_resolution();
  TableList *last_leaf_for_name_resolution();
  bool is_leaf_for_name_resolution();
  inline TableList *top_table()
    { return this; }

  /*
    Cleanup for re-execution in a prepared statement or a stored
    procedure.
  */
  void reinit_before_use(Session *session);
  Item_subselect *containing_subselect();

  /* 
    Compiles the tagged hints list and fills up st_table::keys_in_use_for_query,
    st_table::keys_in_use_for_group_by, st_table::keys_in_use_for_order_by,
    st_table::force_index and st_table::covering_keys.
  */
  bool process_index_hints(Table *table);
};

void close_thread_tables(Session *session);

#endif /* DRIZZLED_TMP_TABLE_H */
