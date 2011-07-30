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

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

#include <string>

#include <drizzled/session.h>
#include <drizzled/util/string.h>

#pragma once

namespace user_locks {

class Key {
  drizzled::identifier::User context;
  std::string lock_name;
  size_t hash_value;

public:
  Key(const drizzled::identifier::User &context_arg, const std::string &lock_name_arg) :
    context(context_arg),
    lock_name(lock_name_arg)
  {
    drizzled::util::insensitive_hash hasher;
    hash_value= hasher(context.username() + lock_name_arg);
  }

  size_t getHashValue() const
  {
    return hash_value;
  }

  const std::string &getLockName() const
  {
    return lock_name;
  }

  const std::string &getUser() const
  {
    return context.username();
  }
};

bool operator==(Key const& left, Key const& right);

std::size_t hash_value(Key const& b);

typedef boost::unordered_set<Key> Keys;

} /* namespace user_locks */

