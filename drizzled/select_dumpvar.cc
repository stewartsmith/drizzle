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

#include <config.h>

#include <drizzled/select_dumpvar.h>
#include <drizzled/sql_lex.h>
#include <drizzled/session.h>
#include <drizzled/var.h>

namespace drizzled {

bool select_dumpvar::send_data(List<Item> &items)
{
  std::vector<var *>::const_iterator iter= var_list.begin();

  List<Item>::iterator it(items.begin());
  Item *item= NULL;
  var *current_var;

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  if (row_count++)
  {
    my_message(ER_TOO_MANY_ROWS, ER(ER_TOO_MANY_ROWS), MYF(0));
    return 1;
  }
  while ((iter != var_list.end()) && (item= it++))
  {
    current_var= *iter;
    if (current_var->local == 0)
    {
      Item_func_set_user_var *suv= new Item_func_set_user_var(current_var->s, item);
      suv->fix_fields(session, 0);
      suv->check(0);
      suv->update();
    }
    ++iter;
  }
  return(session->is_error());
}

bool select_dumpvar::send_eof()
{
  if (! row_count)
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
		 ER_SP_FETCH_NO_DATA, ER(ER_SP_FETCH_NO_DATA));
  /*
    In order to remember the value of affected rows for ROW_COUNT()
    function, SELECT INTO has to have an own SQLCOM.
    @TODO split from SQLCOM_SELECT
  */
  session->my_ok(row_count);
  return 0;
}

int select_dumpvar::prepare(List<Item> &list, Select_Lex_Unit *u)
{
  unit= u;

  if (var_list.size() != list.size())
  {
    my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
	       ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT), MYF(0));
    return 1;
  }
  return 0;
}

} // namespace drizzled
