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

int64_t Wait::val_int()
{
  drizzled::String *res= args[0]->val_str(&value);

  if (res and res->length())
  {
    Barrier::shared_ptr barrier= Barriers::getInstance().find(Key(*getSession().user(), res->c_str()));

    if (barrier and barrier->getOwner() == getSession().getSessionId())
    {
      my_error(drizzled::ER_USER_LOCKS_CANT_WAIT_ON_OWN_BARRIER, MYF(0));

      return 0;
    }
    else if (barrier)
    {
      if (arg_count == 2)
      {
        int64_t generation;
        generation= args[1]->val_int();
        barrier->wait(generation);
      }
      else
      {
        boost::this_thread::restore_interruption dl(getSession().getThreadInterupt());

        try {
          barrier->wait();
        }
        catch(boost::thread_interrupted const&)
        {
          // We need to issue a different sort of error here
          my_error(drizzled::ER_QUERY_INTERRUPTED, MYF(0));
          return 0;
        }
      }
      null_value= false;

      return 1;
    }
  }
  else if (not res || not res->length())
  {
    my_error(drizzled::ER_USER_LOCKS_INVALID_NAME_BARRIER, MYF(0));
    return 0;
  }

  my_error(drizzled::ER_USER_LOCKS_UNKNOWN_BARRIER, MYF(0));

  return 0;
}

} /* namespace barriers */
} /* namespace user_locks */
