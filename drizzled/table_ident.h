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

extern char empty_c_string[1];
extern char internal_table_name[2];

/* Structure for db & table in sql_yacc */
class Table_ident :public memory::SqlAlloc
{
public:
  LEX_STRING db;
  LEX_STRING table;
  Select_Lex_Unit *sel;
  inline Table_ident(LEX_STRING db_arg, LEX_STRING table_arg)
    :table(table_arg), sel((Select_Lex_Unit *)0)
  {
    db= db_arg;
  }
  explicit Table_ident(LEX_STRING table_arg)
    :table(table_arg), sel((Select_Lex_Unit *)0)
  {
    db.str=0;
  }
  /*
    This constructor is used only for the case when we create a derived
    table. A derived table has no name and doesn't belong to any database.
    Later, if there was an alias specified for the table, it will be set
    by add_table_to_list.
  */
  explicit Table_ident(Select_Lex_Unit *s) : sel(s)
  {
    /* We must have a table name here as this is used with add_table_to_list */
    db.str= empty_c_string;                    /* a subject to casedn_str */
    db.length= 0;
    table.str= internal_table_name;
    table.length=1;
  }
  bool is_derived_table() const { return test(sel); }
  inline void change_db(char *db_name)
  {
    db.str= db_name; db.length= (uint32_t) strlen(db_name);
  }
};

} /* namespace drizzled */

