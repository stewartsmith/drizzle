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

#include <drizzled/function/time/dayname.h>
#include <drizzled/session.h>
#include <drizzled/typelib.h>
#include <drizzled/system_variables.h>

namespace drizzled {

String* Item_func_dayname::val_str(String* str)
{
  assert(fixed == 1);
  uint32_t weekday=(uint) val_int();            // Always Item_func_daynr()
  const char *day_name;

  if (null_value)
    return (String*) 0;

  day_name= getSession().variables.lc_time_names->day_names->type_names[weekday];
  str->set(day_name, strlen(day_name), system_charset_info);
  return str;
}

} /* namespace drizzled */
