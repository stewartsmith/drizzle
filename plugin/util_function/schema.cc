/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include "config.h"

#include <drizzled/session.h>
#include <drizzled/function/str/strfunc.h>
#include "plugin/util_function/functions.h"

namespace drizzled
{

namespace util_function
{

String *Schema::val_str(String *str)
{
  assert(fixed == 1);
  Session *session= current_session;
  if (session->db.empty())
  {
    null_value= 1;
    return 0;
  }
  else
  {
    str->copy(session->db.c_str(), session->db.length(), system_charset_info);
  }
  return str;
}

} /* namespace util_function */
} /* namespace drizzled */
