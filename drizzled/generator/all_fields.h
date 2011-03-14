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

typedef std::pair<const drizzled::message::Table *, int32_t> FieldPair;

class AllFields
{
  Session &session;
  message::Table table_message;
  drizzled::message::table::shared_ptr table_ptr;
  int32_t field_iterator;

  drizzled::generator::AllTables all_tables_generator;

  bool table_setup();

public:

  AllFields(Session &arg);

  void reset();

  operator const drizzled::message::Table::Field*()
  {
    if (table_ptr)
    {
      do {
        if (field_iterator != table_message.field_size())
        {
          const message::Table::Field &field(table_message.field(field_iterator++));
          return &field;
        }

      } while ((table_ptr= all_tables_generator) && table_setup());
    }

    return NULL;
  }

  operator const FieldPair()
  {
    if (table_ptr)
    {
      do {
        if (field_iterator != table_message.field_size())
        {
          return std::make_pair(&table_message, field_iterator++);
        }
      } while ((table_ptr= all_tables_generator) && table_setup());
    }

    FieldPair null_pair;
    return null_pair;
  }
};

} /* namespace generator */
} /* namespace drizzled */

bool operator!(const drizzled::generator::FieldPair &arg);

