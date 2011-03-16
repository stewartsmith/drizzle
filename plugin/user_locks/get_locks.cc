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
#include <plugin/user_locks/lock_storage.h>

#include <string>

namespace user_locks {

int64_t GetLocks::val_int()
{
  Keys list_of_locks;

  for (int64_t x= 0; x < arg_count; x++)
  {
    drizzled::String *res= args[x]->val_str(&value);

    if (res && res->length())
    {
      list_of_locks.insert(Key(*getSession().user(), res->c_str()));
    }
    else
    {
      my_error(drizzled::ER_USER_LOCKS_INVALID_NAME_LOCK, MYF(0));
      return 0;
    }
  }
  {
    boost::this_thread::restore_interruption dl(getSession().getThreadInterupt());
    try 
    {
      if (not user_locks::Locks::getInstance().lock(getSession().getSessionId(), list_of_locks))
        return 0;
    }
    catch (boost::thread_interrupted const&)
    {
      my_error(drizzled::ER_QUERY_INTERRUPTED, MYF(0));
      null_value= true;
      return 0;
    }
  }

  user_locks::Storable *list= getSession().getProperty<user_locks::Storable>("user_locks");
  if (not list)
    list= getSession().setProperty("user_locks", new user_locks::Storable(getSession().getSessionId()));
  BOOST_FOREACH(Keys::const_reference iter, list_of_locks)
    list->insert(iter);
  null_value= false;
  return 1;
}

} /* namespace user_locks */
