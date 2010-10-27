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

#include "config.h"

#include "drizzled/pthread_globals.h"
#include "drizzled/session.h"
#include "drizzled/identifier/table.h"
#include "drizzled/table_share.h"

namespace drizzled {

namespace definition {

TableSharePtr Cache::getShare(const TableIdentifier &identifier)
{
  //safe_mutex_assert_owner(LOCK_open.native_handle);

  TableDefinitionCache::iterator iter= cache.find(identifier.getKey());
  if (iter != cache.end())
  {
    return (*iter).second;
  }

  return TableSharePtr();
}

void Cache::erase(const TableIdentifier &identifier)
{
  //safe_mutex_assert_owner(LOCK_open.native_handle);
  
  cache.erase(identifier.getKey());
}

bool Cache::insert(const TableIdentifier &identifier, TableSharePtr share)
{
  std::pair<TableDefinitionCache::iterator, bool> ret=
    cache.insert(std::make_pair(identifier.getKey(), share));

  return ret.second;
}

} /* namespace definition */
} /* namespace drizzled */
