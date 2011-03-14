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

namespace drizzled {
namespace generator {

class AllIndexes
{
  Session &session;
  message::Table table_message;
  drizzled::message::table::shared_ptr table_ptr;
  int32_t index_iterator;

  drizzled::generator::AllTables all_tables_generator;

  bool table_setup();

public:

  AllIndexes(Session &arg);

  void reset();

  operator const drizzled::message::Table::Index*()
  {
    if (table_ptr)
    {
      do {
        if (index_iterator != table_message.indexes_size())
        {
          const message::Table::Index &index(table_message.indexes(index_iterator++));
          return &index;
        }

      } while ((table_ptr= all_tables_generator) && table_setup());
    }

    return NULL;
  }
};

} /* namespace generator */
} /* namespace drizzled */

