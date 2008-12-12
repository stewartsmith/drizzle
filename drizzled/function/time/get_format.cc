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
#include <drizzled/function/time/get_format.h>

String *Item_func_get_format::val_str(String *str)
{
  assert(fixed == 1);
  const char *format_name;
  KNOWN_DATE_TIME_FORMAT *format;
  String *val= args[0]->val_str(str);
  ulong val_len;

  if ((null_value= args[0]->null_value))
    return 0;

  val_len= val->length();
  for (format= &known_date_time_formats[0];
       (format_name= format->format_name);
       format++)
  {
    uint32_t format_name_len;
    format_name_len= strlen(format_name);
    if (val_len == format_name_len &&
        !my_strnncoll(&my_charset_utf8_general_ci,
                      (const unsigned char *) val->ptr(), val_len,
                      (const unsigned char *) format_name, val_len))
    {
      const char *format_str= get_date_time_format_str(format, type);
      str->set(format_str, strlen(format_str), &my_charset_bin);
      return str;
    }
  }

  null_value= 1;
  return 0;
}

void Item_func_get_format::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  str->append('(');

  switch (type) {
  case DRIZZLE_TIMESTAMP_DATE:
    str->append(STRING_WITH_LEN("DATE, "));
    break;
  case DRIZZLE_TIMESTAMP_DATETIME:
    str->append(STRING_WITH_LEN("DATETIME, "));
    break;
  case DRIZZLE_TIMESTAMP_TIME:
    str->append(STRING_WITH_LEN("TIME, "));
    break;
  default:
    assert(0);
  }
  args[0]->print(str, query_type);
  str->append(')');
}


