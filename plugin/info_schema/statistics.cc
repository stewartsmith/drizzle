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
 *   statistics I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/tztime.h"

#include "helper_methods.h"
#include "statistics.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the statistics I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the statistics I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * statistics I_S table.
 */
static plugin::InfoSchemaTable *stats_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *StatisticsIS::createColumns()
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
                                            "Table"));

  columns->push_back(new plugin::ColumnInfo("NON_UNIQUE",
                                            1,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "Non_unique"));

  columns->push_back(new plugin::ColumnInfo("INDEX_SCHEMA",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("INDEX_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Key_name"));

  columns->push_back(new plugin::ColumnInfo("SEQ_IN_INDEX",
                                            2,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "Seq_in_index"));

  columns->push_back(new plugin::ColumnInfo("COLUMN_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Column_name"));

  columns->push_back(new plugin::ColumnInfo("COLLATION",
                                            1,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Collation"));

  columns->push_back(new plugin::ColumnInfo("CARDINALITY",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            1,
                                            "Cardinality"));

  columns->push_back(new plugin::ColumnInfo("SUB_PART",
                                            3,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            1,
                                            "Sub_part"));

  columns->push_back(new plugin::ColumnInfo("PACKED",
                                            10,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Packed"));

  columns->push_back(new plugin::ColumnInfo("NULLABLE",
                                            3,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Null"));

  columns->push_back(new plugin::ColumnInfo("INDEX_TYPE",
                                            16,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Index_type"));

  columns->push_back(new plugin::ColumnInfo("COMMENT",
                                            16,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Comment"));

  columns->push_back(new plugin::ColumnInfo("INDEX_COMMENT",
                                            INDEX_COMMENT_MAXLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Index_Comment"));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *StatisticsIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new StatsISMethods();
  }

  if (stats_table == NULL)
  {
    stats_table= new plugin::InfoSchemaTable("STATISTICS",
                                             *columns,
                                             1, 2, false, true,
                                             OPEN_TABLE_ONLY | OPTIMIZE_I_S_TABLE,
                                             methods);
  }

  return stats_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void StatisticsIS::cleanup()
{
  clearColumns(*columns);
  delete stats_table;
  delete methods;
  delete columns;
}

int StatsISMethods::processTable(plugin::InfoSchemaTable *store_table,
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
    if (session->lex->sql_command != SQLCOM_SHOW_KEYS)
    {
      /*
        I.e. we are in SELECT FROM INFORMATION_SCHEMA.STATISTICS
        rather than in SHOW KEYS
      */
      if (session->is_error())
      {
        push_warning(session, 
                     DRIZZLE_ERROR::WARN_LEVEL_WARN,
                     session->main_da.sql_errno(), 
                     session->main_da.message());
      }
      session->clear_error();
      res= 0;
    }
    return res;
  }
  else
  {
    Table *show_table= tables->table;
    KEY *key_info=show_table->s->key_info;
    if (show_table->cursor)
    {
      show_table->cursor->info(HA_STATUS_VARIABLE |
                               HA_STATUS_NO_LOCK |
                               HA_STATUS_TIME);
    }
    for (uint32_t i= 0; i < show_table->s->keys; i++, key_info++)
    {
      KEY_PART_INFO *key_part= key_info->key_part;
      const char *str;
      for (uint32_t j= 0; j < key_info->key_parts; j++, key_part++)
      {
        table->restoreRecordAsDefault();
        table->setWriteSet(1);
        table->setWriteSet(2);
        table->setWriteSet(3);
        table->setWriteSet(4);
        table->setWriteSet(5);
        table->setWriteSet(6);
        table->setWriteSet(8);
        table->setWriteSet(9);
        table->setWriteSet(10);
        table->setWriteSet(12);
        table->setWriteSet(13);
        table->setWriteSet(14);
        table->setWriteSet(15);
        table->field[1]->store(db_name->str, db_name->length, cs);
        table->field[2]->store(table_name->str, table_name->length, cs);
        table->field[3]->store((int64_t) ((key_info->flags &
                                            HA_NOSAME) ? 0 : 1), true);
        table->field[4]->store(db_name->str, db_name->length, cs);
        table->field[5]->store(key_info->name, strlen(key_info->name), cs);
        table->field[6]->store((int64_t) (j+1), true);
        str= (key_part->field ? key_part->field->field_name :
             "?unknown field?");
        table->field[7]->store(str, strlen(str), cs);
        if (show_table->cursor)
        {
          if (show_table->index_flags(i) & HA_READ_ORDER)
          {
            table->field[8]->store(((key_part->key_part_flag &
                                     HA_REVERSE_SORT) ?
                                    "D" : "A"), 1, cs);
            table->field[8]->set_notnull();
          }
          KEY *key= show_table->key_info + i;
          if (key->rec_per_key[j])
          {
            ha_rows records=(show_table->cursor->stats.records /
                             key->rec_per_key[j]);
            table->field[9]->store((int64_t) records, true);
            table->field[9]->set_notnull();
          }
          str= show_table->cursor->index_type(i);
          table->field[13]->store(str, strlen(str), cs);
        }
        if ((key_part->field &&
             key_part->length !=
             show_table->s->field[key_part->fieldnr-1]->key_length()))
        {
          table->field[10]->store((int64_t) key_part->length /
                                  key_part->field->charset()->mbmaxlen, true);
          table->field[10]->set_notnull();
        }
        uint32_t flags= key_part->field ? key_part->field->flags : 0;
        const char *pos= (char*) ((flags & NOT_NULL_FLAG) ? "" : "YES");
        table->field[12]->store(pos, strlen(pos), cs);
        if (!show_table->s->keys_in_use.test(i))
        {
          table->field[14]->store(STRING_WITH_LEN("disabled"), cs);
        }
        else
        {
          table->field[14]->store("", 0, cs);
        }
        table->field[14]->set_notnull();
        assert(test(key_info->flags & HA_USES_COMMENT) ==
                   (key_info->comment.length > 0));
        if (key_info->flags & HA_USES_COMMENT)
        {
          table->field[15]->store(key_info->comment.str,
                                  key_info->comment.length, cs);
        }
        store_table->addRow(table->record[0], table->s->reclength);
      }
    }
  }
  return res;
}
