/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

/**
 * @file 
 *   Character Set I_S table methods.
 */

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/global_charset_info.h"


#include "helper_methods.h"
#include "columns.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the columns I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the columns I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * columns I_S table.
 */
static plugin::InfoSchemaTable *cols_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *ColumnsIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  /*
   * Create each column for the COLUMNS table.
   */
  columns->push_back(new plugin::ColumnInfo("TABLE_CATALOG",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("TABLE_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("TABLE_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("COLUMN_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Field"));

  columns->push_back(new plugin::ColumnInfo("ORDINAL_POSITION",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            MY_I_S_UNSIGNED,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("COLUMN_DEFAULT",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Default"));

  columns->push_back(new plugin::ColumnInfo("IS_NULLABLE",
                                            3,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Null"));

  columns->push_back(new plugin::ColumnInfo("DATA_TYPE",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("CHARACTER_MAXIMUM_LENGTH",
                                           MY_INT64_NUM_DECIMAL_DIGITS,
                                           DRIZZLE_TYPE_LONGLONG,
                                           0,
                                           (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                           ""));

  columns->push_back(new plugin::ColumnInfo("CHARACTER_OCTET_LENGTH",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            ""));

  columns->push_back(new plugin::ColumnInfo("NUMERIC_PRECISION",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            ""));

  columns->push_back(new plugin::ColumnInfo("NUMERIC_SCALE",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            ""));

  columns->push_back(new plugin::ColumnInfo("CHARACTER_SET_NAME",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("COLLATION_NAME",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Collation"));

  columns->push_back(new plugin::ColumnInfo("COLUMN_TYPE",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Type"));

  columns->push_back(new plugin::ColumnInfo("COLUMN_KEY",
                                            3,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Key"));

  columns->push_back(new plugin::ColumnInfo("EXTRA",
                                            27,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Extra"));

  columns->push_back(new plugin::ColumnInfo("PRIVILEGES",
                                            80,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Privileges"));

  columns->push_back(new plugin::ColumnInfo("COLUMN_COMMENT",
                                            COLUMN_COMMENT_MAXLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Comment"));

  columns->push_back(new plugin::ColumnInfo("STORAGE",
                                            8,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Storage"));

  columns->push_back(new plugin::ColumnInfo("FORMAT",
                                            8,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Format"));
  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *ColumnsIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new ColumnsISMethods();
  }

  if (cols_table == NULL)
  {
    cols_table= new plugin::InfoSchemaTable("COLUMNS",
                                            *columns,
                                            1, 2, false, true,
                                            0,
                                            methods);
  }

  return cols_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void ColumnsIS::cleanup()
{
  clearColumns(*columns);
  delete cols_table;
  delete methods;
  delete columns;
}

int ColumnsISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  int fields_arr[]= {3, 14, 13, 6, 15, 5, 16, 17, 18, -1};
  int *field_num= fields_arr;
  const drizzled::plugin::InfoSchemaTable::Columns tab_columns= schema_table->getColumns();
  const drizzled::plugin::ColumnInfo *column;
  Name_resolution_context *context= &session->lex->select_lex.context;

  for (; *field_num >= 0; field_num++)
  {
    column= tab_columns[*field_num];
    if (! session->lex->verbose && (*field_num == 13 ||
                                    *field_num == 17 ||
                                    *field_num == 18))
    {
      continue;
    }
    Item_field *field= new Item_field(context,
                                      NULL, NULL, column->getName().c_str());
    if (field)
    {
      field->set_name(column->getOldName().c_str(),
                      column->getOldName().length(),
                      system_charset_info);
      if (session->add_item_to_list(field))
      {
        return 1;
      }
    }
  }
  return 0;
}
