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

IndexesTool::IndexesTool() :
  Tool("INDEXES")
{
  add_field("TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field("TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_NAME", message::Table::Field::VARCHAR, 64);
  add_field("INDEX_NAME", message::Table::Field::VARCHAR, 64);
  add_field("IS_PRIMARY", message::Table::Field::VARCHAR, 5);
  add_field("IS_UNIQUE", message::Table::Field::VARCHAR, 5);
  add_field("IS_NULLABLE", message::Table::Field::VARCHAR, 5);
  add_field("KEY_LENGTH", message::Table::Field::BIGINT);
  add_field("INDEX_TYPE", message::Table::Field::VARCHAR, 16);
  add_field("INDEX_COMMENT", message::Table::Field::VARCHAR, 1024);
}

IndexesTool::Generator::Generator() :
  schema_counter(0),
  index_iterator(0)
{
  plugin::StorageEngine::getSchemaNames(schema_names);

  schema_iterator= schema_names.begin(); // Prime the schema list.

  fetch_tables();
  fetch_proto();
}

bool IndexesTool::Generator::populate(Field ** fields)
{
  while (schema_iterator != schema_names.end())
  {
    while (table_iterator != table_names.end())
    {
      for (; index_iterator < table_proto.indexes_size(); index_iterator++)
      {
        const message::Table::Index index= table_proto.indexes(index_iterator);

        fill(fields, index);
        index_iterator++;

        return true;
      }

      index_iterator= 0;
      table_iterator++;

      if (table_iterator == table_names.end())
        break;

      fetch_proto();
    }

    schema_iterator++;
    if (schema_iterator == schema_names.end())
      break;

    fetch_tables();
    fetch_proto();
  }

  return false;
}

void IndexesTool::Generator::fetch_tables()
{
  if (schema_iterator != schema_names.end())
  {
    table_names.clear();
    plugin::StorageEngine::getTableNames(schema_name(), table_names); // Prime up the table names
    table_iterator= table_names.begin(); // Prime the table iterator
  }
}

extern size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);

void IndexesTool::Generator::fetch_proto()
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

void IndexesTool::Generator::fill(Field ** fields, const message::Table::Index &index)
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

  /* INDEX_NAME */
  (*field)->store(index.name().c_str(), index.name().length(), scs);
  field++;

  /* PRIMARY */
  populateBoolean(field, index.is_primary());
  field++;

  /* UNIQUE */
  populateBoolean(field, index.is_unique());
  field++;

  /* NULLABLE */
  populateBoolean(field, index.options().null_part_key());
  field++;

  /* KEY_LENGTH */
  (*field)->store(index.key_length());
  field++;

  /* INDEX_TYPE */
  {
    const char *str;
    uint32_t length;

    switch (index.type())
    {
    default:
    case message::Table::Index::UNKNOWN_INDEX:
      str= "UNKNOWN";
      length= sizeof("UNKNOWN");
      break;
    case message::Table::Index::BTREE:
      str= "BTREE";
      length= sizeof("BTREE");
      break;
    case message::Table::Index::RTREE:
      str= "RTREE";
      length= sizeof("RTREE");
      break;
    case message::Table::Index::HASH:
      str= "HASH";
      length= sizeof("HASH");
      break;
    case message::Table::Index::FULLTEXT:
      str= "FULLTEXT";
      length= sizeof("FULLTEXT");
      break;
    }
    (*field)->store(str, length, scs);
  }
  field++;

 /* "INDEX_COMMENT" */
  (*field)->store(index.comment().c_str(),
                  index.comment().length(),
                  scs);
}


IndexDefinitionTool::IndexDefinitionTool() :
  Tool("INDEX_DEFINITIONS")
{
  add_field("TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field("TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_NAME", message::Table::Field::VARCHAR, 64);
  add_field("INDEX_NAME", message::Table::Field::VARCHAR, 64);
  add_field("COLUMN_NAME", message::Table::Field::VARCHAR, 64);
  add_field("COLUMN_NUMBER", message::Table::Field::BIGINT);
  add_field("COMPARE_LENGTH", message::Table::Field::BIGINT);
  add_field("REVERSE_ORDER", message::Table::Field::VARCHAR, 5);
}

IndexDefinitionTool::Generator::Generator() :
  schema_counter(0),
  index_iterator(0),
  component_iterator(0)
{
  plugin::StorageEngine::getSchemaNames(schema_names);

  schema_iterator= schema_names.begin(); // Prime the schema list.

  fetch_tables();
  fetch_proto();
}

bool IndexDefinitionTool::Generator::populate(Field ** fields)
{
  while (schema_iterator != schema_names.end())
  {
    while (table_iterator != table_names.end())
    {
      for (; index_iterator < table_proto.indexes_size(); index_iterator++)
      {
        const message::Table::Index index= table_proto.indexes(index_iterator);

        for (; component_iterator < index.index_part_size(); component_iterator++)
        {
          const drizzled::message::Table::Index::IndexPart part= index.index_part(component_iterator);

          fill(fields, index, part);
          component_iterator++;

          return true;
        }
        component_iterator= 0;
        index_iterator++;
      }

      index_iterator= 0;

      table_iterator++;
      if (table_iterator == table_names.end())
        break;

      fetch_proto();
    }

    schema_iterator++;
    if (schema_iterator == schema_names.end())
      break;

    fetch_tables();
    fetch_proto();
  }

  return false;
}

void IndexDefinitionTool::Generator::fetch_tables()
{
  if (schema_iterator != schema_names.end())
  {
    table_names.clear();
    plugin::StorageEngine::getTableNames(schema_name(), table_names); // Prime up the table names
    table_iterator= table_names.begin(); // Prime the table iterator
  }
}

extern size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);

void IndexDefinitionTool::Generator::fetch_proto()
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

void IndexDefinitionTool::Generator::fill(Field **fields,
                                          const drizzled::message::Table::Index &index,
                                          const drizzled::message::Table::Index::IndexPart &part)
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

  /* INDEX_NAME */
  (*field)->store(index.name().c_str(), index.name().length(), scs);
  field++;

  /* COLUMN_NAME */
  const message::Table::Field column= table_proto.field(part.fieldnr());
  (*field)->store(column.name().c_str(), column.name().length(), scs);
  field++;

  /* COLUMN_NUMBER */
  (*field)->store(part.fieldnr());
  field++;

  /* COMPARE_LENGTH */
  (*field)->store(part.compare_length());
  field++;

  /* REVERSE_ORDER */
  populateBoolean(field, part.in_reverse_order());
}
