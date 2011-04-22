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

#include <drizzled/function/time/date.h>
#include <drizzled/temporal.h>

namespace drizzled
{

String *Item_date::val_str(String *str)
{
  assert(fixed);

  /* We have our subclass convert a Date temporal for us */
  Date temporal;
  if (! get_temporal(temporal))
    return (String *) NULL; /* get_temporal throws error. */

  str->alloc(type::Time::MAX_STRING_LENGTH);

  /* Convert the Date to a string and return it */
  size_t new_length;
  new_length= temporal.to_string(str->c_ptr(), type::Time::MAX_STRING_LENGTH);
  assert(new_length < type::Time::MAX_STRING_LENGTH);
  str->length(new_length);
  return str;
}

int64_t Item_date::val_int()
{
  assert(fixed);

  /* We have our subclass convert a Date temporal for us */
  Date temporal;
  if (! get_temporal(temporal))
    return 0; /* get_temporal throws error. */

  /* Convert the Date to a string and return it */
  int32_t int_value;
  temporal.to_int32_t(&int_value);
  return (int64_t) int_value;
}

} /* namespace drizzled */
