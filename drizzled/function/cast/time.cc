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

#include "config.h"

#include "drizzled/function/cast/time.h"
#include "drizzled/time_functions.h"

namespace drizzled {
namespace function {
namespace cast {

bool Time::get_date(DRIZZLE_TIME *ltime, uint32_t )
{
  bool res= get_arg0_date(ltime, TIME_FUZZY_DATE);
  ltime->hour= ltime->minute= ltime->second= ltime->second_part= 0;
  ltime->time_type= DRIZZLE_TIMESTAMP_DATE;
  return res;
}


bool Time::get_time(DRIZZLE_TIME *ltime)
{
  memset(ltime, 0, sizeof(DRIZZLE_TIME));
  return args[0]->null_value;
}

String *Time::val_str(String *str)
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;

  if (not get_arg0_date(&ltime, TIME_FUZZY_DATE) && not str->alloc(MAX_DATE_STRING_REP_LENGTH))
  {
    make_time(&ltime, str);
    return str;
  }

  null_value= 1;
  return 0;
}

int64_t Time::val_int()
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;

  if ((null_value= args[0]->get_time(&ltime)))
    return 0;

  return TIME_to_uint64_t(&ltime);
}

} // namespace cast
} // namespace function
} // namespace drizzled
