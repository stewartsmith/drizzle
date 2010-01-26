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
 *   referential constraints I_S table methods.
 */

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
#include "referential_constraints.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the referential constraints I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the referential constraints I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * processlist I_S table.
 */
static plugin::InfoSchemaTable *rc_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *ReferentialConstraintsIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  columns->push_back(new plugin::ColumnInfo("CONSTRAINT_CATALOG",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("CONSTRAINT_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("CONSTRAINT_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("UNIQUE_CONSTRAINT_CATALOG",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("UNIQUE_CONSTRAINT_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("UNIQUE_CONSTRAINT_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            MY_I_S_MAYBE_NULL,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("MATCH_OPTION",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("UPDATE_RULE",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("DELETE_RULE",
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

  columns->push_back(new plugin::ColumnInfo("REFERENCED_TABLE_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *ReferentialConstraintsIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new RefConstraintsISMethods();
  }

  if (rc_table == NULL)
  {
    rc_table= new plugin::InfoSchemaTable("REFERENTIAL_CONSTRAINTS",
                                          *columns,
                                          1, 9, false, true,
                                          0,
                                          methods);
  }

  return rc_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void ReferentialConstraintsIS::cleanup()
{
  clearColumns(*columns);
  delete rc_table;
  delete methods;
  delete columns;
}

int
RefConstraintsISMethods::processTable(plugin::InfoSchemaTable *store_table,
                                      Session *session, 
                                      TableList *tables,
                                      Table *table, 
                                      bool res,
                                      LEX_STRING *db_name, 
                                      LEX_STRING *table_name)
{
  const CHARSET_INFO * const cs= system_charset_info;

  if (res)
  {
    if (session->is_error())
    {
      push_warning(session, 
                   DRIZZLE_ERROR::WARN_LEVEL_WARN,
                   session->main_da.sql_errno(), 
                   session->main_da.message());
    }
    session->clear_error();
    return 0;
  }

  {
    List<FOREIGN_KEY_INFO> f_key_list;
    Table *show_table= tables->table;
    show_table->cursor->info(HA_STATUS_VARIABLE |
                             HA_STATUS_NO_LOCK |
                             HA_STATUS_TIME);

    show_table->cursor->get_foreign_key_list(session, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info= it++))
    {
      table->restoreRecordAsDefault();
      table->setWriteSet(1);
      table->setWriteSet(2);
      table->setWriteSet(4);
      table->setWriteSet(5);
      table->setWriteSet(6);
      table->setWriteSet(7);
      table->setWriteSet(8);
      table->setWriteSet(9);
      table->setWriteSet(10);
      table->field[1]->store(db_name->str, db_name->length, cs);
      table->field[9]->store(table_name->str, table_name->length, cs);
      table->field[2]->store(f_key_info->forein_id->str,
                             f_key_info->forein_id->length, cs);
      table->field[4]->store(f_key_info->referenced_db->str,
                             f_key_info->referenced_db->length, cs);
      table->field[10]->store(f_key_info->referenced_table->str,
                              f_key_info->referenced_table->length, cs);
      if (f_key_info->referenced_key_name)
      {
        table->field[5]->store(f_key_info->referenced_key_name->str,
                               f_key_info->referenced_key_name->length, cs);
        table->field[5]->set_notnull();
      }
      else
      {
        table->field[5]->set_null();
      }
      table->field[6]->store(STRING_WITH_LEN("NONE"), cs);
      table->field[7]->store(f_key_info->update_method->str,
                             f_key_info->update_method->length, cs);
      table->field[8]->store(f_key_info->delete_method->str,
                             f_key_info->delete_method->length, cs);
      store_table->addRow(table->record[0], table->s->reclength);
    }
  }
  return 0;
}

