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
 *   Contains methods for creating the various columns for 
 *   each I_S table.
 */

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/show.h>

#include "info_schema_columns.h"

using namespace std;

bool createStatsColumns(vector<const drizzled::plugin::ColumnInfo *>& cols)
{
  const drizzled::plugin::ColumnInfo *cat= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_CATALOG",
                                                      FN_REFLEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      1,
                                                      "",
                                                      OPEN_FRM_ONLY);
  if (cat == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *sch= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_SCHEMA",
                                                      NAME_CHAR_LEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      0,
                                                      "",
                                                      OPEN_FRM_ONLY);
  if (sch == NULL)
  {
    return true;
  }
  
  const drizzled::plugin::ColumnInfo *name= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_NAME",
                                                       NAME_CHAR_LEN,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "Table",
                                                       OPEN_FRM_ONLY);
  if (name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *uniq= new(std::nothrow) drizzled::plugin::ColumnInfo("NON_UNIQUE",
                                                       1,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       0,
                                                       "Non_unique",
                                                       OPEN_FRM_ONLY);
  if (uniq == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *idx_sch= new(std::nothrow) drizzled::plugin::ColumnInfo("INDEX_SCHEMA",
                                                          NAME_CHAR_LEN,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "",
                                                          OPEN_FRM_ONLY);
  if (idx_sch == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *idx_name= new(std::nothrow) drizzled::plugin::ColumnInfo("INDEX_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Key_name",
                                                           OPEN_FRM_ONLY);
  if (idx_name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *seq_in_idx= new(std::nothrow) drizzled::plugin::ColumnInfo("SEQ_IN_INDEX",
                                                             2,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0,
                                                             "Seq_in_index",
                                                             OPEN_FRM_ONLY);
  if (seq_in_idx == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *col_name= new(std::nothrow) drizzled::plugin::ColumnInfo("COLUMN_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Column_name",
                                                           OPEN_FRM_ONLY);
  if (col_name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *coll= new(std::nothrow) drizzled::plugin::ColumnInfo("COLLATION",
                                                       1,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       1,
                                                       "Collation",
                                                       OPEN_FRM_ONLY);
  if (coll == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *card= new(std::nothrow) drizzled::plugin::ColumnInfo("CARDINALITY",
                                                       MY_INT64_NUM_DECIMAL_DIGITS,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       1,
                                                       "Cardinality",
                                                       OPEN_FULL_TABLE);
  if (card == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *sub_part= new(std::nothrow) drizzled::plugin::ColumnInfo("SUB_PART",
                                                           3,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           1,
                                                           "Sub_part",
                                                           OPEN_FRM_ONLY);
  if (sub_part == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *packed= new(std::nothrow) drizzled::plugin::ColumnInfo("PACKED",
                                                         10,
                                                         DRIZZLE_TYPE_VARCHAR,
                                                         0,
                                                         1,
                                                         "Packed",
                                                         OPEN_FRM_ONLY);
  if (packed == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *nullable= new(std::nothrow) drizzled::plugin::ColumnInfo("NULLABLE",
                                                           3,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Null",
                                                           OPEN_FRM_ONLY);
  if (nullable == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *idx_type= new(std::nothrow) drizzled::plugin::ColumnInfo("INDEX_TYPE",
                                                           16,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Index_type",
                                                           OPEN_FULL_TABLE);
  if (idx_type == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *comment= new(std::nothrow) drizzled::plugin::ColumnInfo("COMMENT",
                                                          16,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          1,
                                                          "Comment",
                                                          OPEN_FRM_ONLY);
  if (comment == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *idx_comment= new(std::nothrow) drizzled::plugin::ColumnInfo("INDEX_COMMENT",
                                                              INDEX_COMMENT_MAXLEN,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Index_Comment",
                                                              OPEN_FRM_ONLY);
  if (idx_comment == NULL)
  {
    return true;
  }

  cols.push_back(cat);
  cols.push_back(sch);
  cols.push_back(name);
  cols.push_back(uniq);
  cols.push_back(idx_sch);
  cols.push_back(idx_name);
  cols.push_back(seq_in_idx);
  cols.push_back(col_name);
  cols.push_back(coll);
  cols.push_back(card);
  cols.push_back(sub_part);
  cols.push_back(packed);
  cols.push_back(nullable);
  cols.push_back(idx_type);
  cols.push_back(comment);
  cols.push_back(idx_comment);

  return false;
}

bool createStatusColumns(vector<const drizzled::plugin::ColumnInfo *>& cols)
{
  const drizzled::plugin::ColumnInfo *name= new(std::nothrow) drizzled::plugin::ColumnInfo("VARIABLE_NAME",
                                                       64,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "Variable_name",
                                                       SKIP_OPEN_TABLE);
  if (name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *value= new(std::nothrow) drizzled::plugin::ColumnInfo("VARIABLE_VALUE",
                                                        16300,
                                                        DRIZZLE_TYPE_VARCHAR,
                                                        0,
                                                        1,
                                                        "Value",
                                                        SKIP_OPEN_TABLE);
  if (value == NULL)
  {
    return true;
  }

  cols.push_back(name);
  cols.push_back(value);

  return false;
}

bool createTabConstraintsColumns(vector<const drizzled::plugin::ColumnInfo *>& cols)
{
  const drizzled::plugin::ColumnInfo *cat= new(std::nothrow) drizzled::plugin::ColumnInfo("CONSTRAINT_CATALOG",
                                                      FN_REFLEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      1,
                                                      "",
                                                      OPEN_FULL_TABLE);
  if (cat == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *sch= new(std::nothrow) drizzled::plugin::ColumnInfo("CONSTRAINT_SCHEMA",
                                                      NAME_CHAR_LEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      0,
                                                      "",
                                                      OPEN_FULL_TABLE);
  if (sch == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *name= new(std::nothrow) drizzled::plugin::ColumnInfo("CONSTRAINT_NAME",
                                                       NAME_CHAR_LEN,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "",
                                                       OPEN_FULL_TABLE);
  if (name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *tab_sch= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_SCHEMA",
                                                          NAME_CHAR_LEN,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "",
                                                          OPEN_FULL_TABLE);
  if (tab_sch == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *tab_name= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "",
                                                           OPEN_FULL_TABLE);
  if (tab_name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *type= new(std::nothrow) drizzled::plugin::ColumnInfo("CONSTRAINT_TYPE",
                                                       NAME_CHAR_LEN,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "",
                                                       OPEN_FULL_TABLE);
  if (type == NULL)
  {
    return true;
  }

  cols.push_back(cat);
  cols.push_back(sch);
  cols.push_back(name);
  cols.push_back(tab_sch);
  cols.push_back(tab_name);
  cols.push_back(type);

  return false;
}

bool createTablesColumns(vector<const drizzled::plugin::ColumnInfo *>& cols)
{
  const drizzled::plugin::ColumnInfo *cat= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_CATALOG",
                                                      FN_REFLEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      1,
                                                      "",
                                                      SKIP_OPEN_TABLE);
  if (cat == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *sch= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_SCHEMA",
                                                      NAME_CHAR_LEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      0,
                                                      "",
                                                      SKIP_OPEN_TABLE);
  if (sch == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *name= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_NAME",
                                                       NAME_CHAR_LEN,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "Name",
                                                       SKIP_OPEN_TABLE);
  if (name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *type= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_TYPE",
                                                       NAME_CHAR_LEN,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "",
                                                       OPEN_FRM_ONLY);
  if (type == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *engine= new(std::nothrow) drizzled::plugin::ColumnInfo("ENGINE",
                                                         NAME_CHAR_LEN,
                                                         DRIZZLE_TYPE_VARCHAR,
                                                         0,
                                                         1,
                                                         "Engine",
                                                         OPEN_FRM_ONLY);
  if (engine == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *version= new(std::nothrow) drizzled::plugin::ColumnInfo("VERSION",
                                                          MY_INT64_NUM_DECIMAL_DIGITS,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                          "Version",
                                                          OPEN_FRM_ONLY);
  if (version == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *row_format= new(std::nothrow) drizzled::plugin::ColumnInfo("ROW_FORMAT",
                                                             10,
                                                             DRIZZLE_TYPE_VARCHAR,
                                                             0,
                                                             1,
                                                             "Row_format",
                                                             OPEN_FULL_TABLE);
  if (row_format == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *tab_rows= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_ROWS",
                                                           MY_INT64_NUM_DECIMAL_DIGITS,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                           "Rows",
                                                           OPEN_FULL_TABLE);
  if (tab_rows == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *avg_row_len= new(std::nothrow) drizzled::plugin::ColumnInfo("AVG_ROW_LENGTH",
                                                              MY_INT64_NUM_DECIMAL_DIGITS,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                              "Avg_row_length",
                                                              OPEN_FULL_TABLE);
  if (avg_row_len == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *data_len= new(std::nothrow) drizzled::plugin::ColumnInfo("DATA_LENGTH",
                                                           MY_INT64_NUM_DECIMAL_DIGITS,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                           "Data_length",
                                                           OPEN_FULL_TABLE);
  if (data_len == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *max_data_len= new(std::nothrow) drizzled::plugin::ColumnInfo("MAX_DATA_LENGTH",
                                                               MY_INT64_NUM_DECIMAL_DIGITS,
                                                               DRIZZLE_TYPE_LONGLONG,
                                                               0,
                                                               (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                               "Max_data_length",
                                                               OPEN_FULL_TABLE);
  if (max_data_len == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *idx_len= new(std::nothrow) drizzled::plugin::ColumnInfo("INDEX_LENGTH",
                                                          MY_INT64_NUM_DECIMAL_DIGITS,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                          "Index_length",
                                                          OPEN_FULL_TABLE);
  if (idx_len == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *data_free= new(std::nothrow) drizzled::plugin::ColumnInfo("DATA_FREE",
                                                           MY_INT64_NUM_DECIMAL_DIGITS,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                           "Data_free",
                                                           OPEN_FULL_TABLE);
  if (data_free == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *auto_inc= new(std::nothrow) drizzled::plugin::ColumnInfo("AUTO_INCREMENT",
                                                           MY_INT64_NUM_DECIMAL_DIGITS,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                           "Auto_increment",
                                                           OPEN_FULL_TABLE);
  if (auto_inc == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *create_time= new(std::nothrow) drizzled::plugin::ColumnInfo("CREATE_TIME",
                                                              0,
                                                              DRIZZLE_TYPE_DATETIME,
                                                              0,
                                                              1,
                                                              "Create_time",
                                                              OPEN_FULL_TABLE);
  if (create_time == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *update_time= new(std::nothrow) drizzled::plugin::ColumnInfo("UPDATE_TIME",
                                                              0,
                                                              DRIZZLE_TYPE_DATETIME,
                                                              0,
                                                              1,
                                                              "Update_time",
                                                              OPEN_FULL_TABLE);
  if (update_time == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *check_time= new(std::nothrow) drizzled::plugin::ColumnInfo("CHECK_TIME",
                                                             0,
                                                             DRIZZLE_TYPE_DATETIME,
                                                             0,
                                                             1,
                                                             "Check_time",
                                                             OPEN_FULL_TABLE);
  if (check_time == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *table_coll= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_COLLATION",
                                                             64,
                                                             DRIZZLE_TYPE_VARCHAR,
                                                             0,
                                                             1,
                                                             "Collation",
                                                             OPEN_FRM_ONLY);
  if (table_coll == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *checksum= new(std::nothrow) drizzled::plugin::ColumnInfo("CHECKSUM",
                                                           MY_INT64_NUM_DECIMAL_DIGITS,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                           "Checksum",
                                                           OPEN_FULL_TABLE);
  if (checksum == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *create_opt= new(std::nothrow) drizzled::plugin::ColumnInfo("CREATE_OPTIONS",
                                                             255,
                                                             DRIZZLE_TYPE_VARCHAR,
                                                             0,
                                                             1,
                                                             "Create_options",
                                                             OPEN_FRM_ONLY);
  if (create_opt == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *tab_comment= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_COMMENT",
                                                              TABLE_COMMENT_MAXLEN,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Comment",
                                                              OPEN_FRM_ONLY);
  if (tab_comment == NULL)
  {
    return true;
  }

  cols.push_back(cat);
  cols.push_back(sch);
  cols.push_back(name);
  cols.push_back(type);
  cols.push_back(engine);
  cols.push_back(version);
  cols.push_back(row_format);
  cols.push_back(tab_rows);
  cols.push_back(avg_row_len);
  cols.push_back(data_len);
  cols.push_back(max_data_len);
  cols.push_back(idx_len);
  cols.push_back(data_free);
  cols.push_back(auto_inc);
  cols.push_back(create_time);
  cols.push_back(update_time);
  cols.push_back(check_time);
  cols.push_back(table_coll);
  cols.push_back(checksum);
  cols.push_back(create_opt);
  cols.push_back(tab_comment);

  return false;
}


bool createTabNamesColumns(vector<const drizzled::plugin::ColumnInfo *>& cols)
{
  const drizzled::plugin::ColumnInfo *cat= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_CATALOG",
                                                      FN_REFLEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      1,
                                                      "",
                                                      SKIP_OPEN_TABLE);
  if (cat == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *sch= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_SCHEMA",
                                                      NAME_CHAR_LEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      0,
                                                      "",
                                                      SKIP_OPEN_TABLE);
  if (sch == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *name= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_NAME",
                                                       NAME_CHAR_LEN,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "Tables_in_",
                                                       SKIP_OPEN_TABLE);
  if (name == NULL)
  {
    return true;
  }

  const drizzled::plugin::ColumnInfo *type= new(std::nothrow) drizzled::plugin::ColumnInfo("TABLE_TYPE",
                                                       NAME_CHAR_LEN,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "Table_type",
                                                       OPEN_FRM_ONLY);
  if (type == NULL)
  {
    return true;
  }

  cols.push_back(cat);
  cols.push_back(sch);
  cols.push_back(name);
  cols.push_back(type);

  return false;
}

/*
 * Function object used for deleting the memory allocated
 * for the columns contained with the vector of columns.
 */
class DeleteColumns
{
public:
  template<typename T>
  inline void operator()(const T *ptr) const
  {
    delete ptr;
  }
};

void clearColumns(vector<const drizzled::plugin::ColumnInfo *>& cols)
{
  for_each(cols.begin(), cols.end(), DeleteColumns());
  cols.clear();
}
