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

Schemata::Schemata() :
  InformationSchema("SCHEMATA")
{
  add_field("CATALOG_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("SCHEMA_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("SCHEMA_OWNER", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("DEFAULT_CHARACTER_SET_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("DEFAULT_CHARACTER_SET_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
  add_field("DEFAULT_CHARACTER_SET_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, true);
}

Schemata::Generator::Generator(drizzled::Field **arg) :
  InformationSchema::Generator(arg),
  schema_generator(getSession())
{
}

bool Schemata::Generator::populate()
{
  drizzled::message::schema::shared_ptr schema_ptr;

  while ((schema_ptr= schema_generator))
  {
    /* CATALOG_NAME */
    push(schema_ptr->catalog());

    /* SCHEMA_NAME */
    push(schema_ptr->name());

    /* SCHEMA_OWNER */
    push();

    /* DEFAULT_CHARACTER_SET_CATALOG */
    push();

    /* DEFAULT_CHARACTER_SET_SCHEMA */
    push();

    /* DEFAULT_CHARACTER_SET_NAME */
    push();

    return true;
  }

  return false;
}
