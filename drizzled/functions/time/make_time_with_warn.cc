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
#include <drizzled/current_session.h>
#include CSTDINT_H
#include <drizzled/functions/time/make_time_with_warn.h>

/*
  Wrapper over make_time() with validation of the input DRIZZLE_TIME value

  NOTE
    see make_time() for more info

  RETURN
    1    if there was an error during conversion
    0    otherwise
*/

bool make_time_with_warn(const DATE_TIME_FORMAT *format,
                                DRIZZLE_TIME *l_time, String *str)
{
  int warning= 0;
  make_time(format, l_time, str);
  if (check_time_range(l_time, &warning))
    return 1;
  if (warning)
  {
    make_truncated_value_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 str->ptr(), str->length(),
                                 DRIZZLE_TIMESTAMP_TIME, NULL);
    make_time(format, l_time, str);
  }

  return 0;
}


