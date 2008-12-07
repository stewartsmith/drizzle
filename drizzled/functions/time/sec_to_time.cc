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
#include <drizzled/functions/time/sec_to_time.h>

/*
  Convert seconds to DRIZZLE_TIME value with overflow checking

  SYNOPSIS:
    sec_to_time()
    seconds          number of seconds
    unsigned_flag    1, if 'seconds' is unsigned, 0, otherwise
    ltime            output DRIZZLE_TIME value

  DESCRIPTION
    If the 'seconds' argument is inside DRIZZLE_TIME data range, convert it to a
    corresponding value.
    Otherwise, truncate the resulting value to the nearest endpoint, and
    produce a warning message.

  RETURN
    1                if the value was truncated during conversion
    0                otherwise
*/

static bool sec_to_time(int64_t seconds, bool unsigned_flag, DRIZZLE_TIME *ltime)
{
  uint32_t sec;

  memset(ltime, 0, sizeof(*ltime));

  if (seconds < 0)
  {
    ltime->neg= 1;
    if (seconds < -3020399)
      goto overflow;
    seconds= -seconds;
  }
  else if (seconds > 3020399)
    goto overflow;

  sec= (uint) ((uint64_t) seconds % 3600);
  ltime->hour= (uint) (seconds/3600);
  ltime->minute= sec/60;
  ltime->second= sec % 60;

  return 0;

overflow:
  ltime->hour= TIME_MAX_HOUR;
  ltime->minute= TIME_MAX_MINUTE;
  ltime->second= TIME_MAX_SECOND;

  char buf[22];
  int len= (int)(int64_t10_to_str(seconds, buf, unsigned_flag ? 10 : -10)
                 - buf);
  make_truncated_value_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                               buf, len, DRIZZLE_TIMESTAMP_TIME,
                               NULL);

  return 1;
}

String *Item_func_sec_to_time::val_str(String *str)
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  int64_t arg_val= args[0]->val_int();

  if ((null_value=args[0]->null_value) ||
      str->alloc(MAX_DATE_STRING_REP_LENGTH))
  {
    null_value= 1;
    return (String*) 0;
  }

  sec_to_time(arg_val, args[0]->unsigned_flag, &ltime);

  make_time((DATE_TIME_FORMAT *) 0, &ltime, str);
  return str;
}

int64_t Item_func_sec_to_time::val_int()
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;
  int64_t arg_val= args[0]->val_int();

  if ((null_value=args[0]->null_value))
    return 0;

  sec_to_time(arg_val, args[0]->unsigned_flag, &ltime);

  return (ltime.neg ? -1 : 1) *
    ((ltime.hour)*10000 + ltime.minute*100 + ltime.second);
}


