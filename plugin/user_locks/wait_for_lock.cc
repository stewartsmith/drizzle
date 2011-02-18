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

#include <string>

namespace user_locks {
namespace locks {

int64_t WaitFor::val_int()
{
  drizzled::String *res= args[0]->val_str(&value);

  if (not res || not res->length())
  {
    my_error(drizzled::ER_USER_LOCKS_INVALID_NAME_LOCK, MYF(0));
    return 0;
  }

  null_value= false;

  drizzled::session_id_t id= getSession().getSessionId();
  bool found= false;

  while (not found)
  {
    found= user_locks::Locks::getInstance().isUsed(Key(*getSession().user(), res->c_str()), id);
    if (not found)
    {
      boost::this_thread::restore_interruption dl(getSession().getThreadInterupt());
      try {
        user_locks::Locks::getInstance().waitCreate();
      }
      catch(boost::thread_interrupted const& error)
      {
        my_error(drizzled::ER_QUERY_INTERRUPTED, MYF(0));
        null_value= true;
        return 0;
      }
    }
    else
    {
      if (id == getSession().getSessionId())
      {
        my_error(drizzled::ER_USER_LOCKS_CANT_WAIT_ON_OWN_LOCK, MYF(0));
        null_value= true;
        return 0;
      }
    }
  }

  return id;
}

} /* namespace locks */
} /* namespace user_locks */
