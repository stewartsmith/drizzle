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
#include <drizzled/functions/time/make_datetime.h>

/**
  @todo
  OPTIMIZATION
  - Replace the switch with a function that should be called for each
  date type.
  - Remove sprintf and opencode the conversion, like we do in
  Field_datetime.

  The reason for this functions existence is that as we don't have a
  way to know if a datetime/time value has microseconds in them
  we are now only adding microseconds to the output if the
  value has microseconds.

  We can't use a standard make_date_time() for this as we don't know
  if someone will use %f in the format specifier in which case we would get
  the microseconds twice.
*/

bool make_datetime(date_time_format_types format, DRIZZLE_TIME *ltime,
                          String *str)
{
  char *buff;
  const CHARSET_INFO * const cs= &my_charset_bin;
  uint32_t length= MAX_DATE_STRING_REP_LENGTH;

  if (str->alloc(length))
    return 1;
  buff= (char*) str->ptr();

  switch (format) {
  case TIME_ONLY:
    length= cs->cset->snprintf(cs, buff, length, "%s%02d:%02d:%02d",
                               ltime->neg ? "-" : "",
                               ltime->hour, ltime->minute, ltime->second);
    break;
  case TIME_MICROSECOND:
    length= cs->cset->snprintf(cs, buff, length, "%s%02d:%02d:%02d.%06ld",
                               ltime->neg ? "-" : "",
                               ltime->hour, ltime->minute, ltime->second,
                               ltime->second_part);
    break;
  case DATE_ONLY:
    length= cs->cset->snprintf(cs, buff, length, "%04d-%02d-%02d",
                               ltime->year, ltime->month, ltime->day);
    break;
  case DATE_TIME:
    length= cs->cset->snprintf(cs, buff, length,
                               "%04d-%02d-%02d %02d:%02d:%02d",
                               ltime->year, ltime->month, ltime->day,
                               ltime->hour, ltime->minute, ltime->second);
    break;
  case DATE_TIME_MICROSECOND:
    length= cs->cset->snprintf(cs, buff, length,
                               "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
                               ltime->year, ltime->month, ltime->day,
                               ltime->hour, ltime->minute, ltime->second,
                               ltime->second_part);
    break;
  }

  str->length(length);
  str->set_charset(cs);
  return 0;
}

