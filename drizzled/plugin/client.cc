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

#include "config.h"
#include <cstdio>
#include "drizzled/plugin/client.h"

namespace drizzled
{

bool plugin::Client::store(const DRIZZLE_TIME *from)
{
  const size_t buff_len= 40;
  char buff[buff_len];
  uint32_t length= 0;
  uint32_t day;

  switch (from->time_type)
  {
  case DRIZZLE_TIMESTAMP_DATETIME:
    length= snprintf(buff, (buff_len-length), "%04d-%02d-%02d %02d:%02d:%02d",
                    (int) from->year,
                    (int) from->month,
                    (int) from->day,
                    (int) from->hour,
                    (int) from->minute,
                    (int) from->second);
    if (from->second_part)
      length+= snprintf(buff+length, (buff_len-length), ".%06d", (int)from->second_part);
    break;

  case DRIZZLE_TIMESTAMP_DATE:
    length= snprintf(buff, (buff_len-length), "%04d-%02d-%02d",
                    (int) from->year,
                    (int) from->month,
                    (int) from->day);
    break;

  case DRIZZLE_TIMESTAMP_TIME:
    day= (from->year || from->month) ? 0 : from->day;
    length= snprintf(buff, (buff_len-length), "%s%02ld:%02d:%02d",
                    from->neg ? "-" : "",
                    (long) day*24L+(long) from->hour,
                    (int) from->minute,
                    (int) from->second);
    if (from->second_part)
      length+= snprintf(buff+length, (buff_len-length), ".%06d", (int)from->second_part);
    break;

  case DRIZZLE_TIMESTAMP_NONE:
  case DRIZZLE_TIMESTAMP_ERROR:
  default:
    assert(0);
    return false;
  }

  return store(buff);
}

bool plugin::Client::store(const char *from)
{
  if (from == NULL)
    return store();
  return store(from, strlen(from));
}


} /* namespace drizzled */
