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
#include <drizzled/functions/time/timediff.h>
#include <drizzled/item/timefunc.h>

/**
  TIMEDIFF(t,s) is a time function that calculates the
  time value between a start and end time.

  t and s: time_or_datetime_expression
  Result: Time value
*/

String *Item_func_timediff::val_str(String *str)
{
  assert(fixed == 1);
  int64_t seconds;
  long microseconds;
  int l_sign= 1;
  DRIZZLE_TIME l_time1 ,l_time2, l_time3;

  null_value= 0;
  if (args[0]->get_time(&l_time1) ||
      args[1]->get_time(&l_time2) ||
      l_time1.time_type != l_time2.time_type)
    goto null_date;

  if (l_time1.neg != l_time2.neg)
    l_sign= -l_sign;

  memset(&l_time3, 0, sizeof(l_time3));

  l_time3.neg= calc_time_diff(&l_time1, &l_time2, l_sign,
                              &seconds, &microseconds);

  /*
    For DRIZZLE_TIMESTAMP_TIME only:
      If first argument was negative and diff between arguments
      is non-zero we need to swap sign to get proper result.
  */
  if (l_time1.neg && (seconds || microseconds))
    l_time3.neg= 1-l_time3.neg;         // Swap sign of result

  calc_time_from_sec(&l_time3, (long) seconds, microseconds);

  if (!make_datetime_with_warn(l_time1.second_part || l_time2.second_part ?
                               TIME_MICROSECOND : TIME_ONLY,
                               &l_time3, str))
    return str;
null_date:
  null_value=1;
  return 0;
}

