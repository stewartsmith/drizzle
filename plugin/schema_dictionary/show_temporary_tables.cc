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

#include "config.h"

#include "plugin/schema_dictionary/dictionary.h"

using namespace std;
using namespace drizzled;

ShowTemporaryTables::ShowTemporaryTables() :
  drizzled::plugin::TableFunction("DATA_DICTIONARY", "SHOW_TEMPORARY_TABLES")
{
  add_field("TABLE_SCHEMA");
  add_field("TABLE_NAME");
  add_field("RECORDS", plugin::TableFunction::NUMBER);
  add_field("RECORD_LENGTH", plugin::TableFunction::NUMBER);
  add_field("ENGINE");
}

ShowTemporaryTables::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  session= current_session;
  table= session->temporary_tables;
}

bool ShowTemporaryTables::Generator::populate()
{
  if (not table)
    return false;

  fill();

  table= table->next;

  return true;
}

void ShowTemporaryTables::Generator::fill()
{
  /* TABLE_SCHEMA */
  push(table->s->getSchemaName());

  /* TABLE_NAME */
  push(table->s->table_name.str);

  /* RECORDS */
  push(static_cast<uint64_t>(table->getCursor().records()));

  /* RECORD_LENGTH */
  push(static_cast<uint64_t>(table->getRecordLength()));

  /* ENGINE */
  push(table->getEngine()->getName());
}
