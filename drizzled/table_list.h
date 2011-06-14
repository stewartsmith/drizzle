/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/nested_join.h>
#include <drizzled/table.h>

namespace drizzled {

/**
 * A Table referenced in the FROM clause.
 *
 * These table references can be of several types that correspond to
 * different SQL elements. Below we list all types of TableLists with
 * the necessary conditions to determine when a TableList instance
 * belongs to a certain type.
 *
 * 1) table (TableList::view == NULL)
 *    - base table
 *    (TableList::derived == NULL)
 *    - subquery - TableList::table is a temp table
 *    (TableList::derived != NULL)
 *    
 *    @note
 *
 *    for schema tables TableList::field_translation may be != NULL
 *
 * 2) Was VIEW 
 * 3) nested table reference (TableList::nested_join != NULL)
 *     - table sequence - e.g. (t1, t2, t3)
 *     @todo how to distinguish from a JOIN?
 *     - general JOIN
 *     @todo how to distinguish from a table sequence?
 *     - NATURAL JOIN
 *     (TableList::natural_join != NULL)
 *     - JOIN ... USING
 *     (TableList::join_using_fields != NULL)
 *     - semi-join
 */
class TableList
{
public:
  TableList():
    next_local(NULL),
    next_global(NULL),
    prev_global(NULL),
    db(NULL),
    alias(NULL),
    table_name(NULL),
    option(NULL),
    on_expr(NULL),
    table(NULL),
    prep_on_expr(NULL),
    cond_equal(NULL),
    natural_join(NULL),
    is_natural_join(false),
    is_join_columns_complete(false),
    straight(false),
    force_index(false),
    ignore_leaves(false),
    join_using_fields(NULL),
    join_columns(NULL),
    next_name_resolution_table(NULL),
    index_hints(NULL),
    derived_result(NULL),
    derived(NULL),
    schema_select_lex(NULL),
    select_lex(NULL),
    next_leaf(NULL),
    outer_join(0),
    db_length(0),
    table_name_length(0),
    dep_tables(0),
    on_expr_dep_tables(0),
    nested_join(NULL),
    embedding(NULL),
    join_list(NULL),
    db_type(NULL),
    internal_tmp_table(false),
    is_alias(false),
    is_fqtn(false),
    create(false)
  {}

  /**
   * List of tables local to a subquery (used by SQL_LIST). Considers
   * views as leaves (unlike 'next_leaf' below). Created at parse time
   * in Select_Lex::add_table_to_list() -> table_list.link_in_list().
   */
  TableList *next_local;

  /** link in a global list of all queries tables */
  TableList *next_global; 
  TableList **prev_global;

private:
  char *db;

public:
  const char *getSchemaName()
  {
    return db;
  }

  char **getSchemaNamePtr()
  {
    return &db;
  }

  void setSchemaName(char *arg)
  {
    db= arg;
  }

  const char *alias;

private:
  const char *table_name;

public:
  const char *getTableName()
  {
    return table_name;
  }

  void setTableName(const char *arg)
  {
    table_name= arg;
  }

  char *option; ///< Used by cache index
  Item *on_expr; ///< Used with outer join
  Table *table; ///< opened table
  /**
   * The structure of ON expression presented in the member above
   * can be changed during certain optimizations. This member
   * contains a snapshot of AND-OR structure of the ON expression
   * made after permanent transformations of the parse tree, and is
   * used to restore ON clause before every reexecution of a prepared
   * statement or stored procedure.
   */
  Item *prep_on_expr;
  COND_EQUAL *cond_equal; ///< Used with outer join
  /**
   * During parsing - left operand of NATURAL/USING join where 'this' is
   * the right operand. After parsing (this->natural_join == this) iff
   * 'this' represents a NATURAL or USING join operation. Thus after
   * parsing 'this' is a NATURAL/USING join iff (natural_join != NULL).
   */
  TableList *natural_join;
  /**
   * True if 'this' represents a nested join that is a NATURAL JOIN.
   * For one of the operands of 'this', the member 'natural_join' points
   * to the other operand of 'this'.
   */
  bool is_natural_join;

  /** true if join_columns contains all columns of this table reference. */
  bool is_join_columns_complete;

  bool straight; ///< optimize with prev table
  bool force_index; ///< prefer index over table scan
  bool ignore_leaves; ///< preload only non-leaf nodes

  /*
    is the table a cartesian join, assumption is yes unless "solved"
  */
  bool isCartesian() const;

  /** Field names in a USING clause for JOIN ... USING. */
  List<String> *join_using_fields;
  /**
   * Explicitly store the result columns of either a NATURAL/USING join or
   * an operand of such a join.
   */
  List<Natural_join_column> *join_columns;

  /**
   * List of nodes in a nested join tree, that should be considered as
   * leaves with respect to name resolution. The leaves are: views,
   * top-most nodes representing NATURAL/USING joins, subqueries, and
   * base tables. All of these TableList instances contain a
   * materialized list of columns. The list is local to a subquery.
   */
  TableList *next_name_resolution_table;
  /** Index names in a "... JOIN ... USE/IGNORE INDEX ..." clause. */
  List<Index_hint> *index_hints;
  /**
   * select_result for derived table to pass it from table creation to table
   * filling procedure
   */
  select_union *derived_result;
  Select_Lex_Unit *derived; ///< Select_Lex_Unit of derived table */
  Select_Lex *schema_select_lex;
  /** link to select_lex where this table was used */
  Select_Lex *select_lex;
  /**
   * List of all base tables local to a subquery including all view
   * tables. Unlike 'next_local', this in this list views are *not*
   * leaves. Created in setup_tables() -> make_leaves_list().
   */
  TableList *next_leaf;
  thr_lock_type lock_type;
  uint32_t outer_join; ///< Which join type
  size_t db_length;
  size_t table_name_length;

  void set_underlying_merge();
  bool setup_underlying(Session *session);

  /**
   * If you change placeholder(), please check the condition in
   * check_transactional_lock() too.
   */
  bool placeholder();
  /**
   * Print table as it should be in join list.
   * 
   * @param str   string where table should be printed
   */
  void print(Session *session, String *str);
  /**
   * Sets insert_values buffer
   *
   * @param[in] memory pool for allocating
   *
   * @retval
   *  false - OK
   * @retval
   *  true - out of memory
   */
  bool set_insert_values(memory::Root *mem_root);
  /**
   * Find underlying base tables (TableList) which represent given
   * table_to_find (Table)
   *
   * @param[in] table to find
   *
   * @retval
   *  NULL if table is not found
   * @retval
   *  Pointer to found table reference
   */
  TableList *find_underlying_table(Table *table);
  /**
   * Retrieve the first (left-most) leaf in a nested join tree with
   * respect to name resolution.
   *
   * @details
   *
   * Given that 'this' is a nested table reference, recursively walk
   * down the left-most children of 'this' until we reach a leaf
   * table reference with respect to name resolution.
   *
   * @retval
   *  If 'this' is a nested table reference - the left-most child of
   *  the tree rooted in 'this',
   *  else return 'this'
   */
  TableList *first_leaf_for_name_resolution();
  /**
   * Retrieve the last (right-most) leaf in a nested join tree with
   * respect to name resolution.
   *
   * @details
   *
   * Given that 'this' is a nested table reference, recursively walk
   * down the right-most children of 'this' until we reach a leaf
   * table reference with respect to name resolution.
   *
   * @retval
   *  If 'this' is a nested table reference - the right-most child of
   *  the tree rooted in 'this',
   *  else 'this'
   */
  TableList *last_leaf_for_name_resolution();
  /**
   * Test if this is a leaf with respect to name resolution.
   *
   * @details
   * 
   * A table reference is a leaf with respect to name resolution if
   * it is either a leaf node in a nested join tree (table, view,
   * schema table, subquery), or an inner node that represents a
   * NATURAL/USING join, or a nested join with materialized join
   * columns.
   *
   * @retval
   *  true if a leaf, false otherwise.
   */
  bool is_leaf_for_name_resolution();
  inline TableList *top_table()
  { return this; }

  /**
   * Return subselect that contains the FROM list this table is taken from
   *
   * @retval
   *  Subselect item for the subquery that contains the FROM list
   *  this table is taken from if there is any
   * @retval
   *  NULL otherwise
   */
  Item_subselect *containing_subselect();

  /**
   * Compiles the tagged hints list and fills up st_table::keys_in_use_for_query,
   * st_table::keys_in_use_for_group_by, st_table::keys_in_use_for_order_by,
   * st_table::force_index and st_table::covering_keys.
   *
   * @param the Table to operate on.
   *
   * @details
   *
   * The parser collects the index hints for each table in a "tagged list"
   * (TableList::index_hints). Using the information in this tagged list
   * this function sets the members Table::keys_in_use_for_query,
   * Table::keys_in_use_for_group_by, Table::keys_in_use_for_order_by,
   * Table::force_index and Table::covering_keys.
   *
   * Current implementation of the runtime does not allow mixing FORCE INDEX
   * and USE INDEX, so this is checked here. Then the FORCE INDEX list
   * (if non-empty) is appended to the USE INDEX list and a flag is set.
   * 
   * Multiple hints of the same kind are processed so that each clause
   * is applied to what is computed in the previous clause.
   * 
   * For example:
   *       USE INDEX (i1) USE INDEX (i2)
   *    is equivalent to
   *       USE INDEX (i1,i2)
   *    and means "consider only i1 and i2".
   *
   * Similarly
   *       USE INDEX () USE INDEX (i1)
   *    is equivalent to
   *       USE INDEX (i1)
   *    and means "consider only the index i1"
   *
   * It is OK to have the same index several times, e.g. "USE INDEX (i1,i1)" is
   * not an error.
   *
   * Different kind of hints (USE/FORCE/IGNORE) are processed in the following
   * order:
   *    1. All indexes in USE (or FORCE) INDEX are added to the mask.
   *    2. All IGNORE INDEX
   *       e.g. "USE INDEX i1, IGNORE INDEX i1, USE INDEX i1" will not use i1 at all
   *       as if we had "USE INDEX i1, USE INDEX i1, IGNORE INDEX i1".
   *       As an optimization if there is a covering index, and we have
   *       IGNORE INDEX FOR GROUP/order_st, and this index is used for the JOIN part,
   *       then we have to ignore the IGNORE INDEX FROM GROUP/order_st.
   *
   * @retval
   *   false no errors found
   * @retval
   *   true found and reported an error.
   */
  bool process_index_hints(Table *table);

  friend std::ostream& operator<<(std::ostream& output, const TableList &list)
  {
    output << "TableList:(";
    output << list.db;
    output << ", ";
    output << list.table_name;
    output << ", ";
    output << list.alias;
    output << ", ";
    output << "is_natural_join:" << list.is_natural_join;
    output << ", ";
    output << "is_join_columns_complete:" << list.is_join_columns_complete;
    output << ", ";
    output << "straight:" << list.straight;
    output << ", ";
    output << "force_index" << list.force_index;
    output << ", ";
    output << "ignore_leaves:" << list.ignore_leaves;
    output << ", ";
    output << "create:" << list.create;
    output << ", ";
    output << "outer_join:" << list.outer_join;
    output << ", ";
    output << "nested_join:" << list.nested_join;
    output << ")";

    return output;  // for multiple << operators.
  }

  void setIsAlias(bool in_is_alias)
  {
    is_alias= in_is_alias;
  }

  void setIsFqtn(bool in_is_fqtn)
  {
    is_fqtn= in_is_fqtn;
  }

  void setCreate(bool in_create)
  {
    create= in_create;
  }

  void setInternalTmpTable(bool in_internal_tmp_table)
  {
    internal_tmp_table= in_internal_tmp_table;
  }

  void setDbType(plugin::StorageEngine *in_db_type)
  {
    db_type= in_db_type;
  }

  void setJoinList(List<TableList> *in_join_list)
  {
    join_list= in_join_list;
  }

  void setEmbedding(TableList *in_embedding)
  {
    embedding= in_embedding;
  }

  void setNestedJoin(NestedJoin *in_nested_join)
  {
    nested_join= in_nested_join;
  }

  void setDepTables(table_map in_dep_tables)
  {
    dep_tables= in_dep_tables;
  }

  void setOnExprDepTables(table_map in_on_expr_dep_tables)
  {
    on_expr_dep_tables= in_on_expr_dep_tables;
  }

  bool getIsAlias() const
  {
    return is_alias;
  }

  bool getIsFqtn() const
  {
    return is_fqtn;
  }

  bool isCreate() const
  {
    return create;
  }

  bool getInternalTmpTable() const
  {
    return internal_tmp_table;
  }

  plugin::StorageEngine *getDbType() const
  {
    return db_type;
  }

  TableList *getEmbedding() const
  {
    return embedding;
  }

  List<TableList> *getJoinList() const
  {
    return join_list;
  }

  NestedJoin *getNestedJoin() const
  {
    return nested_join;
  }

  table_map getDepTables() const
  {
    return dep_tables;
  }

  table_map getOnExprDepTables() const
  {
    return on_expr_dep_tables;
  }

  void unlock_table_name();
  void unlock_table_names(TableList *last_table= NULL);

private:
  table_map dep_tables; ///< tables the table depends on
  table_map on_expr_dep_tables; ///< tables on expression depends on
  NestedJoin *nested_join; ///< if the element is a nested join
  TableList *embedding; ///< nested join containing the table
  List<TableList> *join_list; ///< join list the table belongs to
  plugin::StorageEngine *db_type; ///< table_type for handler
  bool internal_tmp_table;

  /** true if an alias for this table was specified in the SQL. */
  bool is_alias;

  /** 
   * true if the table is referred to in the statement using a fully
   * qualified name (<db_name>.<table_name>).
   */
  bool is_fqtn;

  /**
   * This TableList object corresponds to the table to be created
   * so it is possible that it does not exist (used in CREATE TABLE
   * ... SELECT implementation).
   */
  bool create;

};

void close_thread_tables(Session *session);

} /* namespace drizzled */

