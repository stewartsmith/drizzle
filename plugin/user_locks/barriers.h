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
namespace barriers {

const size_t LARGEST_BARRIER_NAME= 64;

enum return_t {
  SUCCESS,
  NOT_FOUND,
  NOT_OWNED_BY
};

class Barriers
{
public:
  typedef boost::unordered_map<user_locks::Key, Barrier::shared_ptr> Map;

  static Barriers &getInstance(void)
  {
    static Barriers instance;
    return instance;
  }

  bool create(const user_locks::Key &arg, drizzled::session_id_t owner);
  bool create(const user_locks::Key &arg, drizzled::session_id_t owner, int64_t wait_count);
  return_t release(const user_locks::Key &arg, drizzled::session_id_t owner);
  Barrier::shared_ptr find(const user_locks::Key &arg);
  void Copy(Map &arg);

private:

  boost::mutex mutex;
  Map barrier_map; 
};


} /* namespace barriers */
} /* namespace user_locks */

