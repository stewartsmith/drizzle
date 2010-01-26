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

StatisticsTool::StatisticsTool() :
  Tool("STATISTICS")
{
  add_field(schema, "TABLE_CATALOG", message::Table::Field::VARCHAR, 512);
  add_field(schema, "TABLE_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "TABLE_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "NON_UNIQUE", message::Table::Field::BIGINT);
  add_field(schema, "INDEX_SCHEMA", message::Table::Field::VARCHAR, 64);
  add_field(schema, "INDEX_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "SEQ_IN_INDEX", message::Table::Field::BIGINT);
  add_field(schema, "COLUMN_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "COLLATION", message::Table::Field::VARCHAR, 1);
  add_field(schema, "CARDINALITY", message::Table::Field::BIGINT);
  add_field(schema, "SUB_PART", message::Table::Field::BIGINT);
  add_field(schema, "PACKED", message::Table::Field::VARCHAR, 10);
  add_field(schema, "NULLABLE", message::Table::Field::VARCHAR, 3);
  add_field(schema, "INDEX_TYPE", message::Table::Field::VARCHAR, 16);
  add_field(schema, "COMMENT", message::Table::Field::VARCHAR, 16);
  add_field(schema, "INDEX_COMMENT", message::Table::Field::VARCHAR, 1024);
}
