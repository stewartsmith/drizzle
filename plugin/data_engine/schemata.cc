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

SchemataTool::SchemataTool() :
  Tool("SCHEMATA")
{
  add_field("CATALOG_NAME", message::Table::Field::VARCHAR, 512);
  add_field("SCHEMA_NAME", message::Table::Field::VARCHAR, 64);
  add_field("DEFAULT_CHARACTER_SET_NAME", message::Table::Field::VARCHAR, 64);
  add_field("DEFAULT_COLLATION_NAME", message::Table::Field::VARCHAR, 64);
  add_field("SQL_PATH", message::Table::Field::VARCHAR, 512);
}

SchemataTool::Generator::Generator()
{
  plugin::StorageEngine::getSchemaNames(set_of_names);

  it= set_of_names.begin();
}

bool SchemataTool::Generator::populate(Field ** fields)
{
  if (it == set_of_names.end())
    return false;

  bool rc= fill(fields);

  it++;

  return rc;
}

bool SchemataTool::Generator::fill(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  message::Schema schema;
  Field **field= fields;
  bool parsed;

  parsed= plugin::StorageEngine::getSchemaDefinition(schema_name(), schema);

  /* TABLE_CATALOG */
  (*field)->store("default", sizeof("default"), scs);
  field++;

  if (not parsed)
  {
    (*field)->store(schema_name().c_str(), schema_name().length(), scs);
    field++;

    for (; *field ; field++)
      (*field)->store("<error>", sizeof("<error>"), scs);

    return true;
  }

  /* SCHEMA_NAME */
  (*field)->store(schema.name().c_str(), schema.name().length(), scs);
  field++;

  /* DEFAULT_CHARACTER_SET_NAME */
  (*field)->store("utf8", sizeof("utf8"), scs);
  field++;

  /* DEFAULT_COLLATION_NAME */
  (*field)->store(schema.collation().c_str(), schema.collation().length(), scs);
  field++;

  for (; *field ; field++)
  {
    (*field)->store("<not implemented>", sizeof("<not implemented>"), scs);
  }

  return true;
}

SchemataNamesTool::SchemataNamesTool() :
  SchemataTool("SCHEMATA_NAMES")
{
  add_field("SCHEMA_NAME", message::Table::Field::VARCHAR, 64);
}

bool SchemataNamesTool::Generator::fill(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  (*field)->store(schema_name().c_str(), schema_name().length(), scs);

  return true;
}

SchemataInfoTool::SchemataInfoTool() :
  SchemataTool("SCHEMATA_INFO")
{
  add_field("SCHEMA_NAME", message::Table::Field::VARCHAR, 64);
  add_field("DEFAULT_COLLATION_NAME", message::Table::Field::VARCHAR, 64);
}

bool SchemataInfoTool::Generator::fill(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  message::Schema schema;
  Field **field= fields;
  bool parsed;

  parsed= plugin::StorageEngine::getSchemaDefinition(schema_name(), schema);

  if (not parsed)
  {
    (*field)->store(schema_name().c_str(), schema_name().length(), scs);
    field++;

    for (; *field ; field++)
      (*field)->store("<error>", sizeof("<error>"), scs);

    return true;
  }

  /* SCHEMA_NAME */
  (*field)->store(schema.name().c_str(), schema.name().length(), scs);
  field++;

  /* DEFAULT_COLLATION_NAME */
  (*field)->store(schema.collation().c_str(), schema.collation().length(), scs);

  return true;
}
