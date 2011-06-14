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

#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>

#include <drizzled/message.h>
#include <drizzled/identifier/table.h>

namespace drizzled {
namespace message {

class Cache
{
public:
  static Cache &singleton()
  {
    static Cache open_cache;
    return open_cache;
  }

  size_t size() const
  {
    return cache.size();
  }

  void rehash(size_t arg)
  {
    cache.rehash(arg);
  }

  table::shared_ptr find(const identifier::Table &identifier);
  void erase(const identifier::Table &identifier);
  bool insert(const identifier::Table &identifier, table::shared_ptr share);
  bool insert(const identifier::Table &identifier, Table &share);
private:
  typedef boost::unordered_map<identifier::Table::Key, table::shared_ptr> Map;

  boost::mutex _access;
  Map cache;
};

} /* namespace message */
} /* namespace drizzled */

