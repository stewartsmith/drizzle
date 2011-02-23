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

Columns::Columns() :
  InformationSchema("COLUMNS")
{
  add_field("TABLE_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("COLUMN_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("ORDINAL_POSITION", plugin::TableFunction::NUMBER, 0, false);
  add_field("COLUMN_DEFAULT", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("IS_NULLABLE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("DATA_TYPE", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CHARACTER_MAXIMUM_LENGTH", plugin::TableFunction::NUMBER, 0, true);
  add_field("CHARACTER_OCTET_LENGTH", plugin::TableFunction::NUMBER, 0, true);
  add_field("NUMERIC_PRECISION", plugin::TableFunction::NUMBER, 0, true);
  add_field("NUMERIC_PRECISION_RADIX", plugin::TableFunction::NUMBER, 0, true);
  add_field("NUMERIC_SCALE", plugin::TableFunction::NUMBER, 0, true);
  add_field("DATETIME_PRECISION", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("CHARACTER_SET_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("CHARACTER_SET_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("CHARACTER_SET_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("COLLATION_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("COLLATION_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("COLLATION_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("DOMAIN_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("DOMAIN_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("DOMAIN_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
}

Columns::Generator::Generator(drizzled::Field **arg) :
  InformationSchema::Generator(arg),
  field_generator(getSession())
{
}

bool Columns::Generator::populate()
{
  drizzled::generator::FieldPair field_pair;
  while (!!(field_pair= field_generator))
  {
    const drizzled::message::Table *table_message= field_pair.first;
    int32_t field_iterator= field_pair.second;
    const message::Table::Field &field(table_message->field(field_pair.second));

    /* TABLE_CATALOG */
    push(table_message->catalog());

    /* TABLE_SCHEMA */
    push(table_message->schema());

    /* TABLE_NAME */
    push(table_message->name());

    /* COLUMN_NAME */
    push(field.name());

    /* ORDINAL_POSITION */
    push(static_cast<int64_t>(field_iterator));

    /* COLUMN_NAME */
    if (field.options().has_default_value())
    {
      push(field.options().default_value());
    }
    else
    {
      push();
    }

    /* IS_NULLABLE */
    push(not field.constraints().is_notnull());

    /* DATA_TYPE <-- display the type that the user is going to expect, which is not the same as the type we store internally */
    push(drizzled::message::type(field));

    /* "CHARACTER_MAXIMUM_LENGTH" */
    if (field.string_options().has_length())
    {
      push(static_cast<int64_t>(field.string_options().length()));
    }
    else
    {
      push();
    }

    /* "CHARACTER_OCTET_LENGTH" */
    if (field.string_options().has_length())
    {
      push(static_cast<int64_t>(field.string_options().length() * 4));
    }
    else
    {
      push();
    }

    /* "NUMERIC_PRECISION" */
    if (field.numeric_options().has_precision())
    {
      push(static_cast<int64_t>(field.numeric_options().precision()));
    }
    else
    {
      push();
    }

    /* NUMERIC_PRECISION_RADIX */
    push();

    /* "NUMERIC_SCALE" */
    if (field.numeric_options().has_scale())
    {
      push(static_cast<int64_t>(field.numeric_options().scale()));
    }
    else
    {
      push();
    }

    /* DATETIME_PRECISION */
    push();

    /* CHARACTER_SET_CATALOG */
    push();

    /* CHARACTER_SET_SCHEMA */
    push();

    /* CHARACTER_SET_NAME */
    push();

    /* COLLATION_CATALOG */
    push();

    /* COLLATION_SCHEMA */
    push();

    /* COLLATION_NAME */
    if (field.string_options().has_collation())
    {
      push(field.string_options().collation());
    }
    else
    {
      push();
    }

    /* DOMAIN_CATALOG */
    push();

    /* DOMAIN_SCHEMA */
    push();

    /* DOMAIN_NAME */
    push();

    return true;
  }

  return false;
}
