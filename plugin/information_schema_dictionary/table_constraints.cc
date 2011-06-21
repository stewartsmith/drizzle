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

#include <config.h>
#include <plugin/information_schema_dictionary/dictionary.h>

using namespace std;
using namespace drizzled;

TableConstraints::TableConstraints() :
  InformationSchema("TABLE_CONSTRAINTS")
{
  add_field("CONSTRAINT_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CONSTRAINT_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CONSTRAINT_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CONSTRAINT_TYPE");
  add_field("IS_DEFERRABLE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("INITIALLY_DEFERRED", plugin::TableFunction::BOOLEAN, 0, false);
}

TableConstraints::Generator::Generator(drizzled::Field **arg) :
  InformationSchema::Generator(arg),
  generator(getSession()),
  index_iterator(0)
{
  while (not (table_message= generator))
  { };
}

bool TableConstraints::Generator::populate()
{
  if (not table_message)
    return false;

  do 
  {
    if (index_iterator != table_message->indexes_size())
    {
      drizzled::message::Table::Index index= table_message->indexes(index_iterator);

      index_iterator++;

      if (index.is_primary() || index.is_unique())
      {
        /* Constraints live in the same catalog.schema as the table they refer too. */
        /* CONSTRAINT_CATALOG */
        push(table_message->catalog());

        /* CONSTRAINT_SCHEMA */
        push(table_message->schema());

        /* CONSTRAINT_NAME */
        push(index.name());

        /* TABLE_CATALOG */
        push(table_message->catalog());

        /* TABLE_SCHEMA */
        push(table_message->schema());

        /* TABLE_NAME */
        push(table_message->name());

        /* CONSTRAINT_TYPE */
        if (index.is_primary())
        {
          push("PRIMARY KEY");
        }
        else if (index.is_unique())
        {
          push("UNIQUE");
        }
        else
        {
          assert(0);
          push("UNIQUE");
        }

        /* IS_DEFERRABLE */
        push(false);

        /* INITIALLY_DEFERRED */
        push(false);

        return true;
      }
    }

    index_iterator= 0;

  } while ((table_message= generator));

  return false;
}
