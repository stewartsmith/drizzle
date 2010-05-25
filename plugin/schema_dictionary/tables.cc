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
#include "drizzled/table_identifier.h"

using namespace std;
using namespace drizzled;

static const string STANDARD("STANDARD");
static const string TEMPORARY("TEMPORARY");
static const string INTERNAL("INTERNAL");
static const string FUNCTION("FUNCTION");

static const string DEFAULT("DEFAULT");
static const string FIXED("FIXED");
static const string DYNAMIC("DYNAMIC");
static const string COMPRESSED("COMPRESSED");
static const string REDUNDANT("REDUNDANT");
static const string COMPACT("COMPACT");
static const string PAGE("PAGE");

static const string VARCHAR("VARCHAR");
static const string DOUBLE("DOUBLE");
static const string BLOB("BLOB");
static const string ENUM("ENUM");
static const string INTEGER("INTEGER");
static const string BIGINT("BIGINT");
static const string DECIMAL("DECIMAL");
static const string DATE("DATE");
static const string TIMESTAMP("TIMESTAMP");
static const string DATETIME("DATETIME");

TablesTool::TablesTool() :
  SchemasTool("TABLES")
{
  add_field("TABLE_SCHEMA");
  add_field("TABLE_NAME");
  add_field("TABLE_TYPE");
  add_field("ENGINE");
  add_field("ROW_FORMAT", 10);
  add_field("TABLE_COLLATION");
  add_field("TABLE_CREATION_TIME");
  add_field("TABLE_UPDATE_TIME");
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
    SchemaIdentifier identifier(schema_name());
    plugin::StorageEngine::getTableNames(getSession(), identifier, table_names);
    table_iterator= table_names.begin();
    is_tables_primed= true;
  }

  if (table_iterator == table_names.end())
    return false;

  table_proto.Clear();
  {
    TableIdentifier identifier(schema_name().c_str(), table_name().c_str());
    plugin::StorageEngine::getTableDefinition(getSession(),
                                             identifier,
                                             table_proto);
  }

  return true;
}

bool TablesTool::Generator::nextTable()
{
  while (not nextTableCore())
  {

    if (is_tables_primed && table_iterator != table_names.end())
      continue;

    if (not nextSchema())
      return false;

    is_tables_primed= false;
  }

  return true;
}

bool TablesTool::Generator::populate()
{
  if (not nextTable())
    return false;

  fill();

  return true;
}

void TablesTool::Generator::pushRow(message::Table::TableOptions::RowType type)
{
  switch (type)
  {
  default:
  case message::Table::TableOptions::ROW_TYPE_DEFAULT:
    push(DEFAULT);
    break;
  case message::Table::TableOptions::ROW_TYPE_FIXED:
    push(FIXED);
    break;
  case message::Table::TableOptions::ROW_TYPE_DYNAMIC:
    push(DYNAMIC);
    break;
  case message::Table::TableOptions::ROW_TYPE_COMPRESSED:
    push(COMPRESSED);
    break;
  case message::Table::TableOptions::ROW_TYPE_REDUNDANT:
    push(REDUNDANT);
    break;
  case message::Table::TableOptions::ROW_TYPE_COMPACT:
    push(COMPACT);
    break;
  case message::Table::TableOptions::ROW_TYPE_PAGE:
    push(PAGE);
    break;
  }
}

void TablesTool::Generator::pushType(message::Table::Field::FieldType type)
{
  switch (type)
  {
  default:
  case message::Table::Field::VARCHAR:
    push(VARCHAR);
    break;
  case message::Table::Field::DOUBLE:
    push(DOUBLE);
    break;
  case message::Table::Field::BLOB:
    push(BLOB);
    break;
  case message::Table::Field::ENUM:
    push(ENUM);
    break;
  case message::Table::Field::INTEGER:
    push(INTEGER);
    break;
  case message::Table::Field::BIGINT:
    push(BIGINT);
    break;
  case message::Table::Field::DECIMAL:
    push(DECIMAL);
    break;
  case message::Table::Field::DATE:
    push(DATE);
    break;
  case message::Table::Field::TIMESTAMP:
    push(TIMESTAMP);
    break;
  case message::Table::Field::DATETIME:
    push(DATETIME);
    break;
  }
}

void TablesTool::Generator::fill()
{

  /**
    @note use --replace-column
  */

  /* TABLE_SCHEMA */
  push(table_proto.schema());

  /* TABLE_NAME */
  push(table_proto.name());

  /* TABLE_TYPE */
  {
    switch (table_proto.type())
    {
    default:
    case message::Table::STANDARD:
      push(STANDARD);
      break;
    case message::Table::TEMPORARY:
      push(TEMPORARY);
      break;
    case message::Table::INTERNAL:
      push(INTERNAL);
      break;
    case message::Table::FUNCTION:
      push(FUNCTION);
      break;
    }
  }

  /* ENGINE */
  push(table_proto.engine().name());

  /* ROW_FORMAT */
  pushRow(table_proto.options().row_type());

  /* TABLE_COLLATION */
  push(table_proto.options().collation());

  /* TABLE_CREATION_TIME */
  time_t time_arg= table_proto.creation_timestamp();
  char buffer[40];
  struct tm tm_buffer;

  localtime_r(&time_arg, &tm_buffer);
  strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
  push(buffer);

  /* TABLE_UPDATE_TIME */
  time_arg= table_proto.update_timestamp();
  localtime_r(&time_arg, &tm_buffer);
  strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
  push(buffer);

  /* TABLE_COMMENT */
  push(table_proto.options().comment());
}
