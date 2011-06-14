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

#pragma once

#include <drizzled/session.h>
#include <drizzled/util/storable.h>
#include <boost/unordered_set.hpp>

namespace user_locks {
namespace barriers {

/*
  We a storable to track any locks we might have open so that if we are disconnected before we release the locks, we release the locks during the deconstruction of Session.
*/

const std::string property_key("user_barriers");

class Storable : public drizzled::util::Storable {
  Keys list_of_locks;
  drizzled::session_id_t owner;

public:

  Storable(drizzled::session_id_t id_arg) :
    owner(id_arg)
  {
  }

  ~Storable()
  {
    erase_all();
  }

  void insert(const Key &arg)
  {
    list_of_locks.insert(arg);
  }

  bool erase(const Key &arg)
  {
    return boost::lexical_cast<bool>(list_of_locks.erase(arg));
  }

  // An assert() should be added so that we can test to make sure the locks
  // are what we think they are (i.e. test the result of release())
  int64_t erase_all()
  {
    int64_t count= list_of_locks.size();

    for (Keys::iterator iter= list_of_locks.begin();
         iter != list_of_locks.end(); iter++)
    {
      (void)Barriers::getInstance().release(*iter, owner);
    }
    list_of_locks.clear();

    return count;
  }
};


} /* namespace barriers */
} /* namespace user_locks */

