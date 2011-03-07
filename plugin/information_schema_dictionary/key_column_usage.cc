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

KeyColumnUsage::KeyColumnUsage() :
  InformationSchema("KEY_COLUMN_USAGE")
{
  add_field("CONSTRAINT_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CONSTRAINT_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CONSTRAINT_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("COLUMN_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("ORDINAL_POSITION", plugin::TableFunction::NUMBER, 0, false);
}

KeyColumnUsage::Generator::Generator(drizzled::Field **arg) :
  InformationSchema::Generator(arg),
  generator(getSession()),
  index_iterator(0),
  index_part_iterator(0),
  fk_constraint_iterator(0),
  fk_constraint_column_name_iterator(0)
{
  while (not (table_message= generator))
  { };
}

bool KeyColumnUsage::Generator::populate()
{
  if (not table_message)
    return false;

  do 
  {
    if (index_iterator != table_message->indexes_size())
    {
      drizzled::message::Table::Index index= table_message->indexes(index_iterator);

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

        /* COLUMN_NAME */
        int32_t fieldnr= index.index_part(index_part_iterator).fieldnr();
        push(table_message->field(fieldnr).name());

        /* ORDINAL_POSITION */
        push(static_cast<int64_t>((index_part_iterator +1)));

        if (index_part_iterator == index.index_part_size() -1)
        {
          index_iterator++;
          index_part_iterator= 0;
        }
        else
        {
          index_part_iterator++;
        }

        return true;
      }
    }

    if (fk_constraint_iterator != table_message->fk_constraint_size())
    {
      drizzled::message::Table::ForeignKeyConstraint foreign_key= table_message->fk_constraint(fk_constraint_iterator);

      {
        /* Constraints live in the same catalog.schema as the table they refer too. */
        /* CONSTRAINT_CATALOG */
        push(table_message->catalog());

        /* CONSTRAINT_SCHEMA */
        push(table_message->schema());

        /* CONSTRAINT_NAME */
        push(foreign_key.name());

        /* TABLE_CATALOG */
        push(table_message->catalog());

        /* TABLE_SCHEMA */
        push(table_message->schema());

        /* TABLE_NAME */
        push(table_message->name());

        /* COLUMN_NAME */
        push(foreign_key.column_names(fk_constraint_column_name_iterator));

        /* ORDINAL_POSITION */
        push(static_cast<int64_t>((fk_constraint_column_name_iterator + 1)));

        if (fk_constraint_column_name_iterator == foreign_key.column_names_size() -1)
        {
          fk_constraint_iterator++;
          fk_constraint_column_name_iterator= 0;
        }
        else
        {
          fk_constraint_column_name_iterator++;
        }

        return true;
      }
    }

    index_iterator= 0;
    index_part_iterator= 0;

    fk_constraint_iterator= 0;
    fk_constraint_column_name_iterator= 0;

  } while ((table_message= generator));

  return false;
}
