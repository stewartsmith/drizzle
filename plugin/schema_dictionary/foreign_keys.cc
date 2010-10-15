/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems
 *  Copyright (C) 2010 Andrew Hutchings
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

ForeignKeysTool::ForeignKeysTool() :
  plugin::TableFunction("DATA_DICTIONARY", "FOREIGN_KEYS")
{
  add_field("CONSTRAINT_SCHEMA");
  add_field("CONSTRAINT_TABLE");
  add_field("CONSTRAINT_NAME");
  add_field("CONSTRAINT_COLUMNS");

  add_field("REFERENCED_TABLE_NAME");
  add_field("REFERENCED_TABLE_COLUMNS");

  add_field("MATCH_OPTION");
  add_field("UPDATE_RULE");
  add_field("DELETE_RULE");
}

ForeignKeysTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  all_tables_generator(getSession()),
  keyPos(0),
  firstFill(true)
{
}

bool ForeignKeysTool::Generator::nextTable()
{
  const drizzled::message::Table *table_ptr;
  while ((table_ptr= all_tables_generator))
  {
    table_message.CopyFrom(*table_ptr);
    return true;
  }

  return false;
}

bool ForeignKeysTool::Generator::fillFkey()
{
  if (firstFill)
  {
    firstFill= false;
    nextTable();
  }

  while(true)
  {
    if (keyPos < getTableMessage().fk_constraint_size())
    { 
      fkey= getTableMessage().fk_constraint(keyPos);
      keyPos++;
      fill();
      return true;
    }
    else if (nextTable())
    {
      keyPos= 0;
    }
    else
      return false;
  }
}

bool ForeignKeysTool::Generator::populate()
{
  if (fillFkey())
    return true;

  return false;
}

void ForeignKeysTool::Generator::pushType(message::Table::Field::FieldType type)
{
  switch (type)
  {
    default:
      push("VARCHAR");
      break;
  }
}

std::string ForeignKeysTool::Generator::fkeyOption(message::Table::ForeignKeyConstraint::ForeignKeyOption option)
{
  std::string ret;
  switch (option)
  {
    case message::Table::ForeignKeyConstraint::OPTION_UNDEF:
      ret= "UNDEFINED";
      break;
    case message::Table::ForeignKeyConstraint::OPTION_RESTRICT:
      ret= "RESTRICT";
      break;
    case message::Table::ForeignKeyConstraint::OPTION_CASCADE:
      ret= "CASCADE";
      break;
    case message::Table::ForeignKeyConstraint::OPTION_SET_NULL:
      ret= "SET NULL";
      break;
    case message::Table::ForeignKeyConstraint::OPTION_NO_ACTION:
      ret= "NO ACTION";
      break;
    case message::Table::ForeignKeyConstraint::OPTION_DEFAULT:
    default:
      ret= "DEFAULT";
      break;
  }
  return ret;
}

void ForeignKeysTool::Generator::fill()
{
  /* CONSTRAINT_SCHEMA */
  push(getTableMessage().schema());

  /* CONSTRAINT_TABLE */
  push(getTableMessage().name());

  /* CONSTRAINT_NAME */
  push(fkey.name());

  /* CONSTRAINT_COLUMNS */
  std::string source;

  for (ssize_t x= 0; x < fkey.column_names_size(); ++x)
  {
    if (x != 0)
      source.append(", ");
    source.push_back('`');
    source.append(fkey.column_names(x));
    source.push_back('`');
  }

  push(source);

  /* REFERENCED_TABLE_NAME */
  push(fkey.references_table_name());

  /* REFERENCED_TABLE_COLUMNS */
  std::string destination;

  for (ssize_t x= 0; x < fkey.references_columns_size(); ++x)
  {
    if (x != 0)
      destination.append(", ");
    destination.push_back('`'); 
    destination.append(fkey.references_columns(x));
    destination.push_back('`');
  }

  push(destination);

  /* MATCH_OPTION */
  switch (fkey.match())
  {
    case message::Table::ForeignKeyConstraint::MATCH_FULL:
      push("FULL");
      break;
    case message::Table::ForeignKeyConstraint::MATCH_PARTIAL:
      push("PARTIAL");
      break;
    case message::Table::ForeignKeyConstraint::MATCH_SIMPLE:
      push("SIMPLE");
      break;
    default:
      push("NONE");
      break;
  }
  /* UPDATE_RULE */
  push(fkeyOption(fkey.update_option()));

  /* DELETE_RULE */
  push(fkeyOption(fkey.delete_option()));

}
