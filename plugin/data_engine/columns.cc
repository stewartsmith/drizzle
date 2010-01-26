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
