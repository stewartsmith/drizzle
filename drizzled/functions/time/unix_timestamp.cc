/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/functions/time/unix_timestamp.h>
#include <drizzled/session.h>

int64_t Item_func_unix_timestamp::val_int()
{
  DRIZZLE_TIME ltime;
  bool not_used;
  
  assert(fixed == 1);
  if (arg_count == 0)
    return (int64_t) current_session->query_start();
  if (args[0]->type() == FIELD_ITEM)
  {                                             // Optimize timestamp field
    Field *field=((Item_field*) args[0])->field;
    if (field->type() == DRIZZLE_TYPE_TIMESTAMP)
      return ((Field_timestamp*) field)->get_timestamp(&null_value);
  }

  if (get_arg0_date(&ltime, 0)) 
  {
    /*
      We have to set null_value again because get_arg0_date will also set it
      to true if we have wrong datetime parameter (and we should return 0 in
      this case).
    */
    null_value= args[0]->null_value;
    return 0;
  }

  return (int64_t) TIME_to_timestamp(current_session, &ltime, &not_used);
}

