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
 *   table constraints I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
#include "table_constraints.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the table constraints I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the table constraints I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * table constraints I_S table.
 */
static plugin::InfoSchemaTable *tc_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *TableConstraintsIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    columns->clear();
  }

  columns->push_back(new plugin::ColumnInfo("CONSTRAINT_CATALOG",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("CONSTRAINT_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("CONSTRAINT_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("CONSTRAINT_TYPE",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            OPEN_FULL_TABLE));
  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *TableConstraintsIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new TabConstraintsISMethods();
  }

  if (tc_table == NULL)
  {
    tc_table= new plugin::InfoSchemaTable("TABLE_CONSTRAINTS",
                                          *columns,
                                          3, 4, false, true,
                                          OPEN_TABLE_ONLY,
                                          methods);
  }

  return tc_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void TableConstraintsIS::cleanup()
{
  clearColumns(*columns);
  delete tc_table;
  delete methods;
  delete columns;
}

static bool store_constraints(Session *session, 
                              Table *table, 
                              LEX_STRING *db_name,
                              LEX_STRING *table_name, 
                              const char *key_name,
                              uint32_t key_len, 
                              const char *con_type, 
                              uint32_t con_len)
{
  const CHARSET_INFO * const cs= system_charset_info;
  table->restoreRecordAsDefault();
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[3]->store(db_name->str, db_name->length, cs);
  table->field[4]->store(table_name->str, table_name->length, cs);
  table->field[5]->store(con_type, con_len, cs);
  return schema_table_store_record(session, table);
}

int TabConstraintsISMethods::processTable(Session *session, 
                                          TableList *tables,
                                          Table *table, 
                                          bool res,
                                          LEX_STRING *db_name,
                                          LEX_STRING *table_name) const
{
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
  else
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    Table *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint32_t primary_key= show_table->s->primary_key;
    show_table->cursor->info(HA_STATUS_VARIABLE |
                             HA_STATUS_NO_LOCK |
                             HA_STATUS_TIME);
    for (uint32_t i= 0; i < show_table->s->keys; i++, key_info++)
    {
      if (i != primary_key && ! (key_info->flags & HA_NOSAME))
      {
        continue;
      }

      if (i == primary_key && is_primary_key(key_info))
      {
        if (store_constraints(session, 
                              table, 
                              db_name, 
                              table_name, 
                              key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("PRIMARY KEY")))
        {
          return 1;
        }
      }
      else if (key_info->flags & HA_NOSAME)
      {
        if (store_constraints(session, 
                              table, 
                              db_name, 
                              table_name, 
                              key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("UNIQUE")))
        {
          return 1;
        }
      }
    }

    show_table->cursor->get_foreign_key_list(session, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info= NULL;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info= it++))
    {
      if (store_constraints(session, 
                            table, 
                            db_name, 
                            table_name,
                            f_key_info->forein_id->str,
                            strlen(f_key_info->forein_id->str),
                            "FOREIGN KEY", 11))
      {
        return 1;
      }
    }
  }
  return res;
}

