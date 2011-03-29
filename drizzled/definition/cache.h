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

#include <drizzled/table/instance.h>

namespace drizzled {
namespace definition {

class Cache
{
public:
  static inline Cache &singleton()
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

  table::instance::Shared::shared_ptr find(const identifier::Table::Key &identifier);
  void erase(const identifier::Table::Key &identifier);
  bool insert(const identifier::Table::Key &identifier, table::instance::Shared::shared_ptr share);

protected:
  friend class drizzled::generator::TableDefinitionCache;

  void CopyFrom(table::instance::Shared::vector &vector);

private:
  typedef boost::unordered_map< identifier::Table::Key, table::instance::Shared::shared_ptr> Map;

  Map cache;
  boost::mutex _mutex;
};

} /* namespace definition */
} /* namespace drizzled */

