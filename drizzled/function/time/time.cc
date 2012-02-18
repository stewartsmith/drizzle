/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2011 Matthew Rheaume
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hopethat it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#include <drizzled/function/time/time.h>
#include <drizzled/temporal.h>

namespace drizzled
{

String *Item_time::val_str(String *str)
{
  assert(fixed);

  Time temporal;
  if (!get_temporal(temporal))
    return (String *)NULL;

  str->alloc(type::Time::MAX_STRING_LENGTH);

  size_t new_length;
  new_length = temporal.to_string(str->c_ptr(), type::Time::MAX_STRING_LENGTH);
  assert(new_length < type::Time::MAX_STRING_LENGTH);
  str->length(new_length);
  return str;
}

int64_t Item_time::val_int()
{
  assert(fixed);
  Time temporal;
  if (!get_temporal(temporal))
    return 0;

  int32_t int_value;
  temporal.to_int32_t(&int_value);
  return (int64_t) int_value;
}

} /* namespace drizzled */
