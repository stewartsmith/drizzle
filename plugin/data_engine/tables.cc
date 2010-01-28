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

TablesTool::TablesTool() :
  Tool("TABLES")
{
  add_field("TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field("TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_NAME", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_TYPE", message::Table::Field::VARCHAR, 64);
  add_field("ENGINE", message::Table::Field::VARCHAR, 64);
  add_field("VERSION", message::Table::Field::BIGINT);
  add_field("ROW_FORMAT", message::Table::Field::VARCHAR, 10);
  add_field("TABLE_ROWS", message::Table::Field::BIGINT);
  add_field("AVG_ROW_LENGTH", message::Table::Field::BIGINT);
  add_field("DATA_LENGTH", message::Table::Field::BIGINT);
  add_field("MAX_DATA_LENGTH", message::Table::Field::BIGINT);
  add_field("INDEX_LENGTH", message::Table::Field::BIGINT);
  add_field("DATA_FREE", message::Table::Field::BIGINT);
  add_field("AUTO_INCREMENT", message::Table::Field::BIGINT);
  add_field("CREATE_TIME", message::Table::Field::BIGINT);
  add_field("UPDATE_TIME", message::Table::Field::BIGINT);
  add_field("CHECK_TIME", message::Table::Field::BIGINT);
  add_field("TABLE_COLLATION", message::Table::Field::VARCHAR, 64);
  add_field("CHECKSUM", message::Table::Field::BIGINT);
  add_field("CREATE_OPTIONS", message::Table::Field::VARCHAR, 255);
  add_field("TABLE_COMMENT", message::Table::Field::VARCHAR, 2048);
  add_field("PLUGIN_NAME", message::Table::Field::VARCHAR, 64);
}

TablesTool::Generator::Generator() :
  schema_counter(0),
  table_counter(0)
{
  plugin::StorageEngine::getSchemaNames(schema_names);

  schema_iterator= schema_names.begin(); // Prime it to begin()
  table_iterator= table_names.begin(); // Prime it to null
}

bool TablesTool::Generator::populate(Field ** fields)
{
  if (table_iterator == table_names.end())
  {
    // If no tables exist in the schema we just loop over it
    do {
      /* If we are done with schema we have nothing else to return. */
      if (schema_counter)
        schema_iterator++;

      if (schema_iterator == schema_names.end())
        return false;

      db_name= *schema_iterator;
      table_names.clear();
      plugin::StorageEngine::getTableNames(db_name, table_names);
      table_iterator= table_names.begin();
      schema_counter++;
    } while (table_iterator == table_names.end());
  }

  tb_name= *table_iterator;
  bool rc= fill(fields);
  table_iterator++;

  return rc;
}

bool TablesTool::Generator::fill(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  /* TABLE_SCHEMA */
  (*field)->store(schema_name().c_str(), schema_name().length(), scs);
  field++;

  /* TABLE_NAME */
  (*field)->store(table_name().c_str(), table_name().length(), scs);
  field++;

  for (; *field ; field++)
  {
    (*field)->store("<not implemented>", sizeof("<not implemented>"), scs);
  }

  return true;
}

TablesNameTool::TablesNameTool() :
  TablesTool("TABLE_NAMES")
{
  add_field("TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_NAME", message::Table::Field::VARCHAR, 64);
}

bool TablesNameTool::Generator::fill(Field **fields) 
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  (*field)->store(schema_name().c_str(), schema_name().length(), scs);
  field++;

  (*field)->store(table_name().c_str(), table_name().length(), scs);

  return true;
}

TablesInfoTool::TablesInfoTool() :
  TablesTool("TABLE_INFO")
{
  add_field("TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_NAME", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_TYPE", message::Table::Field::VARCHAR, 64);
  add_field("ENGINE", message::Table::Field::VARCHAR, 64);
  add_field("ROW_FORMAT", message::Table::Field::VARCHAR, 10);
  add_field("TABLE_COLLATION", message::Table::Field::VARCHAR, 64);
  add_field("TABLE_COMMENT", message::Table::Field::VARCHAR, 2048);
}

extern size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp);

bool TablesInfoTool::Generator::fill(Field **fields) 
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;
  message::Table table_proto;
  Session *session= current_session;
  char path[FN_REFLEN];
  build_table_filename(path, sizeof(path), schema_name().c_str(), table_name().c_str(), false);
  plugin::StorageEngine::getTableDefinition(*session,
                                            path,
                                            schema_name().c_str(), table_name().c_str(),
                                            false,
                                            &table_proto);

  /* TABLE_SCHEMA */
  (*field)->store(schema_name().c_str(), schema_name().length(), scs);
  field++;

  /* TABLE_NAME */
  (*field)->store(table_name().c_str(), table_name().length(), scs);
  field++;

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
    (*field)->store(str, strlen(str), scs);
    field++;
  }

  /* ENGINE */
  (*field)->store(table_proto.engine().name().c_str(),
                  table_proto.engine().name().length(), scs);
  field++;

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

    (*field)->store(str, strlen(str), scs);
  }
  field++;

  /* TABLE_COLLATION */
  (*field)->store(table_proto.options().collation().c_str(),
                  table_proto.options().collation().length(), scs);
  field++;

  /* TABLE_COMMENT */
  (*field)->store(table_proto.options().comment().c_str(),
                  table_proto.options().comment().length(), scs);

  return true;
}
