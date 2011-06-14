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

#include <drizzled/item.h>

namespace drizzled {

/**
 * Instances of Name_resolution_context store the information necesary for
 * name resolution of Items and other context analysis of a query made in
 * fix_fields().
 *
 * This structure is a part of Select_Lex, a pointer to this structure is
 * assigned when an item is created (which happens mostly during  parsing
 * (sql_yacc.yy)), but the structure itself will be initialized after parsing
 * is complete
 *
 * @todo
 *
 * Move subquery of INSERT ... SELECT and CREATE ... SELECT to
 * separate Select_Lex which allow to remove tricks of changing this
 * structure before and after INSERT/CREATE and its SELECT to make correct
 * field name resolution.
 */
class Name_resolution_context: public memory::SqlAlloc
{
public:
  /**
   * The name resolution context to search in when an Item cannot be
   * resolved in this context (the context of an outer select)
   */
  Name_resolution_context *outer_context;

  /**
   * List of tables used to resolve the items of this context.  Usually these
   * are tables from the FROM clause of SELECT statement.  The exceptions are
   * INSERT ... SELECT and CREATE ... SELECT statements, where SELECT
   * subquery is not moved to a separate Select_Lex.  For these types of
   * statements we have to change this member dynamically to ensure correct
   * name resolution of different parts of the statement.
   */
  TableList *table_list;
  /**
   * In most cases the two table references below replace 'table_list' above
   * for the purpose of name resolution. The first and last name resolution
   * table references allow us to search only in a sub-tree of the nested
   * join tree in a FROM clause. This is needed for NATURAL JOIN, JOIN ... USING
   * and JOIN ... ON.
   */
  TableList *first_name_resolution_table;
  /**
   * Last table to search in the list of leaf table references that begins
   * with first_name_resolution_table.
   */
  TableList *last_name_resolution_table;

  /**
   * Select_Lex item belong to, in case of merged VIEW it can differ from
   * Select_Lex where item was created, so we can't use table_list/field_list
   * from there
   */
  Select_Lex *select_lex;

  /**
   * Processor of errors caused during Item name resolving, now used only to
   * hide underlying tables in errors about views (i.e. it substitute some
   * errors for views)
   */
  void (*error_processor)(Session *, void *);
  void *error_processor_data;

  /**
   * When true items are resolved in this context both against the
   * SELECT list and this->table_list. If false, items are resolved
   * only against this->table_list.
   */
  bool resolve_in_select_list;

  /**
   * Security context of this name resolution context. It's used for views
   * and is non-zero only if the view is defined with SQL SECURITY DEFINER.
   */
  SecurityContext *security_ctx;

  Name_resolution_context()
    :
      outer_context(0), 
      table_list(0), 
      select_lex(0),
      error_processor_data(0),
      security_ctx(0)
    {}

  inline void init()
  {
    resolve_in_select_list= false;
    error_processor= &dummy_error_processor;
    first_name_resolution_table= NULL;
    last_name_resolution_table= NULL;
  }

  inline void resolve_in_table_list_only(TableList *tables)
  {
    table_list= first_name_resolution_table= tables;
    resolve_in_select_list= false;
  }

  inline void process_error(Session *session)
  {
    (*error_processor)(session, error_processor_data);
  }
};

} /* namespace drizzled */

