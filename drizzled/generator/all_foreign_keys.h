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

typedef std::pair<const drizzled::message::Table *, int32_t> ForeignKeyConstraintPair;

class AllForeignKeys
{
  Session &session;
  message::Table table_message;
  drizzled::message::table::shared_ptr table_ptr;
  int32_t foreign_keys_iterator;

  drizzled::generator::AllTables all_tables_generator;

  bool table_setup();

public:

  AllForeignKeys(Session &arg);

  void reset();

  operator const drizzled::message::Table::ForeignKeyConstraint*()
  {
    if (table_ptr)
    {
      do {
        if (foreign_keys_iterator != table_message.fk_constraint_size())
        {
          const message::Table::ForeignKeyConstraint &fk_constraint(table_message.fk_constraint(foreign_keys_iterator++));
          return &fk_constraint;
        }

      } while ((table_ptr= all_tables_generator) && table_setup());
    }

    return NULL;
  }

  operator const ForeignKeyConstraintPair()
  {
    if (table_ptr)
    {
      do {
        if (foreign_keys_iterator != table_message.fk_constraint_size())
        {
          return std::make_pair(&table_message, foreign_keys_iterator++);
        }
      } while ((table_ptr= all_tables_generator) && table_setup());
    }

    ForeignKeyConstraintPair null_pair;
    return null_pair;
  }
};

} /* namespace generator */
} /* namespace drizzled */

