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
  add_field(schema, "TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field(schema, "TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "TABLE_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "TABLE_TYPE", message::Table::Field::VARCHAR, 64);
  add_field(schema, "ENGINE", message::Table::Field::VARCHAR, 64);
  add_field(schema, "VERSION", message::Table::Field::BIGINT);
  add_field(schema, "ROW_FORMAT", message::Table::Field::VARCHAR, 10);
  add_field(schema, "TABLE_ROWS", message::Table::Field::BIGINT);
  add_field(schema, "AVG_ROW_LENGTH", message::Table::Field::BIGINT);
  add_field(schema, "DATA_LENGTH", message::Table::Field::BIGINT);
  add_field(schema, "MAX_DATA_LENGTH", message::Table::Field::BIGINT);
  add_field(schema, "INDEX_LENGTH", message::Table::Field::BIGINT);
  add_field(schema, "DATA_FREE", message::Table::Field::BIGINT);
  add_field(schema, "AUTO_INCREMENT", message::Table::Field::BIGINT);
  add_field(schema, "CREATE_TIME", message::Table::Field::BIGINT);
  add_field(schema, "UPDATE_TIME", message::Table::Field::BIGINT);
  add_field(schema, "CHECK_TIME", message::Table::Field::BIGINT);
  add_field(schema, "TABLE_COLLATION", message::Table::Field::VARCHAR, 64);
  add_field(schema, "CHECKSUM", message::Table::Field::BIGINT);
  add_field(schema, "CREATE_OPTIONS", message::Table::Field::VARCHAR, 255);
  add_field(schema, "TABLE_COMMENT", message::Table::Field::VARCHAR, 2048);
  add_field(schema, "PLUGIN_NAME", message::Table::Field::VARCHAR, 64);
}
