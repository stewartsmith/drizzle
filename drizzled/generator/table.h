/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <drizzled/plugin/storage_engine.h>

namespace drizzled {
namespace generator {

class Table
{
  Session &session;

  identifier::table::vector table_names;
  identifier::table::vector::const_iterator table_iterator;

public:

  Table(Session &arg, const identifier::Schema &schema_identifier);

  operator const drizzled::message::table::shared_ptr()
  {
    while (table_iterator != table_names.end())
    {
      message::table::shared_ptr table;
      table= plugin::StorageEngine::getTableMessage(session, *table_iterator);
      table_iterator++;

      if (table)
        return table;
    }

    return message::table::shared_ptr();
  }

  operator const drizzled::identifier::Table*()
  {
    while (table_iterator != table_names.end())
    {
      const drizzled::identifier::Table *_ptr= &(*table_iterator);
      table_iterator++;

      return _ptr;
    }

    return NULL;
  }
};

} /* namespace generator */
} /* namespace drizzled */

