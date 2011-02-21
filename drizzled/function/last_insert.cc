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

#include <drizzled/function/last_insert.h>
#include <drizzled/session.h>

namespace drizzled
{

int64_t Item_func_last_insert_id::val_int()
{
  assert(fixed == 1);
  if (arg_count)
  {
    int64_t value= args[0]->val_int();
    null_value= args[0]->null_value;
    /*
      LAST_INSERT_ID(X) must affect the client's insert_id() as
      documented in the manual. We don't want to touch
      first_successful_insert_id_in_cur_stmt because it would make
      LAST_INSERT_ID(X) take precedence over an generated auto_increment
      value for this row.
    */
    getSession().arg_of_last_insert_id_function= true;
    getSession().first_successful_insert_id_in_prev_stmt= value;

    return value;
  }
  return getSession().read_first_successful_insert_id_in_prev_stmt();
}


bool Item_func_last_insert_id::fix_fields(Session *session, Item **ref)
{
  return Item_int_func::fix_fields(session, ref);
}

} /* namespace drizzled */
