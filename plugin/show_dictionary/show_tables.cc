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
#include <plugin/show_dictionary/dictionary.h>
#include <drizzled/identifier.h>

using namespace std;
using namespace drizzled;

ShowTables::ShowTables() :
  show_dictionary::Show("SHOW_TABLES")
{
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
}

ShowTables::Generator::Generator(drizzled::Field **arg) :
  show_dictionary::Show::Generator(arg),
  is_primed(false)
{
  if (not isShowQuery())
   return;

  statement::Show& select= static_cast<statement::Show&>(statement());

  if (not select.getShowSchema().empty())
  {
    schema_name.append(select.getShowSchema());
    assert(not schema_name.empty());
  }
}

bool ShowTables::Generator::nextCore()
{
  if (is_primed)
  {
    table_iterator++;
  }
  else
  {
    if (schema_name.empty())
    {
      is_primed= true;
      return false;
    }

    identifier::Schema identifier(schema_name);
    plugin::StorageEngine::getIdentifiers(getSession(), identifier, set_of_identifiers);
    table_iterator= set_of_identifiers.begin();
    is_primed= true;
  }

  if (table_iterator == set_of_identifiers.end())
    return false;

  if (isWild((*table_iterator).getTableName()))
    return false;

  return true;
}

bool ShowTables::Generator::next()
{
  while (not nextCore())
  {
    if (table_iterator != set_of_identifiers.end())
      continue;

    return false;
  }

  return true;
}

bool ShowTables::Generator::populate()
{
  if (not next())
    return false;

  fill();

  return true;
}

void ShowTables::Generator::fill()
{
  /* TABLE_NAME */
  push((*table_iterator).getTableName());
}
