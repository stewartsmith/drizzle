/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems
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

#include <plugin/data_engine/dictionary.h>
#include <drizzled/charset.h>

using namespace std;
using namespace drizzled;

ReferentialConstraintsTool::ReferentialConstraintsTool()
{
  message::Table::StorageEngine *engine;
  message::Table::TableOptions *table_options;

  schema.set_name("referential_constraints");
  schema.set_type(message::Table::STANDARD);

  table_options= schema.mutable_options();
  table_options->set_collation_id(default_charset_info->number);
  table_options->set_collation(default_charset_info->name);

  engine= schema.mutable_engine();
  engine->set_name(engine_name);

  add_field(schema, "CONSTRAINT_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field(schema, "CONSTRAINT_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "CONSTRAINT_NAME", message::Table::Field::VARCHAR, 64);

  add_field(schema, "UNIQUE_CONSTRAINT_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field(schema, "UNIQUE_CONSTRAINT_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "UNIQUE_CONSTRAINT_NAME", message::Table::Field::VARCHAR, 64);


  add_field(schema, "MATCH_OPTION", message::Table::Field::VARCHAR, 64);
  add_field(schema, "UPDATE_RULE", message::Table::Field::VARCHAR, 64);
  add_field(schema, "DELETE_RULE", message::Table::Field::VARCHAR, 64);
  add_field(schema, "TABLE_NAME", message::Table::Field::VARCHAR, 64);

  add_field(schema, "REFERENCED_TABLE_NAME", message::Table::Field::VARCHAR, 64);
}

ReferentialConstraintsTool::Generator::Generator()
{
}

bool ReferentialConstraintsTool::Generator::populate(Field ** fields)
{
  (void)fields;

  return false;
}
