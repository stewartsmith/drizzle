/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <drizzled/function/cast/time.h>
#include <drizzled/time_functions.h>

namespace drizzled {
namespace function {
namespace cast {

bool Time::get_time(type::Time &ltime)
{
  bool res= get_arg0_time(ltime);

  ltime.truncate(type::DRIZZLE_TIMESTAMP_TIME);

  return res;
}

String *Time::val_str(String *str)
{
  assert(fixed == 1);
  type::Time ltime;

  if (not get_arg0_time(ltime))
  {
    null_value= 0;
    ltime.convert(*str, type::DRIZZLE_TIMESTAMP_TIME);

    return str;
  }

  null_value= 1;
  return 0;
}

int64_t Time::val_int()
{
  assert(fixed == 1);
  type::Time ltime;

  if (get_time(ltime))
    return 0;

  return (ltime.neg ? -1 : 1) * (int64_t)((ltime.hour)*10000 + ltime.minute*100 + ltime.second);
}

} // namespace cast
} // namespace function
} // namespace drizzled
