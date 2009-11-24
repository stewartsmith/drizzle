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
 *   Open tables I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
#include "open_tables.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the open tables I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the open tables I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * open tables I_S table.
 */
static plugin::InfoSchemaTable *open_tabs_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *OpenTablesIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  columns->push_back(new plugin::ColumnInfo("Database",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Database",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("Table",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Table",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("In_use",
                                            1,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "In_use",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("Name_locked",
                                            4,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "Name_locked",
                                            SKIP_OPEN_TABLE));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *OpenTablesIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new OpenTablesISMethods();
  }

  if (open_tabs_table == NULL)
  {
    open_tabs_table= new plugin::InfoSchemaTable("OPEN_TABLES",
                                                 *columns,
                                                 -1, -1, true, false, 0,
                                                 methods);
  }

  return open_tabs_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void OpenTablesIS::cleanup()
{
  clearColumns(*columns);
  delete open_tabs_table;
  delete methods;
  delete columns;
}

inline bool open_list_store(Table *table, open_table_list_st& open_list);
inline bool open_list_store(Table *table, open_table_list_st& open_list)
{
  table->restoreRecordAsDefault();
  table->field[0]->store(open_list.db.c_str(), open_list.db.length(), system_charset_info);
  table->field[1]->store(open_list.table.c_str(), open_list.table.length(), system_charset_info);
  table->field[2]->store((int64_t) open_list.in_use, true);
  table->field[3]->store((int64_t) open_list.locked, true);
  TableList *tmp= table->pos_in_table_list;
  tmp->schema_table->addRow(table->record[0], table->s->reclength);

  return false;
}

int OpenTablesISMethods::fillTable(Session *session, TableList *tables)
{
  const char *wild= session->lex->wild ? session->lex->wild->ptr() : NULL;

  if ((list_open_tables(session->lex->select_lex.db, wild, open_list_store, tables->table) == true) && session->is_fatal_error)
    return 1;

  return 0;
}

