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

#include <drizzled/atomics.h>
#include <drizzled/session.h>

using namespace drizzled;
using namespace std;

user_locks::UserLocks::UserLocks() :
  plugin::TableFunction("DATA_DICTIONARY", "USER_DEFINED_LOCKS")
{
  add_field("USER_LOCK_NAME", plugin::TableFunction::STRING, user_locks::LARGEST_LOCK_NAME, false);
  add_field("SESSION_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("USERNAME", plugin::TableFunction::STRING);
}

user_locks::UserLocks::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg)
{
  user_locks::Locks::getInstance().Copy(lock_map);
  iter= lock_map.begin();
}

bool user_locks::UserLocks::Generator::populate()
{

  while (iter != lock_map.end())
  {
    // USER_LOCK_NAME
    push(iter->first.getLockName());

    // SESSION_ID
    push(iter->second->id);
    //
    // USERNAME
    push(iter->first.getUser());

    iter++;
    return true;
  }

  return false;
}
