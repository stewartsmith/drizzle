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
 *   table names I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/tztime.h"

#include "helper_methods.h"
#include "table_names.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the table names I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the table names I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * table names I_S table.
 */
static plugin::InfoSchemaTable *tn_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *TableNamesIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  columns->push_back(new plugin::ColumnInfo("TABLE_CATALOG",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Tables_in_",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_TYPE",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Table_type",
                                            OPEN_FRM_ONLY));
  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *TableNamesIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new TabNamesISMethods();
  }

  if (tn_table == NULL)
  {
    tn_table= new plugin::InfoSchemaTable("TABLE_NAMES",
                                          *columns,
                                          1, 2, true, true, 0,
                                          methods);
  }

  return tn_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void TableNamesIS::cleanup()
{
  clearColumns(*columns);
  delete tn_table;
  delete methods;
  delete columns;
}

int TabNamesISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  char tmp[128];
  String buffer(tmp,sizeof(tmp), session->charset());
  LEX *lex= session->lex;
  Name_resolution_context *context= &lex->select_lex.context;
  const drizzled::plugin::InfoSchemaTable::Columns tab_columns= schema_table->getColumns();

  const drizzled::plugin::ColumnInfo *column= tab_columns[2];
  buffer.length(0);
  buffer.append(column->getOldName().c_str());
  buffer.append(lex->select_lex.db);
  if (lex->wild && lex->wild->ptr())
  {
    buffer.append(STRING_WITH_LEN(" ("));
    buffer.append(lex->wild->ptr());
    buffer.append(')');
  }
  Item_field *field= new Item_field(context,
                                    NULL, NULL, column->getName().c_str());
  if (session->add_item_to_list(field))
  {
    return 1;
  }
  field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  if (session->lex->verbose)
  {
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
    column= tab_columns[3];
    field= new Item_field(context, NULL, NULL, column->getName().c_str());
    if (session->add_item_to_list(field))
    {
      return 1;
    }
    field->set_name(column->getOldName().c_str(),
                    column->getOldName().length(),
                    system_charset_info);
  }
  return 0;
}

