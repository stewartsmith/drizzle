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

#pragma once

#include <drizzled/util/string.h>
#include <drizzled/message/table.h>

#include <boost/unordered_map.hpp>

namespace drizzled {
namespace session {

class DRIZZLED_API TableMessages
{
  typedef boost::unordered_map<std::string, message::Table, util::insensitive_hash, util::insensitive_equal_to> Cache;

  Cache table_message_cache;

public:
  bool storeTableMessage(const identifier::Table &identifier, const message::Table &table_message);
  bool removeTableMessage(const identifier::Table &identifier);
  bool getTableMessage(const identifier::Table &identifier, message::Table &table_message);
  bool doesTableMessageExist(const identifier::Table &identifier);
  bool renameTableMessage(const identifier::Table &from, const identifier::Table &to);
};

} /* namespace session */
} /* namespace drizzled */

