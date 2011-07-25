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

Routines::Routines() :
  InformationSchema("ROUTINES")
{
  add_field("SPECIFIC_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("SPECIFIC_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("SPECIFIC_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);

  add_field("ROUTINE_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("ROUTINE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("ROUTINE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);

  add_field("MODULE_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("MODULE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("MODULE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);

  add_field("UDT_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("UDT_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("UDT_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);

  add_field("DATA_TYPE");
  add_field("CHARACTER_MAXIMUM_LENGTH");
  add_field("CHARACTER_OCTET_LENGTH");

  add_field("COLLATION_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("COLLATION_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("COLLATION_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);

  add_field("CHARACTER_SET_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CHARACTER_SET_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CHARACTER_SET_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);

  add_field("TABLE_CATALOG", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);

  add_field("NUMERIC_PRECISION");
  add_field("NUMERIC_PRECISION_RADIX");
  add_field("NUMERIC_SCALE");

  add_field("DATETIME_PRECISION");

  add_field("INTERVAL_TYPE");
  add_field("INTERVAL_PRECISION");

  add_field("TYPE_UDT_CATALOG");
  add_field("TYPE_UDT_SCHEMA");
  add_field("TYPE_UDT_NAME");

  add_field("SCOPE_CATALOG");
  add_field("SCOPE_SCHEMA");
  add_field("SCOPE_NAME");

  add_field("MAXIMUM_CARDINALITY");
  add_field("DTD_IDENTIFIER");

  add_field("ROUTINE_BODY");
  add_field("ROUTINE_DEFINITION");

  add_field("EXTERNAL_NAME");
  add_field("EXTERNAL_LANGUAGE");

  add_field("PARAMETER_STYLE");
  add_field("IS_DETERMINISTIC");

  add_field("SQL_DATA_ACCESS");
  add_field("IS_NULL_CALL");
  add_field("SQL_PATH");
  add_field("SCHEMA_LEVEL_ROUTINE");
  add_field("MAX_DYNAMIC_RESULT_SETS");
  add_field("IS_USER_DEFINED_CAST");
  add_field("IS_IMPLICITLY_INVOCABLE");
  add_field("CREATED");
  add_field("LAST_ALTERED");
}

void Routines::Generator::fill()
{
}

bool Routines::Generator::nextCore()
{
  return false;
}

bool Routines::Generator::next()
{
  while (not nextCore())
  {
    return false;
  }

  return true;
}

Routines::Generator::Generator(drizzled::Field **arg) :
  InformationSchema::Generator(arg),
  is_primed(false)
{
}

bool Routines::Generator::populate()
{
  if (not next())
    return false;

  fill();

  return true;
}
