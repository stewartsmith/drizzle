/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#include <drizzled/util/string.h>
#include <drizzled/session/table_messages.h>
#include <drizzled/identifier.h>
#include <drizzled/message/table.h>
#include <drizzled/util/find_ptr.h>
#include <string>

namespace drizzled {
namespace session {

bool TableMessages::storeTableMessage(const identifier::Table &identifier, const message::Table &table_message)
{
  table_message_cache.insert(make_pair(identifier.getPath(), table_message));
  return true;
}

bool TableMessages::removeTableMessage(const identifier::Table &identifier)
{
  Cache::iterator iter= table_message_cache.find(identifier.getPath());
  if (iter == table_message_cache.end())
    return false;
  table_message_cache.erase(iter);
  return true;
}

bool TableMessages::getTableMessage(const identifier::Table &identifier, message::Table &table_message)
{
  Cache::mapped_type* ptr= find_ptr(table_message_cache, identifier.getPath());
  if (!ptr)
    return false;
  table_message.CopyFrom(*ptr);
  return true;
}

bool TableMessages::doesTableMessageExist(const identifier::Table &identifier)
{
  return find_ptr(table_message_cache, identifier.getPath());
}

bool TableMessages::renameTableMessage(const identifier::Table &from, const identifier::Table &to)
{
  table_message_cache[to.getPath()]= table_message_cache[from.getPath()];
  Cache::mapped_type* ptr= find_ptr(table_message_cache, to.getPath());
  if (!ptr)
    return false;
  ptr->set_schema(to.getSchemaName());
  ptr->set_name(to.getTableName());
  return true;
}

} /* namespace session */
} /* namespace drizzled */
