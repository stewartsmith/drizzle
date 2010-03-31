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

#include "config.h"
#include "plugin/schema_dictionary/dictionary.h"

using namespace std;
using namespace drizzled;

SchemasTool::SchemasTool() :
  plugin::TableFunction("DATA_DICTIONARY", "SCHEMAS")
{
  add_field("SCHEMA_NAME");
  add_field("DEFAULT_COLLATION_NAME");
  add_field("SCHEMA_CREATION_TIME");
  add_field("SCHEMA_UPDATE_TIME");
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
  if (isWild(schema_name()))
    return true;

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
    Session *session= current_session;
    plugin::StorageEngine::getSchemaIdentifiers(*session, schema_names);
    schema_names.sort();
    schema_iterator= schema_names.begin();
    is_schema_primed= true;
  }

  if (schema_iterator == schema_names.end())
    return false;

  schema.Clear();
  SchemaIdentifier schema_identifier(*schema_iterator);
  is_schema_parsed= plugin::StorageEngine::getSchemaDefinition(schema_identifier, schema);

  if (not is_schema_parsed)
  {
    return false;
  }

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

  /* SCHEMA_CREATION_TIME */
  time_t time_arg= schema.creation_timestamp();
  char buffer[40];
  struct tm tm_buffer;

  localtime_r(&time_arg, &tm_buffer);
  strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
  push(buffer);

  /* SCHEMA_UPDATE_TIME */
  time_arg= schema.update_timestamp();
  localtime_r(&time_arg, &tm_buffer);
  strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
  push(buffer);
}
