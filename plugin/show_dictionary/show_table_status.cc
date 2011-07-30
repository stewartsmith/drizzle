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
#include <drizzled/table/cache.h>
#include <drizzled/pthread_globals.h>

using namespace drizzled;
using namespace std;

ShowTableStatus::ShowTableStatus() :
  show_dictionary::Show("SHOW_TABLE_STATUS")
{
  add_field("Session", plugin::TableFunction::NUMBER, 0, false);
  add_field("Schema");
  add_field("Name");
  add_field("Type");
  add_field("Engine");
  add_field("Version");
  add_field("Rows");
  add_field("Avg_row_length");
  add_field("Table_size");
  add_field("Auto_increment");
}

ShowTableStatus::Generator::Generator(drizzled::Field **arg) :
  show_dictionary::Show::Generator(arg),
  is_primed(false),
  scopedLock(table::Cache::mutex())
{
  if (not isShowQuery())
   return;

  statement::Show& select= static_cast<statement::Show&>(statement());

  schema_predicate.append(select.getShowSchema());

  util::string::ptr schema(getSession().schema());
  if (schema_predicate.empty() and schema)
  {
    schema_predicate.append(*schema);
  }

  if (not schema_predicate.empty())
  {
    table::CacheMap &open_cache(table::getCache());

    for (table::CacheMap::const_iterator iter= open_cache.begin();
         iter != open_cache.end();
         iter++)
    {
      table_list.push_back(iter->second);
    }

    for (drizzled::Table *tmp_table= getSession().open_tables.getTemporaryTables(); tmp_table; tmp_table= tmp_table->getNext())
    {
      if (tmp_table->getShare())
      {
        table_list.push_back(tmp_table);
      }
    }
    std::sort(table_list.begin(), table_list.end(), Table::compare);
  }
}

ShowTableStatus::Generator::~Generator()
{
}

bool ShowTableStatus::Generator::nextCore()
{
  if (is_primed)
  {
    table_list_iterator++;
  }
  else
  {
    is_primed= true;
    table_list_iterator= table_list.begin();
  }

  if (table_list_iterator == table_list.end())
    return false;

  table= *table_list_iterator;

  if (checkSchemaName())
    return false;

  return true;
}

bool ShowTableStatus::Generator::next()
{
  while (not nextCore())
  {
    if (table_list_iterator != table_list.end())
      continue;

    return false;
  }

  return true;
}

bool ShowTableStatus::Generator::checkSchemaName()
{
  if (not schema_predicate.empty() && schema_predicate.compare(schema_name()))
    return true;

  return false;
}

const char *ShowTableStatus::Generator::schema_name()
{
  return table->getShare()->getSchemaName();
}

bool ShowTableStatus::Generator::populate()
{
  if (not next())
    return false;

  fill();

  return true;
}

void ShowTableStatus::Generator::fill()
{
  /**
    For test cases use:
    --replace_column 1 #  6 # 7 # 8 # 9 # 10 #
  */

  /* Session 1 */
  if (table->getSession())
    push(table->getSession()->getSessionId());
  else
    push(static_cast<int64_t>(0));

  /* Schema 2 */
  push(table->getShare()->getSchemaName());

  /* Name  3 */
  push(table->getShare()->getTableName());

  /* Type  4 */
  push(table->getShare()->getTableTypeAsString());

  /* Engine 5 */
  push(table->getEngine()->getName());

  /* Version 6 */
  push(static_cast<int64_t>(table->getShare()->getVersion()));

  /* Rows 7 */
  push(static_cast<uint64_t>(table->getCursor().records()));

  /* Avg_row_length 8 */
  push(table->getCursor().rowSize());

  /* Table_size 9 */
  push(table->getCursor().tableSize());

  /* Auto_increment 10 */
  bool session_set= false;
  if (table->in_use == NULL)
  {
    table->in_use= &getSession();
    session_set= true;
  }

  table->getCursor().info(HA_STATUS_AUTO);
  push(table->getCursor().getAutoIncrement());

  if (session_set)
    table->in_use= NULL;
}
