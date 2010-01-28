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

#include <plugin/data_engine/dictionary.h>
#include <drizzled/charset.h>

using namespace std;
using namespace drizzled;

ColumnsTool::ColumnsTool() :
  Tool("COLUMNS")
{
  add_field("TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field("TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_NAME", message::Table::Field::VARCHAR, 64);

  add_field("COLUMN_NAME", message::Table::Field::VARCHAR, 64);
  add_field("ORDINAL_POSITION", message::Table::Field::BIGINT);
  add_field("COLUMN_DEFAULT", message::Table::Field::VARCHAR, 64);
  add_field("IS_NULLABLE", message::Table::Field::VARCHAR, 3);
  add_field("DATATYPE", message::Table::Field::VARCHAR, 64);

  add_field("CHARACTER_MAXIMUM_LENGTH", message::Table::Field::BIGINT);
  add_field("CHARACTER_OCTET_LENGTH", message::Table::Field::BIGINT);
  add_field("NUMERIC_PRECISION", message::Table::Field::BIGINT);
  add_field("NUMERIC_SCALE", message::Table::Field::BIGINT);

  add_field("CHARACTER_SET_NAME", message::Table::Field::VARCHAR, 64);
  add_field("COLLATION_NAME", message::Table::Field::VARCHAR, 64);

  add_field("COLUMN_COMMENT", message::Table::Field::VARCHAR, 1024);
}

ColumnsTool::Generator::Generator() :
  schema_counter(0),
  column_iterator(0)
{
  plugin::StorageEngine::getSchemaNames(schema_names);

  schema_iterator= schema_names.begin(); // Prime the schema list.

  fetch_tables();
  fetch_proto();
}

bool ColumnsTool::Generator::populate(Field ** fields)
{
  while (schema_iterator != schema_names.end())
  {
    while (table_iterator != table_names.end())
    {
      for (; column_iterator < table_proto.field_size(); column_iterator++)
      {
        const message::Table::Field column= table_proto.field(column_iterator);
        fill(fields, column);

        column_iterator++;

        return true;
      }

      column_iterator= 0;
      table_iterator++;

      fetch_proto();
    }

    schema_iterator++;

    fetch_tables();
  }

  return false;
}

void ColumnsTool::Generator::fetch_tables()
{
  if (schema_iterator != schema_names.end())
  {
    table_names.clear();
    plugin::StorageEngine::getTableNames(schema_name(), table_names); // Prime up the table names
    table_iterator= table_names.begin(); // Prime the table iterator
  }
}

extern size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);

void ColumnsTool::Generator::fetch_proto()
{
  Session *session= current_session;
  char path[FN_REFLEN];

  if (table_iterator != table_names.end())
  {
    build_table_filename(path, sizeof(path), schema_name().c_str(), table_name().c_str(), false);

    plugin::StorageEngine::getTableDefinition(*session,
                                              path,
                                              schema_name().c_str(),
                                              table_name().c_str(),
                                              false,
                                              &table_proto);
  }
}

void ColumnsTool::Generator::fill(Field ** fields, const message::Table::Field &column)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  /* TABLE_CATALOG */
  (*field)->store("default", sizeof("default"), scs);
  field++;

  /* TABLE_SCHEMA */
  (*field)->store(schema_name().c_str(), schema_name().length(), scs);
  field++;

  /* TABLE_NAME */
  (*field)->store(table_name().c_str(), table_name().length(), scs);
  field++;

  /* COLUMN_NAME */
  (*field)->store(column.name().c_str(), column.name().length(), scs);
  field++;

  /* ORDINAL_POSITION */
  (*field)->store(column_iterator);
  field++;

  /* COLUMN_DEFAULT */
  (*field)->store(column.options().default_value().c_str(),
                  column.options().default_value().length(), scs);
  field++;

  /* IS_NULLABLE */
  {
    const char *yes= "YES";
    const char *no= "NO";

    uint32_t yes_length= sizeof("YES");
    uint32_t no_length= sizeof("YES");

    (*field)->store(column.constraints().is_nullable() ? yes : no,
                    column.constraints().is_nullable() ? yes_length : no_length,
                    scs);
    field++;
  }

  /* DATATYPE */
  {
    const char *str;
    uint32_t length;

    switch (column.type())
    {
    case message::Table::Field::DOUBLE:
      str= "DOUBLE";
      length= sizeof("DOUBLE");
      break;
    default:
    case message::Table::Field::VARCHAR:
      str= "VARCHAR";
      length= sizeof("VARCHAR");
      break;
    case message::Table::Field::BLOB:
      str= "BLOB";
      length= sizeof("BLOB");
      break;
    case message::Table::Field::ENUM:
      str= "ENUM";
      length= sizeof("ENUM");
      break;
    case message::Table::Field::INTEGER:
      str= "INT";
      length= sizeof("INT");
      break;
    case message::Table::Field::BIGINT:
      str= "BIGINT";
      length= sizeof("BIGINT");
      break;
    case message::Table::Field::DECIMAL:
      str= "DECIMAL";
      length= sizeof("DECIMAL");
      break;
    case message::Table::Field::DATE:
      str= "DATE";
      length= sizeof("DATE");
      break;
    case message::Table::Field::TIME:
      str= "TIME";
      length= sizeof("TIME");
      break;
    case message::Table::Field::TIMESTAMP:
      str= "TIMESTAMP";
      length= sizeof("TIMESTAMP");
      break;
    case message::Table::Field::DATETIME:
      str= "DATETIME";
      length= sizeof("DATETIME");
      break;
    }
    (*field)->store(str, length, scs);
    field++;
  }

 /* "CHARACTER_MAXIMUM_LENGTH" */
  (*field)->store(column.string_options().length());

 /* "CHARACTER_OCTET_LENGTH" */
  (*field)->store(column.string_options().length() * 4);

 /* "NUMERIC_PRECISION" */
  (*field)->store(column.numeric_options().precision());

 /* "NUMERIC_SCALE" */
  (*field)->store(column.numeric_options().scale());

 /* "CHARACTER_SET_NAME" */
  (*field)->store("UTF-8", sizeof("UTF-8"), scs);

 /* "COLLATION_NAME" */
  (*field)->store(column.string_options().collation().c_str(),
                  column.string_options().collation().length(),
                  scs);

 /* "COLUMN_COMMENT" */
  (*field)->store(column.comment().c_str(),
                  column.comment().length(),
                  scs);
}
