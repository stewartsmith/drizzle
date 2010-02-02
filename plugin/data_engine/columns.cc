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
#include <assert.h>

using namespace std;
using namespace drizzled;


ColumnsTool::ColumnsTool() :
  TablesTool("COLUMNS")
{
  add_field("TABLE_SCHEMA");
  add_field("TABLE_NAME");

  add_field("COLUMN_NAME");
  add_field("ORDINAL_POSITION", Tool::NUMBER);
  add_field("COLUMN_DEFAULT");
  add_field("IS_NULLABLE", Tool::BOOLEAN);
  add_field("DATATYPE");

  add_field("CHARACTER_MAXIMUM_LENGTH", Tool::NUMBER);
  add_field("CHARACTER_OCTET_LENGTH", Tool::NUMBER);
  add_field("NUMERIC_PRECISION", Tool::NUMBER);
  add_field("NUMERIC_SCALE", Tool::NUMBER);

  add_field("COLLATION_NAME");
  add_field("COLUMN_COMMENT", 1024);
}


ColumnsTool::Generator::Generator(Field **arg) :
  TablesTool::Generator(arg),
  column_iterator(0),
  is_columns_primed(false)
{
}


bool ColumnsTool::Generator::nextColumnCore()
{
  if (is_columns_primed)
  {
    column_iterator++;
  }
  else
  {
    if (not isTablesPrimed())
      return false;

    column_iterator= 0;
    is_columns_primed= true;
  }

  if (column_iterator >= getTableProto().field_size())
    return false;

  column= getTableProto().field(column_iterator);

  return true;
}


bool ColumnsTool::Generator::nextColumn()
{
  while (not nextColumnCore())
  {
    if (not nextTable())
      return false;
    is_columns_primed= false;
  }

  return true;
}

bool ColumnsTool::Generator::populate()
{
  if (not nextColumn())
    return false;

  fill();

  return true;
}

void ColumnsTool::Generator::fill()
{
  /* TABLE_SCHEMA */
  push(schema_name());

  /* TABLE_NAME */
  push(table_name());

  /* COLUMN_NAME */
  push(column.name());

  /* ORDINAL_POSITION */
  push(column_iterator);

  /* COLUMN_DEFAULT */
  push(column.options().default_value());

  /* IS_NULLABLE */
  push(column.constraints().is_nullable());

  /* DATATYPE */
  push(column.type());

 /* "CHARACTER_MAXIMUM_LENGTH" */
  push(static_cast<int64_t>(column.string_options().length()));

 /* "CHARACTER_OCTET_LENGTH" */
  push(static_cast<int64_t>(column.string_options().length()) * 4);

 /* "NUMERIC_PRECISION" */
  push(static_cast<int64_t>(column.numeric_options().precision()));

 /* "NUMERIC_SCALE" */
  push(static_cast<int64_t>(column.numeric_options().scale()));

 /* "COLLATION_NAME" */
  push(column.string_options().collation());

 /* "COLUMN_COMMENT" */
  push(column.comment());
}
