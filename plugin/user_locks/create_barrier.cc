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
#include <plugin/user_locks/module.h>
#include <plugin/user_locks/barrier_storage.h>

#include <string>

namespace user_locks {
namespace barriers {

int64_t CreateBarrier::val_int()
{
  drizzled::String *res= args[0]->val_str(&value);

  if (not res || not res->length())
  {
    my_error(drizzled::ER_USER_LOCKS_INVALID_NAME_BARRIER, MYF(0));
    return 0;
  }
  null_value= false;

  drizzled::identifier::user::ptr user_identifier(getSession().user());
  bool result= arg_count == 2
    ? Barriers::getInstance().create(Key(*user_identifier, res->c_str()), getSession().getSessionId(), args[1]->val_int())
    : Barriers::getInstance().create(Key(*user_identifier, res->c_str()), getSession().getSessionId());
  if (not result)
    return 0;
  barriers::Storable *list= getSession().getProperty<barriers::Storable>(barriers::property_key);
  if (not list)
    list= getSession().setProperty(barriers::property_key, new barriers::Storable(getSession().getSessionId()));
  list->insert(Key(*user_identifier, res->c_str()));
  return 1;
}

} /* namespace barriers */
} /* namespace user_locks */
