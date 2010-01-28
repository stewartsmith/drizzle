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
  add_field("COLUMN_TYPE", message::Table::Field::VARCHAR, 64);
  add_field("COLUMN_KEY", message::Table::Field::VARCHAR, 3);
  add_field("EXTRA", message::Table::Field::VARCHAR, 27);
  add_field("PRIVILEGES", message::Table::Field::VARCHAR, 80);
  add_field("COLUMN_COMMENT", message::Table::Field::VARCHAR, 1024);
  add_field("STORAGE", message::Table::Field::VARCHAR, 8);
  add_field("FORMAT", message::Table::Field::VARCHAR, 8);
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

  for (; *field ; field++)
  {
    (*field)->store("<not implemented>", sizeof("<not implemented>"), scs);
  }
}
