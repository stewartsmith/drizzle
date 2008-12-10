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
#include <drizzled/error.h>
#include <drizzled/current_session.h>
#include <drizzled/functions/signed.h>

void Item_func_signed::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as signed)"));

}

int64_t Item_func_signed::val_int_from_str(int *error)
{
  char buff[MAX_FIELD_WIDTH], *end, *start;
  uint32_t length;
  String tmp(buff,sizeof(buff), &my_charset_bin), *res;
  int64_t value;

  /*
    For a string result, we must first get the string and then convert it
    to a int64_t
  */

  if (!(res= args[0]->val_str(&tmp)))
  {
    null_value= 1;
    *error= 0;
    return 0;
  }
  null_value= 0;
  start= (char *)res->ptr();
  length= res->length();

  end= start + length;
  value= my_strtoll10(start, &end, error);
  if (*error > 0 || end != start+ length)
  {
    char err_buff[128];
    String err_tmp(err_buff,(uint32_t) sizeof(err_buff), system_charset_info);
    err_tmp.copy(start, length, system_charset_info);
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "INTEGER",
                        err_tmp.c_ptr());
  }
  return value;
}


int64_t Item_func_signed::val_int()
{
  int64_t value;
  int error;

  if (args[0]->cast_to_int_type() != STRING_RESULT ||
      args[0]->result_as_int64_t())
  {
    value= args[0]->val_int();
    null_value= args[0]->null_value;
    return value;
  }

  value= val_int_from_str(&error);
  if (value < 0 && error == 0)
  {
    push_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                 "Cast to signed converted positive out-of-range integer to "
                 "it's negative complement");
  }
  return value;
}
