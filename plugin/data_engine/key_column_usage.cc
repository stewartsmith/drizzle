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

KeyColumnUsageTool::KeyColumnUsageTool()
{
  message::Table::StorageEngine *engine;
  message::Table::TableOptions *table_options;

  setName("KEY_COLUMN_USAGE");
  schema.set_name(getName().c_str());
  schema.set_type(message::Table::STANDARD);

  table_options= schema.mutable_options();
  table_options->set_collation_id(default_charset_info->number);
  table_options->set_collation(default_charset_info->name);

  engine= schema.mutable_engine();
  engine->set_name(engine_name);

  add_field(schema, "CONSTRAINT_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field(schema, "CONSTRAINT_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "CONSTRAINT_NAME", message::Table::Field::VARCHAR, 64);

  add_field(schema, "TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field(schema, "TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "TABLE_NAME", message::Table::Field::VARCHAR, 64);

  add_field(schema, "COLUMN_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "ORDINAL_POSITION", message::Table::Field::BIGINT);
  add_field(schema, "POSITION_IN_UNIQUE_CONSTRAINT", message::Table::Field::BIGINT);

  add_field(schema, "REFERENCED_TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "REFERENCED_TABLE_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "REFERENCED_COLUMN_NAME", message::Table::Field::VARCHAR, 64);
}

KeyColumnUsageTool::Generator::Generator()
{
}

bool KeyColumnUsageTool::Generator::populate(Field ** fields)
{
  (void)fields;

  return false;
}
