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

using namespace std;
using namespace drizzled;


ColumnsTool::ColumnsTool() :
  TablesTool("COLUMNS")
{
  add_field("TABLE_SCHEMA");
  add_field("TABLE_NAME");

  add_field("COLUMN_NAME");
  add_field("COLUMN_TYPE");
  add_field("ORDINAL_POSITION", plugin::TableFunction::NUMBER, 0, false);
  add_field("COLUMN_DEFAULT", plugin::TableFunction::VARBINARY, 65535, true);
  add_field("COLUMN_DEFAULT_IS_NULL", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("COLUMN_DEFAULT_UPDATE");
  add_field("IS_AUTO_INCREMENT", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_NULLABLE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_INDEXED", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_USED_IN_PRIMARY", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_UNIQUE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_MULTI", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_FIRST_IN_MULTI", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("INDEXES_FOUND_IN", plugin::TableFunction::NUMBER, 0, false);
  add_field("DATA_TYPE");

  add_field("CHARACTER_MAXIMUM_LENGTH", plugin::TableFunction::NUMBER);
  add_field("CHARACTER_OCTET_LENGTH", plugin::TableFunction::NUMBER);
  add_field("NUMERIC_PRECISION", plugin::TableFunction::NUMBER);
  add_field("NUMERIC_SCALE", plugin::TableFunction::NUMBER);

  add_field("ENUM_VALUES", plugin::TableFunction::STRING, 1024, true);

  add_field("COLLATION_NAME");

  add_field("COLUMN_COMMENT", plugin::TableFunction::STRING, 1024, true);
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
  assert(getTableProto().schema().length());
  assert(getTableProto().schema().c_str());
  push(getTableProto().schema());

  /* TABLE_NAME */
  push(getTableProto().name());

  /* COLUMN_NAME */
  push(column.name());

  /* COLUMN_TYPE */
  push(drizzled::message::type(column.type()));

  /* ORDINAL_POSITION */
  push(static_cast<int64_t>(column_iterator));

  /* COLUMN_DEFAULT */
  if (column.options().has_default_value())
  {
    push(column.options().default_value());
  }
  else if (column.options().has_default_bin_value())
  {
    push(column.options().default_bin_value().c_str(), column.options().default_bin_value().length());
  }
  else if (column.options().has_default_expression())
  {
    push(column.options().default_expression());
  }
  else
  {
    push();
  }

  /* COLUMN_DEFAULT_IS_NULL */
  push(column.options().default_null());

  /* COLUMN_DEFAULT_UPDATE */
  push(column.options().update_expression());

  /* IS_AUTO_INCREMENT */
  push(column.numeric_options().is_autoincrement());

  /* IS_NULLABLE */
  push(column.constraints().is_nullable());

  /* IS_INDEXED, IS_USED_IN_PRIMARY, IS_UNIQUE, IS_MULTI, IS_FIRST_IN_MULTI, INDEXES_FOUND_IN */
  bool is_indexed= false;
  bool is_primary= false;
  bool is_unique= false;
  bool is_multi= false;
  bool is_multi_first= false;
  int64_t indexes_found_in= 0;
  for (int32_t x= 0; x < getTableProto().indexes_size() ; x++)
  {
    drizzled::message::Table::Index index=
      getTableProto().indexes(x);

    for (int32_t y= 0; y < index.index_part_size() ; y++)
    {
      drizzled::message::Table::Index::IndexPart index_part=
        index.index_part(y);

      if (static_cast<int32_t>(index_part.fieldnr()) == column_iterator)
      {
        indexes_found_in++;
        is_indexed= true;

        if (index.is_primary())
          is_primary= true;

        if (index.is_unique())
          is_unique= true;

        if (index.index_part_size() > 1)
        {
          is_multi= true;

          if (y == 0)
            is_multi_first= true;
        }
      }
    }
  }
  /* ...IS_INDEXED, IS_USED_IN_PRIMARY, IS_UNIQUE, IS_MULTI, IS_FIRST_IN_MULTI, INDEXES_FOUND_IN */
  push(is_indexed);
  push(is_primary);
  push(is_unique);
  push(is_multi);
  push(is_multi_first);
  push(indexes_found_in);

  /* DATATYPE */
  push(drizzled::message::type(column.type()));

 /* "CHARACTER_MAXIMUM_LENGTH" */
  push(static_cast<int64_t>(column.string_options().length()));

 /* "CHARACTER_OCTET_LENGTH" */
  push(static_cast<int64_t>(column.string_options().length()) * 4);

 /* "NUMERIC_PRECISION" */
  push(static_cast<int64_t>(column.numeric_options().precision()));

 /* "NUMERIC_SCALE" */
  push(static_cast<int64_t>(column.numeric_options().scale()));

 /* "ENUM_VALUES" */
  if (column.type() == drizzled::message::Table::Field::ENUM)
  {
    string destination;
    size_t num_field_values= column.enumeration_values().field_value_size();
    for (size_t x= 0; x < num_field_values; ++x)
    {
      const string &type= column.enumeration_values().field_value(x);

      if (x != 0)
        destination.push_back(',');

      destination.push_back('\'');
      destination.append(type);
      destination.push_back('\'');
    }
    push(destination);
  }
  else
    push();

 /* "COLLATION_NAME" */
  push(column.string_options().collation());

 /* "COLUMN_COMMENT" */
  if (column.has_comment())
  {
    push(column.comment());
  }
  else
  {
    push();
  }
}
