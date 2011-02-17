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

Parameters::Parameters() :
  InformationSchema("PARAMETERS")
{
  add_field("SPECIFIC_CATALOG");
  add_field("SPECIFIC_SCHEMA");
  add_field("SPECIFIC_NAME");
  add_field("ORDINAL_POSITION");
  add_field("PARAMETER_MODE");
  add_field("IS_RESULT");
  add_field("AS_LOCATOR");
  add_field("PARAMETER_NAME");
  add_field("DATA_TYPE");
  add_field("CHARACTER_MAXIMUM_LENGTH");
  add_field("CHARACTER_OCTET_LENGTH");
  add_field("CHARACTER_OCTET_LENGTH");
  add_field("COLLATION_CATALOG");
  add_field("COLLATION_SCHEMA");
  add_field("COLLATION_NAME");
  add_field("CHARACTER_SET_CATALOG");
  add_field("CHARACTER_SET_SCHEMA");
  add_field("CHARACTER_SET_NAME");
  add_field("NUMERIC_PRECISION");
  add_field("NUMERIC_PRECISION_RADIX");
  add_field("NUMERIC_SCALE");
  add_field("DATETIME_PRECISION");
  add_field("INTERVAL_TYPE");
  add_field("INTERVAL_PRECISION");
  add_field("USER_DEFINED_TYPE_CATALOG");
  add_field("USER_DEFINED_TYPE_SCHEMA");
  add_field("USER_DEFINED_TYPE_NAME");
  add_field("SCOPE_CATALOG");
  add_field("SCOPE_SCHEMA");
  add_field("SCOPE_NAME");
}

void Parameters::Generator::fill()
{
}

bool Parameters::Generator::nextCore()
{
  return false;
}

bool Parameters::Generator::next()
{
  while (not nextCore())
  {
    return false;
  }

  return true;
}

Parameters::Generator::Generator(drizzled::Field **arg) :
  InformationSchema::Generator(arg),
  is_primed(false)
{
}

bool Parameters::Generator::populate()
{
  if (not next())
    return false;

  fill();

  return true;
}
