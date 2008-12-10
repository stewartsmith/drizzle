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
#include <drizzled/functions/time/maketime.h>
#include <drizzled/functions/time/make_time_with_warn.h>

/**
  MAKETIME(h,m,s) is a time function that calculates a time value
  from the total number of hours, minutes, and seconds.
  Result: Time value
*/

String *Item_func_maketime::val_str(String *str)
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  bool overflow= 0;

  int64_t hour=   args[0]->val_int();
  int64_t minute= args[1]->val_int();
  int64_t second= args[2]->val_int();

  if ((null_value=(args[0]->null_value ||
                   args[1]->null_value ||
                   args[2]->null_value ||
                   minute < 0 || minute > 59 ||
                   second < 0 || second > 59 ||
                   str->alloc(MAX_DATE_STRING_REP_LENGTH))))
    return 0;

  memset(&ltime, 0, sizeof(ltime));
  ltime.neg= 0;

  /* Check for integer overflows */
  if (hour < 0)
    ltime.neg= 1;

  if (-hour > UINT_MAX || hour > UINT_MAX)
    overflow= 1;

  if (!overflow)
  {
    ltime.hour=   (uint) ((hour < 0 ? -hour : hour));
    ltime.minute= (uint) minute;
    ltime.second= (uint) second;
  }
  else
  {
    ltime.hour= TIME_MAX_HOUR;
    ltime.minute= TIME_MAX_MINUTE;
    ltime.second= TIME_MAX_SECOND;
    char buf[28];
    char *ptr= int64_t10_to_str(hour, buf, args[0]->unsigned_flag ? 10 : -10);
    int len = (int)(ptr - buf) +
      sprintf(ptr, ":%02u:%02u", (uint)minute, (uint)second);
    make_truncated_value_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 buf, len, DRIZZLE_TIMESTAMP_TIME,
                                 NULL);
  }

  if (make_time_with_warn((DATE_TIME_FORMAT *) 0, &ltime, str))
  {
    null_value= 1;
    return 0;
  }
  return str;
}

