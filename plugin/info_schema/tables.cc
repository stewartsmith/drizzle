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
 *   tables I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/tztime.h"

#include "helper_methods.h"
#include "tables.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the tables I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the tables I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * tables I_S table.
 */
static plugin::InfoSchemaTable *tbls_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *TablesIS::createColumns()
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
                                            "Name",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_TYPE",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            OPEN_FRM_ONLY));

  columns->push_back(new plugin::ColumnInfo("ENGINE",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Engine",
                                            OPEN_FRM_ONLY));

  columns->push_back(new plugin::ColumnInfo("VERSION",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Version",
                                            OPEN_FRM_ONLY));

  columns->push_back(new plugin::ColumnInfo("ROW_FORMAT",
                                            10,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Row_format",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_ROWS",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Rows",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("AVG_ROW_LENGTH",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Avg_row_length",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("DATA_LENGTH",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Data_length",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("MAX_DATA_LENGTH",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Max_data_length",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("INDEX_LENGTH",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Index_length",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("DATA_FREE",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Data_free",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("AUTO_INCREMENT",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Auto_increment",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("CREATE_TIME",
                                            0,
                                            DRIZZLE_TYPE_DATETIME,
                                            0,
                                            1,
                                            "Create_time",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("UPDATE_TIME",
                                            0,
                                            DRIZZLE_TYPE_DATETIME,
                                            0,
                                            1,
                                            "Update_time",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("CHECK_TIME",
                                            0,
                                            DRIZZLE_TYPE_DATETIME,
                                            0,
                                            1,
                                            "Check_time",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("TABLE_COLLATION",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Collation",
                                            OPEN_FRM_ONLY));

  columns->push_back(new plugin::ColumnInfo("CHECKSUM",
                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                            "Checksum",
                                            OPEN_FULL_TABLE));

  columns->push_back(new plugin::ColumnInfo("CREATE_OPTIONS",
                                            255,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Create_options",
                                            OPEN_FRM_ONLY));

  columns->push_back(new plugin::ColumnInfo("TABLE_COMMENT",
                                            TABLE_COMMENT_MAXLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Comment",
                                            OPEN_FRM_ONLY));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *TablesIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new TablesISMethods();
  }

  if (tbls_table == NULL)
  {
    tbls_table= new plugin::InfoSchemaTable("TABLES",
                                            *columns,
                                            1, 2, false, true,
                                            OPTIMIZE_I_S_TABLE,
                                            methods);
  }

  return tbls_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void TablesIS::cleanup()
{
  clearColumns(*columns);
  delete tbls_table;
  delete methods;
  delete columns;
}

int TablesISMethods::processTable(plugin::InfoSchemaTable *store_table,
                                  Session *session, 
                                  TableList *tables,
                                  Table *table, 
                                  bool res,
                                  LEX_STRING *db_name,
                                  LEX_STRING *table_name)
{
  const char *tmp_buff= NULL;
  DRIZZLE_TIME time;
  const CHARSET_INFO * const cs= system_charset_info;

  table->restoreRecordAsDefault();
  table->setWriteSet(1);
  table->setWriteSet(2);
  table->setWriteSet(3);
  table->setWriteSet(4);
  table->setWriteSet(5);
  table->setWriteSet(6);
  table->setWriteSet(7);
  table->setWriteSet(8);
  table->setWriteSet(9);
  table->setWriteSet(11);
  table->setWriteSet(12);
  table->setWriteSet(13);
  table->setWriteSet(14);
  table->setWriteSet(15);
  table->setWriteSet(16);
  table->setWriteSet(17);
  table->setWriteSet(18);
  table->setWriteSet(19);
  table->setWriteSet(20);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(table_name->str, table_name->length, cs);
  if (res)
  {
    /*
      there was errors during opening tables
    */
    const char *error= session->is_error() ? session->main_da.message() : "";
    if (tables->schema_table)
    {
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    }
    else
    {
      table->field[3]->store(STRING_WITH_LEN("BASE Table"), cs);
    }
    table->field[20]->store(error, strlen(error), cs);
    session->clear_error();
  }
  else
  {
    char option_buff[400];
    char *ptr= NULL;
    Table *show_table= tables->table;
    TableShare *share= show_table->s;
    Cursor *cursor= show_table->cursor;
    drizzled::plugin::StorageEngine *tmp_db_type= share->db_type();

    if (share->tmp_table == SYSTEM_TMP_TABLE)
    {
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    }
    else if (share->tmp_table)
    {
      table->field[3]->store(STRING_WITH_LEN("LOCAL TEMPORARY"), cs);
    }
    else
    {
      table->field[3]->store(STRING_WITH_LEN("BASE Table"), cs);
    }

    for (int i= 4; i < 20; i++)
    {
      if (i == 7 || (i > 12 && i < 17) || i == 18)
      {
        continue;
      }
      table->field[i]->set_notnull();
    }
    const string &engine_name= drizzled::plugin::StorageEngine::resolveName(tmp_db_type);
    table->field[4]->store(engine_name.c_str(), engine_name.size(), cs);
    table->field[5]->store((int64_t) 0, true);

    ptr=option_buff;

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
    {
      ptr= strcpy(ptr," pack_keys=1")+12;
    }
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
    {
      ptr= strcpy(ptr," pack_keys=0")+12;
    }
    if (share->row_type != ROW_TYPE_DEFAULT)
    {
      ptr+= sprintf(ptr, " row_format=%s", ha_row_type[(uint32_t)share->row_type]);
    }
    if (share->block_size)
    {
      ptr= strcpy(ptr, " block_size=")+12;
      ptr= int64_t10_to_str(share->block_size, ptr, 10);
    }

    table->field[19]->store(option_buff+1,
                            (ptr == option_buff ? 0 :
                             (uint32_t) (ptr-option_buff)-1), cs);

    tmp_buff= (share->table_charset ?
               share->table_charset->name : "default");
    table->field[17]->store(tmp_buff, strlen(tmp_buff), cs);

    if (share->hasComment())
      table->field[20]->store(share->getComment(),
                              share->getCommentLength(), cs);

    if (cursor)
    {
      cursor->info(HA_STATUS_VARIABLE | 
                   HA_STATUS_TIME | 
                   HA_STATUS_AUTO |
                   HA_STATUS_NO_LOCK);
      enum row_type row_type = cursor->get_row_type();
      switch (row_type) 
      {
      case ROW_TYPE_NOT_USED:
      case ROW_TYPE_DEFAULT:
        tmp_buff= ((share->db_options_in_use &
                    HA_OPTION_COMPRESS_RECORD) ? "Compressed" :
                   (share->db_options_in_use & HA_OPTION_PACK_RECORD) ?
                   "Dynamic" : "Fixed");
        break;
      case ROW_TYPE_FIXED:
        tmp_buff= "Fixed";
        break;
      case ROW_TYPE_DYNAMIC:
        tmp_buff= "Dynamic";
        break;
      case ROW_TYPE_COMPRESSED:
        tmp_buff= "Compressed";
        break;
      case ROW_TYPE_REDUNDANT:
        tmp_buff= "Redundant";
        break;
      case ROW_TYPE_COMPACT:
        tmp_buff= "Compact";
        break;
      case ROW_TYPE_PAGE:
        tmp_buff= "Paged";
        break;
      }
      table->field[6]->store(tmp_buff, strlen(tmp_buff), cs);
      if (! tables->schema_table)
      {
        table->field[7]->store((int64_t) cursor->stats.records, true);
        table->field[7]->set_notnull();
      }
      table->field[8]->store((int64_t) cursor->stats.mean_rec_length, true);
      table->field[9]->store((int64_t) cursor->stats.data_file_length, true);
      if (cursor->stats.max_data_file_length)
      {
        table->field[10]->store((int64_t) cursor->stats.max_data_file_length,
                                true);
      }
      table->field[11]->store((int64_t) cursor->stats.index_file_length, true);
      table->field[12]->store((int64_t) cursor->stats.delete_length, true);
      if (show_table->found_next_number_field)
      {
        table->field[13]->store((int64_t) cursor->stats.auto_increment_value, true);
        table->field[13]->set_notnull();
      }
      if (cursor->stats.create_time)
      {
        session->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (time_t) cursor->stats.create_time);
        table->field[14]->store_time(&time, DRIZZLE_TIMESTAMP_DATETIME);
        table->field[14]->set_notnull();
      }
      if (cursor->stats.update_time)
      {
        session->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (time_t) cursor->stats.update_time);
        table->field[15]->store_time(&time, DRIZZLE_TIMESTAMP_DATETIME);
        table->field[15]->set_notnull();
      }
      if (cursor->stats.check_time)
      {
        session->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (time_t) cursor->stats.check_time);
        table->field[16]->store_time(&time, DRIZZLE_TIMESTAMP_DATETIME);
        table->field[16]->set_notnull();
      }
      if (cursor->ha_table_flags() & (ulong) HA_HAS_CHECKSUM)
      {
        table->field[18]->store((int64_t) cursor->checksum(), true);
        table->field[18]->set_notnull();
      }
    }
  }
  store_table->addRow(table->record[0], table->s->reclength);
  return false;
}

