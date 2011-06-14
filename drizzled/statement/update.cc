/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/update.h>
#include <drizzled/sql_lex.h>

namespace drizzled {

bool statement::Update::execute()
{
  TableList *first_table= (TableList *) lex().select_lex.table_list.first;
  TableList *all_tables= lex().query_tables;
  assert(first_table == all_tables && first_table != 0);
  Select_Lex *select_lex= &lex().select_lex;
  Select_Lex_Unit *unit= &lex().unit;
  if (update_precheck(&session(), all_tables))
  {
    return true;
  }
  assert(select_lex->offset_limit == 0);
  unit->set_limit(select_lex);
  bool res= update_query(&session(), 
                         all_tables,
                         select_lex->item_list,
                         lex().value_list,
                         select_lex->where,
                         select_lex->order_list.elements,
                         (Order *) select_lex->order_list.first,
                         unit->select_limit_cnt,
                         lex().duplicates, 
                         lex().ignore);
  return res;
}

} /* namespace drizzled */

