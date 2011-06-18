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
#include <drizzled/open_tables_state.h>

using namespace std;
using namespace drizzled;

ShowTemporaryTables::ShowTemporaryTables() :
  show_dictionary::Show("SHOW_TEMPORARY_TABLES")
{
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("RECORDS", plugin::TableFunction::NUMBER, 0, false);
  add_field("RECORD_LENGTH", plugin::TableFunction::NUMBER, 0, false);
  add_field("ENGINE");
}

ShowTemporaryTables::Generator::Generator(Field **arg) :
  show_dictionary::Show::Generator(arg),
  session(getSession())
{
  table= session.open_tables.getTemporaryTables();
}

bool ShowTemporaryTables::Generator::populate()
{
  while (table)
  {
    if (not isWild(table->getShare()->getTableName()))
    {
      break;
    }
    table= table->getNext();
  }

  if (not table)
    return false;

  fill();

  table= table->getNext();

  return true;
}

void ShowTemporaryTables::Generator::fill()
{
  /* TABLE_SCHEMA */
  push(table->getShare()->getSchemaName());

  /* TABLE_NAME */
  push(table->getShare()->getTableName());

  /* RECORDS */
  push(static_cast<uint64_t>(table->getCursor().records()));

  /* RECORD_LENGTH */
  push(static_cast<uint64_t>(table->getRecordLength()));

  /* ENGINE */
  push(table->getEngine()->getName());
}
