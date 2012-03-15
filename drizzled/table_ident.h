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

#include <drizzled/lex_string.h>
#include <drizzled/memory/sql_alloc.h>
#include <drizzled/util/test.h>

namespace drizzled {

/* Structure for db & table in sql_yacc */
class Table_ident : public memory::SqlAlloc
{
public:
  lex_string_t db;
  str_ref table;
  Select_Lex_Unit *sel;

  Table_ident(lex_string_t db_arg, str_ref table_arg)
    : db(db_arg), table(table_arg), sel(NULL)
  {
  }

  explicit Table_ident(lex_string_t table_arg)
    : table(table_arg), sel(NULL)
  {
    db.assign(static_cast<const char*>(NULL), 0);
  }

  /*
    This constructor is used only for the case when we create a derived
    table. A derived table has no name and doesn't belong to any database.
    Later, if there was an alias specified for the table, it will be set
    by add_table_to_list.
  */
  explicit Table_ident(Select_Lex_Unit *s) : 
    table("*"), sel(s)
  {
    /* We must have a table name here as this is used with add_table_to_list */
    db.assign("", 0); // a subject to casedn_str
  }
  bool is_derived_table() const { return test(sel); }
};

} /* namespace drizzled */

