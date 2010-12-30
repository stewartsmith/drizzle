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

#include "config.h"

#include "plugin/utility_functions/functions.h"
#include <drizzled/error.h>

namespace drizzled
{

namespace utility_functions
{

bool Assert::val_bool()
{
  if (not args[0]->val_bool())
  {
    drizzled::String _res;
    drizzled::String *res;

    if (args[0]->is_null())
    {
      drizzled::my_error(ER_ASSERT_NULL, MYF(0));

      return false;
    }

    res= args[0]->val_str(&_res);
    drizzled::my_error(ER_ASSERT, MYF(0), res->c_str());

    return false;
  }

  return true;
}

} /* namespace utility_functions */
} /* namespace drizzled */
