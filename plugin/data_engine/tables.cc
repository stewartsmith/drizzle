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

#include <plugin/data_engine/function.h>
#include <drizzled/charset.h>

extern size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);

using namespace std;
using namespace drizzled;

TablesTool::TablesTool() :
  SchemasTool("TABLES")
{
  add_field("TABLE_SCHEMA");
  add_field("TABLE_NAME");
  add_field("TABLE_TYPE");
  add_field("ENGINE");
  add_field("ROW_FORMAT", 10);
  add_field("TABLE_COLLATION");
  add_field("TABLE_COMMENT", 2048);
}

TablesTool::Generator::Generator(Field **arg) :
  SchemasTool::Generator(arg),
  is_tables_primed(false)
{
}

bool TablesTool::Generator::nextTableCore()
{
  if (is_tables_primed)
  {
    table_iterator++;
  }
  else
  {
    if (not isSchemaPrimed())
     return false;

    table_names.clear();
    plugin::StorageEngine::getTableNames(schema_name(), table_names);
    table_iterator= table_names.begin();
    is_tables_primed= true;
  }

  if (table_iterator == table_names.end())
    return false;

  table_proto.Clear();
  {
    Session *session= current_session;
    char path[FN_REFLEN];
    build_table_filename(path, sizeof(path), schema_name().c_str(), table_name().c_str(), false);
    plugin::StorageEngine::getTableDefinition(*session,
                                             path,
                                             schema_name().c_str(),
                                             table_name().c_str(),
                                             false,
                                             &table_proto);
  }

  return true;
}

bool TablesTool::Generator::nextTable()
{
  while (not nextTableCore())
  {
    if (not nextSchema())
      return false;
    is_tables_primed= false;
  }

  return true;
}

bool TablesTool::Generator::populate(Field **)
{
  if (not nextTable())
    return false;

  fill();

  return true;
}

void TablesTool::Generator::fill()
{

  /* TABLE_SCHEMA */
  push(schema_name());

  /* TABLE_NAME */
  push(table_name());

  /* TABLE_TYPE */
  {
    const char *str;

    switch (table_proto.type())
    {
    default:
    case message::Table::STANDARD:
      str= "STANDARD";
      break;
    case message::Table::TEMPORARY:
      str= "TEMPORARY";
      break;
    case message::Table::INTERNAL:
      str= "INTERNAL";
      break;
    case message::Table::FUNCTION:
      str= "FUNCTION";
      break;
    }
    push(str);
  }

  /* ENGINE */
  push(table_proto.engine().name());

  /* ROW_FORMAT */
  {
    const char *str;

    switch (table_proto.options().row_type())
    {
    default:
    case message::Table::TableOptions::ROW_TYPE_DEFAULT:
      str= "DEFAULT";
      break;
    case message::Table::TableOptions::ROW_TYPE_FIXED:
      str= "FIXED";
      break;
    case message::Table::TableOptions::ROW_TYPE_DYNAMIC:
      str= "DYNAMIC";
      break;
    case message::Table::TableOptions::ROW_TYPE_COMPRESSED:
      str= "COMPRESSED";
      break;
    case message::Table::TableOptions::ROW_TYPE_REDUNDANT:
      str= "REDUNDANT";
      break;
    case message::Table::TableOptions::ROW_TYPE_COMPACT:
      str= "COMPACT";
      break;
    case message::Table::TableOptions::ROW_TYPE_PAGE:
      str= "PAGE";
      break;
    }
    message::Table::TableOptions options= table_proto.options();

    push(str);
  }

  /* TABLE_COLLATION */
  push(table_proto.options().collation());

  /* TABLE_COMMENT */
  push(table_proto.options().comment());
}
