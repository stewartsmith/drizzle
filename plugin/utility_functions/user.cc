/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Stewart Smith
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

#include <drizzled/session.h>
#include <plugin/utility_functions/functions.h>

namespace drizzled
{

namespace utility_functions
{


String *User::val_str(String *str)
{
  assert(fixed == 1);
  identifier::user::ptr user= getSession().user();
  assert(user);
  if (user->username().empty())
  {
    null_value= 1;
    return 0;
  }
  else
  {
    str->copy(user->username().c_str(), user->username().length(), system_charset_info);
  }
  return str;
}

} /* namespace utility_functions */
} /* namespace drizzled */
