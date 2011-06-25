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

/* Functions to create an item. Used by sql/sql_yacc.yy */

#pragma once

#include <drizzled/item/func.h>
#include <drizzled/plugin/function.h>

namespace drizzled
{

/**
  Public function builder interface.
  The parser (sql/sql_yacc.yy) uses a factory / builder pattern to
  construct an <code>Item</code> object for each function call.
  All the concrete function builders implements this interface,
  either directly or indirectly with some adapter helpers.
  Keeping the function creation separated from the bison grammar allows
  to simplify the parser, and avoid the need to introduce a new token
  for each function, which has undesirable side effects in the grammar.
*/

class Create_func
{
public:
  /**
    The builder create method.
    Given the function name and list or arguments, this method creates
    an <code>Item</code> that represents the function call.
    In case or errors, a NULL item is returned, and an error is reported.
    Note that the <code>session</code> object may be modified by the builder.
    In particular, the following members/methods can be set/called,
    depending on the function called and the function possible side effects.
    <ul>
      <li><code>session->lex().binlog_row_based_if_mixed</code></li>
      <li><code>session->lex().current_context()</code></li>
      <li><code>session->lex().safe_to_cache_query</code></li>
      <li><code>session->lex().uncacheable(UNCACHEABLE_SIDEEFFECT)</code></li>
      <li><code>session->lex().uncacheable(UNCACHEABLE_RAND)</code></li>
      <li><code>session->lex().add_time_zone_tables_to_query_tables(session)</code></li>
    </ul>
    @param session The current thread
    @param name The function name
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call, or NULL
  */
  virtual Item *create(Session *session, LEX_STRING name, List<Item> *item_list) = 0;

protected:
  /** Constructor */
  Create_func() {}
  /** Destructor */
  virtual ~Create_func() {}
};

/**
  Function builder for qualified functions.
  This builder is used with functions call using a qualified function name
  syntax, as in <code>db.func(expr, expr, ...)</code>.
*/

class Create_qfunc : public Create_func
{
public:
  /**
    The builder create method, for unqualified functions.
    This builder will use the current database for the database name.
    @param session The current thread
    @param name The function name
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call
  */
  virtual Item *create(Session *session, LEX_STRING name, List<Item> *item_list);

  /**
    The builder create method, for qualified functions.
    @param session The current thread
    @param db The database name
    @param name The function name
    @param use_explicit_name Should the function be represented as 'db.name'?
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call
  */
  virtual Item* create(Session *session, LEX_STRING db, LEX_STRING name,
                       bool use_explicit_name, List<Item> *item_list) = 0;

protected:
  /** Constructor. */
  Create_qfunc() {}
  /** Destructor. */
  virtual ~Create_qfunc() {}
};


/**
  Find the native function builder associated with a given function name.
  @param name The native function name
  @return The native function builder associated with the name, or NULL
*/
extern Create_func * find_native_function_builder(LEX_STRING name);


/**
  Find the function builder for qualified functions.
  @param session The current thread
  @return A function builder for qualified functions
*/
extern Create_qfunc * find_qualified_function_builder(Session *session);


/**
  Function builder for User Defined Functions.
*/

class Create_udf_func : public Create_func
{
public:
  virtual Item *create(Session *session, LEX_STRING name, List<Item> *item_list);

  /**
    The builder create method, for User Defined Functions.
    @param session The current thread
    @param fct The User Defined Function metadata
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call
  */
  Item *create(Session *session,
               const plugin::Function *fct,
               List<Item> *item_list);

  /** Singleton. */
  static Create_udf_func s_singleton;

protected:
  /** Constructor. */
  Create_udf_func() {}
  /** Destructor. */
  virtual ~Create_udf_func() {}
};

Item*
create_func_char_cast(Session *session, Item *a, int len, const charset_info_st * const cs);

/**
  Builder for cast expressions.
  @param session The current thread
  @param a The item to cast
  @param cast_type the type casted into
  @param len TODO
  @param dec TODO
  @param cs The character set
*/
Item *
create_func_cast(Session *session, Item *a, Cast_target cast_type,
                 const char *len, const char *dec,
                 const charset_info_st * const cs);

void item_create_init();
void item_create_cleanup();

} /* namespace drizzled */

