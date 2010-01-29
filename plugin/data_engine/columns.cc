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
#include <assert.h>

using namespace std;
using namespace drizzled;

extern size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);


ColumnsTool::ColumnsTool() :
  Tool("COLUMNS")
{
  add_field("TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field("TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_NAME", message::Table::Field::VARCHAR, 64);

  add_field("COLUMN_NAME", message::Table::Field::VARCHAR, 64);
  add_field("ORDINAL_POSITION", message::Table::Field::BIGINT);
  add_field("COLUMN_DEFAULT", message::Table::Field::VARCHAR, 64);
  add_field("IS_NULLABLE", message::Table::Field::VARCHAR, 5);
  add_field("DATATYPE", message::Table::Field::VARCHAR, 64);

  add_field("CHARACTER_MAXIMUM_LENGTH", message::Table::Field::BIGINT);
  add_field("CHARACTER_OCTET_LENGTH", message::Table::Field::BIGINT);
  add_field("NUMERIC_PRECISION", message::Table::Field::BIGINT);
  add_field("NUMERIC_SCALE", message::Table::Field::BIGINT);

  add_field("COLLATION_NAME", message::Table::Field::VARCHAR, 64);

  add_field("COLUMN_COMMENT", message::Table::Field::VARCHAR, 1024);
}

ColumnsTool::Generator::Generator() :
  schema_counter(0),
  column_iterator(0),
  primed(false)
{
  plugin::StorageEngine::getSchemaNames(schema_names);
  schema_iterator= schema_names.begin();

  fetch_tables();
}

bool ColumnsTool::Generator::populate(Field ** fields)
{
  while (schema_iterator != schema_names.end())
  {
    while (table_iterator != table_names.end())
    {
      while (column_iterator < table_proto.field_size())
      {
        const message::Table::Field column= table_proto.field(column_iterator);

        fill(fields, column);
        column_iterator++;

        return true;
      }

      table_iterator++;

      if (table_iterator != table_names.end())
        fetch_proto();
    }

    fetch_tables();

    if (schema_iterator == schema_names.end())
      break;

    fetch_proto();
  }

  return false;
}

void ColumnsTool::Generator::fetch_tables()
{
  do
  {
    if (primed)
    {
      schema_iterator++;
      if (schema_iterator == schema_names.end())
        throw true;
    }
    else
    {
      schema_iterator= schema_names.begin();
      primed= true;
    }


    table_names.clear();
    plugin::StorageEngine::getTableNames(schema_name(), table_names); // Prime up the table names
    table_iterator= table_names.begin(); // Prime the table iterator

  } while (table_iterator == table_names.end());

  fetch_proto();
}

void ColumnsTool::Generator::fetch_proto()
{
  Session *session= current_session;
  char path[FN_REFLEN];

  assert(schema_name().c_str());
  assert(table_name().c_str());
  build_table_filename(path, sizeof(path), schema_name().c_str(), table_name().c_str(), false);

  plugin::StorageEngine::getTableDefinition(*session,
                                            path,
                                            schema_name().c_str(),
                                            table_name().c_str(),
                                            false,
                                            &table_proto);
  column_iterator= 0;
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
  populateBoolean(field, column.constraints().is_nullable());
  field++;

  /* DATATYPE */
  populateFieldType(field, column.type());
  field++;

 /* "CHARACTER_MAXIMUM_LENGTH" */
  (*field)->store(column.string_options().length());
  field++;

 /* "CHARACTER_OCTET_LENGTH" */
  (*field)->store(column.string_options().length() * 4);
  field++;

 /* "NUMERIC_PRECISION" */
  (*field)->store(column.numeric_options().precision());
  field++;

 /* "NUMERIC_SCALE" */
  (*field)->store(column.numeric_options().scale());
  field++;

 /* "COLLATION_NAME" */
  (*field)->store(column.string_options().collation().c_str(),
                  column.string_options().collation().length(),
                  scs);
  field++;

 /* "COLUMN_COMMENT" */
  (*field)->store(column.comment().c_str(),
                  column.comment().length(),
                  scs);
  field++;
}
