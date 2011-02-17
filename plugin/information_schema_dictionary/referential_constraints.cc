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

ReferentialConstraints::ReferentialConstraints() :
  InformationSchema("REFERENTIAL_CONSTRAINTS")
{
  add_field("CONSTRAINT_CATALOG");
  add_field("CONSTRAINT_SCHEMA");
  add_field("CONSTRAINT_NAME");
  add_field("UNIQUE_CONSTRAINT_CATALOG");
  add_field("UNIQUE_CONSTRAINT_SCHEMA");
  add_field("UNIQUE_CONSTRAINT_NAME");
  add_field("MATCH_OPTION");
  add_field("UPDATE_RULE");
  add_field("DELETE_RULE");
}

ReferentialConstraints::Generator::Generator(drizzled::Field **arg) :
  InformationSchema::Generator(arg),
  foreign_key_generator(getSession())
{
}

bool ReferentialConstraints::Generator::populate()
{
  drizzled::generator::FieldPair field_pair;
  while (!!(field_pair= foreign_key_generator))
  {
    const drizzled::message::Table *table_message= field_pair.first;
    const message::Table::ForeignKeyConstraint &foreign_key(table_message->fk_constraint(field_pair.second));

    // CONSTRAINT_CATALOG
    push(table_message->catalog());

    // CONSTRAINT_SCHEMA
    push(table_message->schema());

    // CONSTRAINT_NAME
    push(foreign_key.name());

    // UNIQUE_CONSTRAINT_CATALOG
    push(table_message->catalog());
    
    // UNIQUE_CONSTRAINT_SCHEMA
    push(table_message->schema());

    // UNIQUE_CONSTRAINT_NAME
    push(" ");
    
    // MATCH_OPTION 
    push(drizzled::message::type(foreign_key.match()));
    
    //UPDATE_RULE
    push(drizzled::message::type(foreign_key.update_option()));

    //DELETE_RULE
    push(drizzled::message::type(foreign_key.delete_option()));

    return true;
  }

  return false;
}
