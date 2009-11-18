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
 *   key column usage I_S table methods.
 */

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/show.h>

#include "info_schema_methods.h"
#include "key_column_usage.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the key column usage I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the key column usage I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * key column usage I_S table.
 */
static plugin::InfoSchemaTable *key_col_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *KeyColumnUsageIS::createColumns()
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

  columns->push_back(new plugin::ColumnInfo("TABLE_CATALOG",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
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

  columns->push_back(new plugin::ColumnInfo("COLUMN_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("ORDINAL_POSITION",
                                            10,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("POSITION_IN_UNIQUE_CONSTRAINT",
                                            10,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            1,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("REFERENCED_TABLE_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("REFERENCED_TABLE_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("REFERENCED_COLUMN_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            OPEN_FULL_TABLE));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *KeyColumnUsageIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new KeyColUsageISMethods();
  }

  if (key_col_table == NULL)
  {
    key_col_table= new plugin::InfoSchemaTable("KEY_COLUMN_USAGE",
                                               *columns,
                                               4, 5, false, true,
                                               OPEN_TABLE_ONLY,
                                               methods);
  }

  return key_col_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void KeyColumnUsageIS::cleanup()
{
  clearColumns(*columns);
  delete key_col_table;
  delete methods;
  delete columns;
}

int KeyColUsageISMethods::processTable(Session *session,
                                       TableList *tables,
                                       Table *table, bool res,
                                       LEX_STRING *db_name,
                                       LEX_STRING *table_name) const
{
  if (res)
  {
    if (session->is_error())
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                   session->main_da.sql_errno(), session->main_da.message());
    session->clear_error();
    return(0);
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
    for (uint32_t i=0 ; i < show_table->s->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
      {
        continue;
      }
      uint32_t f_idx= 0;
      KEY_PART_INFO *key_part= key_info->key_part;
      for (uint32_t j=0 ; j < key_info->key_parts ; j++,key_part++)
      {
        if (key_part->field)
        {
          f_idx++;
          table->restoreRecordAsDefault();
          store_key_column_usage(table, db_name, table_name,
                                 key_info->name,
                                 strlen(key_info->name),
                                 key_part->field->field_name,
                                 strlen(key_part->field->field_name),
                                 (int64_t) f_idx);
          if (schema_table_store_record(session, table))
          {
            return (1);
          }
        }
      }
    }

    show_table->cursor->get_foreign_key_list(session, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> fkey_it(f_key_list);
    while ((f_key_info= fkey_it++))
    {
      LEX_STRING *f_info;
      LEX_STRING *r_info;
      List_iterator_fast<LEX_STRING> it(f_key_info->foreign_fields),
        it1(f_key_info->referenced_fields);
      uint32_t f_idx= 0;
      while ((f_info= it++))
      {
        r_info= it1++;
        f_idx++;
        table->restoreRecordAsDefault();
        store_key_column_usage(table, db_name, table_name,
                               f_key_info->forein_id->str,
                               f_key_info->forein_id->length,
                               f_info->str, f_info->length,
                               (int64_t) f_idx);
        table->field[8]->store((int64_t) f_idx, true);
        table->field[8]->set_notnull();
        table->field[9]->store(f_key_info->referenced_db->str,
                               f_key_info->referenced_db->length,
                               system_charset_info);
        table->field[9]->set_notnull();
        table->field[10]->store(f_key_info->referenced_table->str,
                                f_key_info->referenced_table->length,
                                system_charset_info);
        table->field[10]->set_notnull();
        table->field[11]->store(r_info->str, r_info->length,
                                system_charset_info);
        table->field[11]->set_notnull();
        if (schema_table_store_record(session, table))
        {
          return (1);
        }
      }
    }
  }
  return (res);
}
