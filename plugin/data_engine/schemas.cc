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

#include <plugin/data_engine/schemas.h>

using namespace std;
using namespace drizzled;

SchemasTool::SchemasTool() :
  plugin::TableFunction("DATA_DICTIONARY", "SCHEMAS")
{
  add_field("SCHEMA_NAME");
  add_field("DEFAULT_COLLATION_NAME");
}

SchemasTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  is_schema_primed(false),
  is_schema_parsed(false)
{
}

/**
  @note return true if a match occurs.
*/
bool SchemasTool::Generator::checkSchema()
{
  return false;
}

bool SchemasTool::Generator::nextSchemaCore()
{
  if (is_schema_primed)
  {
    schema_iterator++;
  }
  else
  {
    plugin::StorageEngine::getSchemaNames(schema_names);
    schema_iterator= schema_names.begin();
    is_schema_primed= true;
  }

  if (schema_iterator == schema_names.end())
    return false;

  schema.Clear();
  is_schema_parsed= plugin::StorageEngine::getSchemaDefinition(*schema_iterator, schema);

  if (checkSchema())
      return false;

  return true;
}
  
bool SchemasTool::Generator::nextSchema()
{
  while (not nextSchemaCore())
  {
    if (schema_iterator == schema_names.end())
      return false;
  }

  return true;
}


bool SchemasTool::Generator::populate()
{
  if (nextSchema())
  {
    fill();
    return true;
  }

  return false;
}

/**
  A lack of a parsed schema file means we are using defaults.
*/
void SchemasTool::Generator::fill()
{
  /* SCHEMA_NAME */
  push(schema.name());

  /* DEFAULT_COLLATION_NAME */
  if (is_schema_parsed)
    push(schema.collation());
  else
    push(scs->name);
}
