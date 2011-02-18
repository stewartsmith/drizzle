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

#include <drizzled/session.h>
#include <plugin/utility_functions/functions.h>

namespace drizzled
{

namespace utility_functions
{

int64_t GlobalReadLock::val_int()
{
  assert(fixed == 1);
  null_value= false;

  if (getSession().isGlobalReadLock())
    return 1;

  return 0;
}

String *GlobalReadLock::val_str(String *str)
{
  assert(fixed == 1);
  null_value= false;
  const std::string &global_state_for_session= display::type(getSession().isGlobalReadLock());
  str->copy(global_state_for_session.c_str(), global_state_for_session.length(), system_charset_info);

  return str;
}

void GlobalReadLock::fix_length_and_dec()
{
  max_length= drizzled::display::max_string_length(getSession().isGlobalReadLock()) * system_charset_info->mbmaxlen;
}

} /* namespace utility_functions */
} /* namespace drizzled */
