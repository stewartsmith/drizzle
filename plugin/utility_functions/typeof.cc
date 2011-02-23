/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <plugin/utility_functions/functions.h>
#include <drizzled/display.h>

namespace drizzled
{

namespace utility_functions
{

int64_t Typeof::val_int()
{
  return static_cast<int64_t>(args[0]->field_type());
}

String *Typeof::val_str(String *str)
{
  assert(fixed == 1);
  null_value= false;

  const std::string &tmp= display::type(args[0]->field_type());
  str->copy(tmp.c_str(), tmp.length(), system_charset_info);

  return str;
}

void Typeof::fix_length_and_dec()
{
  max_length= display::type(args[0]->field_type()).size() * system_charset_info->mbmaxlen;
}

} /* namespace utility_functions */
} /* namespace drizzled */
