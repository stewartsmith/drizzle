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

#ifndef DRIZZLED_GENERATOR_ALL_TABLES_H
#define DRIZZLED_GENERATOR_ALL_TABLES_H

#include "drizzled/plugin/storage_engine.h"

namespace drizzled {
namespace generator {

class AllTables
{
  Session &session;
  message::Table table;

  TableIdentifiers table_names;
  TableIdentifiers::const_iterator table_iterator;

  drizzled::generator::Schema schema_generator;
  const drizzled::SchemaIdentifier *schema_ptr;

  bool table_setup();

public:

  AllTables(Session &arg);

  void reset();

  operator const drizzled::message::Table*()
  {
    do {
      while (table_iterator != table_names.end())
      {
        table.Clear();
        bool is_table_parsed= plugin::StorageEngine::getTableDefinition(session, *table_iterator, table);
        table_iterator++;

        if (is_table_parsed)
          return &table;
      }
    } while ((schema_ptr= schema_generator) && table_setup());

    return NULL;
  }

  operator const drizzled::TableIdentifier*()
  {
    do {
      while (table_iterator != table_names.end())
      {
        const drizzled::TableIdentifier *_ptr= &(*table_iterator);
        table_iterator++;

        return _ptr;
      }
    } while ((schema_ptr= schema_generator) && table_setup());

    return NULL;
  }
};

} /* namespace generator */
} /* namespace drizzled */

#endif /* DRIZZLED_GENERATOR_ALL_TABLES_H */
