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

using namespace std;
using namespace drizzled;

SchemaTool::SchemaTool() :
  Tool("SCHEMA_NAMES")
{
  add_field("SCHEMA_NAME", message::Table::Field::VARCHAR, 64);
}

SchemaTool::Generator::Generator()
{
  plugin::StorageEngine::getSchemaNames(set_of_names);

  it= set_of_names.begin();
}

bool SchemaTool::Generator::populate(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  if (it == set_of_names.end())
    return false;

  (*field)->store((*it).c_str(), (*it).length(), scs);

  it++;

  return true;
}

SchemaFullTool::SchemaFullTool() :
  Tool("SCHEMA_INFO")
{
  add_field("SCHEMA_NAME", message::Table::Field::VARCHAR, 64);
  add_field("DEFAULT_COLLATION_NAME", message::Table::Field::VARCHAR, 64);
}

SchemaFullTool::Generator::Generator()
{
  plugin::StorageEngine::getSchemaNames(set_of_names);

  it= set_of_names.begin();
}

bool SchemaFullTool::Generator::populate(Field ** fields)
{
  message::Schema schema;
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;
  bool error;

  if (it == set_of_names.end())
    return false;

  string schema_name= (*it);
  error= plugin::StorageEngine::getSchemaDefinition(schema_name, schema);

  if (error)
  {
    (*field)->store((*it).c_str(), (*it).length(), scs);
    field++;

    (*field)->store("<error>", sizeof("<error>"), scs);
  }
  else
  {
    (*field)->store(schema.name().c_str(), schema.name().length(), scs);
    field++;

    (*field)->store(schema.collation().c_str(), schema.collation().length(), scs);
  }

  it++;

  return true;
}
