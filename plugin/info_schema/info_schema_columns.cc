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

#define LIST_PROCESS_HOST_LEN 64

using namespace std;

bool createCharSetColumns(vector<const ColumnInfo *>& cols)
{
  /*
   * Create each column for the CHARACTER_SET table.
   */
  const ColumnInfo *name_col= new(std::nothrow) ColumnInfo("CHARACTER_SET_NAME",
                                                           64,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Charset",
                                                           SKIP_OPEN_TABLE);
  if (name_col == NULL)
  {
    return true;
  }

  const ColumnInfo *collate_col= new(std::nothrow) ColumnInfo("DEFAULT_COLLATE_NAME",
                                                              64,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Default collation",
                                                              SKIP_OPEN_TABLE);
  if (collate_col == NULL)
  {
    return true;
  }

  const ColumnInfo *descrip_col= new(std::nothrow) ColumnInfo("DESCRIPTION",
                                                              60,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Description",
                                                              SKIP_OPEN_TABLE);
  if (descrip_col == NULL)
  {
    return true;
  }

  const ColumnInfo *len_col= new(std::nothrow) ColumnInfo("MAXLEN",
                                                          3,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          0,
                                                          "Maxlen",
                                                          SKIP_OPEN_TABLE);
  if (len_col == NULL)
  {
    return true;
  }

  /*
   * Add the columns to the vector.
   */
  cols.push_back(name_col);
  cols.push_back(collate_col);
  cols.push_back(descrip_col);
  cols.push_back(len_col);

  return false;
}

bool createCollationColumns(vector<const ColumnInfo *>& cols)
{
  /*
   * Create each column for the COLLATION table.
   */
  const ColumnInfo *name_col= new(std::nothrow) ColumnInfo("COLLATION_NAME",
                                                           64,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Collation",
                                                           SKIP_OPEN_TABLE);
  if (name_col == NULL)
  {
    return true;
  }

  const ColumnInfo *char_set_col= new(std::nothrow) ColumnInfo("CHARACTER_SET_NAME",
                                                               64,
                                                               DRIZZLE_TYPE_VARCHAR,
                                                               0,
                                                               0,
                                                               "Default collation",
                                                               SKIP_OPEN_TABLE);
  if (char_set_col == NULL)
  {
    return true;
  }

  const ColumnInfo *descrip_col= new(std::nothrow) ColumnInfo("DESCRIPTION",
                                                              60,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Charset",
                                                              SKIP_OPEN_TABLE);
  if (descrip_col == NULL)
  {
    return true;
  }

  const ColumnInfo *id_col= new(std::nothrow) ColumnInfo("ID",
                                                         MY_INT32_NUM_DECIMAL_DIGITS,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "Id",
                                                         SKIP_OPEN_TABLE);
  if (id_col == NULL)
  {
    return true;
  }

  const ColumnInfo *default_col= new(std::nothrow) ColumnInfo("IS_DEFAULT",
                                                              3,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Default",
                                                              SKIP_OPEN_TABLE);
  if (default_col == NULL)
  {
    return true;
  }

  const ColumnInfo *compiled_col= new(std::nothrow) ColumnInfo("IS_COMPILED",
                                                               3,
                                                               DRIZZLE_TYPE_VARCHAR,
                                                               0,
                                                               0,
                                                               "Compiled",
                                                               SKIP_OPEN_TABLE);
  if (compiled_col == NULL)
  {
    return true;
  }

  const ColumnInfo *sortlen_col= new(std::nothrow) ColumnInfo("SORTLEN",
                                                              3,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              0,
                                                              "Sortlen",
                                                              SKIP_OPEN_TABLE);
  if (sortlen_col == NULL)
  {
    return true;
  }

  /*
   * Add the columns to the vector.
   */
  cols.push_back(name_col);
  cols.push_back(char_set_col);
  cols.push_back(descrip_col);
  cols.push_back(id_col);
  cols.push_back(default_col);
  cols.push_back(compiled_col);
  cols.push_back(sortlen_col);

  return false;
}

bool createCollCharSetColumns(vector<const ColumnInfo *>& cols)
{
  /*
   * Create each column for the table.
   */
  const ColumnInfo *name_col= new(std::nothrow) ColumnInfo("COLLATION_NAME",
                                                           64,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "",
                                                           SKIP_OPEN_TABLE);
  if (name_col == NULL)
  {
    return true;
  }

  const ColumnInfo *char_set_col= new(std::nothrow) ColumnInfo("CHARACTER_SET_NAME",
                                                               64,
                                                               DRIZZLE_TYPE_VARCHAR,
                                                               0,
                                                               0,
                                                               "",
                                                               SKIP_OPEN_TABLE);
  if (char_set_col == NULL)
  {
    return true;
  }

  /*
   * Add the columns to the vector.
   */
  cols.push_back(name_col);
  cols.push_back(char_set_col);

  return false;
}

bool createColColumns(vector<const ColumnInfo *>& cols)
{
  /*
   * Create each column for the COLUMNS table.
   */
  const ColumnInfo *tab_cat= new(std::nothrow) ColumnInfo("TABLE_CATALOG",
                                                          FN_REFLEN,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          1,
                                                          "",
                                                          OPEN_FRM_ONLY);
  if (tab_cat == NULL)
  {
    return true;
  }

  const ColumnInfo *tab_sch= new(std::nothrow) ColumnInfo("TABLE_SCHEMA",
                                                          NAME_CHAR_LEN,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "",
                                                          OPEN_FRM_ONLY);
  if (tab_sch == NULL)
  {
    return true;
  }

  const ColumnInfo *tab_name= new(std::nothrow) ColumnInfo("TABLE_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "",
                                                           OPEN_FRM_ONLY);
  if (tab_name == NULL)
  {
    return true;
  }

  const ColumnInfo *col_name= new(std::nothrow) ColumnInfo("COLUMN_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Field",
                                                           OPEN_FRM_ONLY);
  if (col_name == NULL)
  {
    return true;
  }

  const ColumnInfo *ord_pos= new(std::nothrow) ColumnInfo("ORDINAL_POSITION",
                                                          MY_INT64_NUM_DECIMAL_DIGITS,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          MY_I_S_UNSIGNED,
                                                          "",
                                                          OPEN_FRM_ONLY);
  if (ord_pos == NULL)
  {
    return true;
  }

  const ColumnInfo *col_def= new(std::nothrow) ColumnInfo("COLUMN_DEFAULT",
                                                          MAX_FIELD_VARCHARLENGTH,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          1,
                                                          "Default",
                                                          OPEN_FRM_ONLY);
  if (col_def == NULL)
  {
    return true;
  }

  const ColumnInfo *is_nullable= new(std::nothrow) ColumnInfo("IS_NULLABLE",
                                                              3,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Null",
                                                              OPEN_FRM_ONLY);
  if (is_nullable == NULL)
  {
    return true;
  }

  const ColumnInfo *data_type= new(std::nothrow) ColumnInfo("DATA_TYPE",
                                                            NAME_CHAR_LEN,
                                                            DRIZZLE_TYPE_VARCHAR,
                                                            0,
                                                            0,
                                                            "",
                                                            OPEN_FRM_ONLY);
  if (data_type == NULL)
  {
    return true;
  }

  const ColumnInfo *max_len= new(std::nothrow) ColumnInfo("CHARACTER_MAXIMUM_LENGTH",
                                                          MY_INT64_NUM_DECIMAL_DIGITS,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                          "",
                                                          OPEN_FRM_ONLY);
  if (max_len == NULL)
  {
    return true;
  }

  const ColumnInfo *octet_len= new(std::nothrow) ColumnInfo("CHARACTER_OCTET_LENGTH",
                                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                                            DRIZZLE_TYPE_LONGLONG,
                                                            0,
                                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                            "",
                                                            OPEN_FRM_ONLY);
  if (octet_len == NULL)
  {
    return true;
  }

  const ColumnInfo *num_prec= new(std::nothrow) ColumnInfo("NUMERIC_PRECISION",
                                                           MY_INT64_NUM_DECIMAL_DIGITS,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                           "",
                                                           OPEN_FRM_ONLY);
  if (num_prec == NULL)
  {
    return true;
  }

  const ColumnInfo *num_scale= new(std::nothrow) ColumnInfo("NUMERIC_SCALE",
                                                            MY_INT64_NUM_DECIMAL_DIGITS,
                                                            DRIZZLE_TYPE_LONGLONG,
                                                            0,
                                                            (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED),
                                                            "",
                                                            OPEN_FRM_ONLY);
  if (num_scale == NULL)
  {
    return true;
  }

  const ColumnInfo *char_set_name= new(std::nothrow) ColumnInfo("CHARACTER_SET_NAME",
                                                                64,
                                                                DRIZZLE_TYPE_VARCHAR,
                                                                0,
                                                                1,
                                                                "",
                                                                OPEN_FRM_ONLY);
  if (char_set_name == NULL)
  {
    return true;
  }

  const ColumnInfo *coll_name= new(std::nothrow) ColumnInfo("COLLATION_NAME",
                                                            64,
                                                            DRIZZLE_TYPE_VARCHAR,
                                                            0,
                                                            1,
                                                            "Collation",
                                                            OPEN_FRM_ONLY);
  if (coll_name == NULL)
  {
    return true;
  }

  const ColumnInfo *col_type= new(std::nothrow) ColumnInfo("COLUMN_TYPE",
                                                           65535,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Type",
                                                           OPEN_FRM_ONLY);
  if (col_type == NULL)
  {
    return true;
  }

  const ColumnInfo *col_key= new(std::nothrow) ColumnInfo("COLUMN_KEY",
                                                          3,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "Key",
                                                          OPEN_FRM_ONLY);
  if (col_key == NULL)
  {
    return true;
  }

  const ColumnInfo *extra= new(std::nothrow) ColumnInfo("EXTRA",
                                                        27,
                                                        DRIZZLE_TYPE_VARCHAR,
                                                        0,
                                                        0,
                                                        "Extra",
                                                        OPEN_FRM_ONLY);
  if (extra == NULL)
  {
    return true;
  }

  const ColumnInfo *priv= new(std::nothrow) ColumnInfo("PRIVILEGES",
                                                       80,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "Privileges",
                                                       OPEN_FRM_ONLY);
  if (priv == NULL)
  {
    return true;
  }

  const ColumnInfo *col_comment= new(std::nothrow) ColumnInfo("COLUMN_COMMENT",
                                                              COLUMN_COMMENT_MAXLEN,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Comment",
                                                              OPEN_FRM_ONLY);
  if (col_comment == NULL)
  {
    return true;
  }

  const ColumnInfo *storage= new(std::nothrow) ColumnInfo("STORAGE",
                                                          8,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "Storage",
                                                          OPEN_FRM_ONLY);
  if (storage == NULL)
  {
    return true;
  }

  const ColumnInfo *format= new(std::nothrow) ColumnInfo("FORMAT",
                                                         8,
                                                         DRIZZLE_TYPE_VARCHAR,
                                                         0,
                                                         0,
                                                         "Format",
                                                         OPEN_FRM_ONLY);
  if (format == NULL)
  {
    return true;
  }

  /*
   * Add the columns to the vector.
   */
  cols.push_back(tab_cat);
  cols.push_back(tab_sch);
  cols.push_back(tab_name);
  cols.push_back(col_name);
  cols.push_back(ord_pos);
  cols.push_back(col_def);
  cols.push_back(is_nullable);
  cols.push_back(data_type);
  cols.push_back(max_len);
  cols.push_back(octet_len);
  cols.push_back(num_prec);
  cols.push_back(num_scale);
  cols.push_back(char_set_name);
  cols.push_back(coll_name);
  cols.push_back(col_type);
  cols.push_back(col_key);
  cols.push_back(extra);
  cols.push_back(priv);
  cols.push_back(col_comment);
  cols.push_back(storage);
  cols.push_back(format);

  return false;
}

bool createKeyColUsageColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *cat= new(std::nothrow) ColumnInfo("CONSTRAINT_CATALOG",
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

  const ColumnInfo *sch= new(std::nothrow) ColumnInfo("CONSTRAINT_SCHEMA",
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

  const ColumnInfo *name= new(std::nothrow) ColumnInfo("CONSTRAINT_NAME",
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

  const ColumnInfo *tab_cat= new(std::nothrow) ColumnInfo("TABLE_CATALOG",
                                                          FN_REFLEN,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          1,
                                                          "",
                                                          OPEN_FULL_TABLE);
  if (tab_cat == NULL)
  {
    return true;
  }

  const ColumnInfo *tab_sch= new(std::nothrow) ColumnInfo("TABLE_SCHEMA",
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

  const ColumnInfo *tab_name= new(std::nothrow) ColumnInfo("TABLE_NAME",
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

  const ColumnInfo *col_name= new(std::nothrow) ColumnInfo("COLUMN_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "",
                                                           OPEN_FULL_TABLE);
  if (col_name == NULL)
  {
    return true;
  }
  const ColumnInfo *ord_pos= new(std::nothrow) ColumnInfo("ORDINAL_POSITION",
                                                          10,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          0,
                                                          "",
                                                          OPEN_FULL_TABLE);
  if (ord_pos == NULL)
  {
    return true;
  }

  const ColumnInfo *pos_in_uniq= new(std::nothrow) ColumnInfo("POSITION_IN_UNIQUE_CONSTRAINT",
                                                              10,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              1,
                                                              "",
                                                              OPEN_FULL_TABLE);
  if (pos_in_uniq == NULL)
  {
    return true;
  }

  const ColumnInfo *ref_tab_sch= new(std::nothrow) ColumnInfo("REFERENCED_TABLE_SCHEMA",
                                                              NAME_CHAR_LEN,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              1,
                                                              "",
                                                              OPEN_FULL_TABLE);
  if (ref_tab_sch == NULL)
  {
    return true;
  }

  const ColumnInfo *ref_tab_name= new(std::nothrow) ColumnInfo("REFERENCED_TABLE_NAME",
                                                               NAME_CHAR_LEN,
                                                               DRIZZLE_TYPE_VARCHAR,
                                                               0,
                                                               1,
                                                               "",
                                                               OPEN_FULL_TABLE);
  if (ref_tab_name == NULL)
  {
    return true;
  }

  const ColumnInfo *ref_col_name= new(std::nothrow) ColumnInfo("REFERENCED_COLUMN_NAME",
                                                               NAME_CHAR_LEN,
                                                               DRIZZLE_TYPE_VARCHAR,
                                                               0,
                                                               1,
                                                               "",
                                                               OPEN_FULL_TABLE);
  if (ref_col_name == NULL)
  {
    return true;
  }

  cols.push_back(cat);
  cols.push_back(sch);
  cols.push_back(name);
  cols.push_back(tab_cat);
  cols.push_back(tab_sch);
  cols.push_back(tab_name);
  cols.push_back(col_name);
  cols.push_back(ord_pos);
  cols.push_back(pos_in_uniq);
  cols.push_back(ref_tab_sch);
  cols.push_back(ref_tab_name);
  cols.push_back(ref_col_name);

  return false;
}

bool createOpenTabColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *db= new(std::nothrow) ColumnInfo("Database",
                                                     NAME_CHAR_LEN,
                                                     DRIZZLE_TYPE_VARCHAR,
                                                     0,
                                                     0,
                                                     "Database",
                                                     SKIP_OPEN_TABLE);
  if (db == NULL)
  {
    return true;
  }

  const ColumnInfo *tab= new(std::nothrow) ColumnInfo("Table",
                                                      NAME_CHAR_LEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      0,
                                                      "Table",
                                                      SKIP_OPEN_TABLE);
  if (tab == NULL)
  {
    return true;
  }

  const ColumnInfo *in_use= new(std::nothrow) ColumnInfo("In_use",
                                                         1,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "In_use",
                                                         SKIP_OPEN_TABLE);
  if (in_use == NULL)
  {
    return true;
  }

  const ColumnInfo *name_locked= new(std::nothrow) ColumnInfo("Name_locked",
                                                              4,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              0,
                                                              "Name_locked",
                                                              SKIP_OPEN_TABLE);
  if (name_locked == NULL)
  {
    return true;
  }

  cols.push_back(db);
  cols.push_back(tab);
  cols.push_back(in_use);
  cols.push_back(name_locked);

  return false;
}

bool createPluginsColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *name= new(std::nothrow) ColumnInfo("PLUGIN_NAME",
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

  const ColumnInfo *ver= new(std::nothrow) ColumnInfo("PLUGIN_VERSION",
                                                      20,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      0,
                                                      "",
                                                      SKIP_OPEN_TABLE);
  if (ver == NULL)
  {
    return true;
  }

  const ColumnInfo *stat= new(std::nothrow) ColumnInfo("PLUGIN_STATUS",
                                                       10,
                                                       DRIZZLE_TYPE_VARCHAR,
                                                       0,
                                                       0,
                                                       "Status",
                                                       SKIP_OPEN_TABLE);
  if (stat == NULL)
  {
    return true;
  }

  const ColumnInfo *aut= new(std::nothrow) ColumnInfo("PLUGIN_AUTHOR",
                                                      NAME_CHAR_LEN,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      1,
                                                      "",
                                                      SKIP_OPEN_TABLE);
  if (aut == NULL)
  {
    return true;
  }

  const ColumnInfo *descrip= new(std::nothrow) ColumnInfo("PLUGIN_DESCRIPTION",
                                                          65535,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          1,
                                                          "",
                                                          SKIP_OPEN_TABLE);
  if (descrip == NULL)
  {
    return true;
  }

  const ColumnInfo *lic= new(std::nothrow) ColumnInfo("PLUGIN_LICENSE",
                                                      80,
                                                      DRIZZLE_TYPE_VARCHAR,
                                                      0,
                                                      1,
                                                      "License",
                                                      SKIP_OPEN_TABLE);
  if (lic == NULL)
  {
    return true;
  }

  cols.push_back(name);
  cols.push_back(ver);
  cols.push_back(stat);
  cols.push_back(aut);
  cols.push_back(descrip);
  cols.push_back(lic);

  return false;
}

bool createProcessListColumns(vector<const ColumnInfo *>& cols)
{
  /*
   * Create each column for the PROCESSLIST table.
   */
  const ColumnInfo *id_col= new(std::nothrow) ColumnInfo("ID", 
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "Id",
                                                         SKIP_OPEN_TABLE);
  if (id_col == NULL)
  {
    return true;
  }

  const ColumnInfo *user_col= new(std::nothrow) ColumnInfo("USER",
                                                           16,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "User",
                                                           SKIP_OPEN_TABLE);
  if (user_col == NULL)
  {
    return true;
  }

  const ColumnInfo *host_col= new(std::nothrow) ColumnInfo("HOST",
                                                           LIST_PROCESS_HOST_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Host",
                                                           SKIP_OPEN_TABLE);
  if (host_col == NULL)
  {
    return true;
  }

  const ColumnInfo *db_col= new(std::nothrow) ColumnInfo("DB",
                                                         NAME_CHAR_LEN,
                                                         DRIZZLE_TYPE_VARCHAR,
                                                         0,
                                                         1,
                                                         "Db",
                                                         SKIP_OPEN_TABLE);
  if (db_col == NULL)
  {
    return true;
  }

  const ColumnInfo *command_col= new(std::nothrow) ColumnInfo("COMMAND",
                                                              16,
                                                              DRIZZLE_TYPE_VARCHAR,
                                                              0,
                                                              0,
                                                              "Command",
                                                              SKIP_OPEN_TABLE);
  if (command_col == NULL)
  {
    return true;
  }

  const ColumnInfo *time_col= new(std::nothrow) ColumnInfo("TIME",
                                                           7,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0,
                                                           "Time",
                                                           SKIP_OPEN_TABLE);
  if (time_col == NULL)
  {
    return true;
  }

  const ColumnInfo *state_col= new(std::nothrow) ColumnInfo("STATE",
                                                            64,
                                                            DRIZZLE_TYPE_VARCHAR,
                                                            0,
                                                            1,
                                                            "State",
                                                            SKIP_OPEN_TABLE);
  if (state_col == NULL)
  {
    return true;
  }

  const ColumnInfo *info_col= new(std::nothrow) ColumnInfo("INFO",
                                                           PROCESS_LIST_INFO_WIDTH,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           1,
                                                           "Info",
                                                           SKIP_OPEN_TABLE);
  if (info_col == NULL)
  {
    return true;
  }

  /*
   * Add the columns to the vector.
   */
  cols.push_back(id_col);
  cols.push_back(user_col);
  cols.push_back(host_col);
  cols.push_back(db_col);
  cols.push_back(command_col);
  cols.push_back(time_col);
  cols.push_back(state_col);
  cols.push_back(info_col);

  return false;
}

bool createRefConstraintColumns(vector<const ColumnInfo *>& cols)
{
  /*
   * Create the columns for the table.
   */
  const ColumnInfo *cat= new(std::nothrow) ColumnInfo("CONSTRAINT_CATALOG",
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

  const ColumnInfo *sch= new(std::nothrow) ColumnInfo("CONSTRAINT_SCHEMA",
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

  const ColumnInfo *name= new(std::nothrow) ColumnInfo("CONSTRAINT_NAME",
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

  const ColumnInfo *uniq_cat= new(std::nothrow) ColumnInfo("UNIQUE_CONSTRAINT_CATALOG",
                                                           FN_REFLEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           1,
                                                           "",
                                                           OPEN_FULL_TABLE);
  if (uniq_cat == NULL)
  {
    return true;
  }

  const ColumnInfo *uniq_sch= new(std::nothrow) ColumnInfo("UNIQUE_CONSTRAINT_SCHEMA",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "",
                                                           OPEN_FULL_TABLE);
  if (uniq_sch == NULL)
  {
    return true;
  }

  const ColumnInfo *uniq_name= new(std::nothrow) ColumnInfo("UNIQUE_CONSTRAINT_NAME",
                                                            NAME_CHAR_LEN,
                                                            DRIZZLE_TYPE_VARCHAR,
                                                            0,
                                                            MY_I_S_MAYBE_NULL,
                                                            "",
                                                            OPEN_FULL_TABLE);
  if (uniq_name == NULL)
  {
    return true;
  }

  const ColumnInfo *match= new(std::nothrow) ColumnInfo("MATCH_OPTION",
                                                        NAME_CHAR_LEN,
                                                        DRIZZLE_TYPE_VARCHAR,
                                                        0,
                                                        0,
                                                        "",
                                                        OPEN_FULL_TABLE);
  if (match == NULL)
  {
    return true;
  }

  const ColumnInfo *update= new(std::nothrow) ColumnInfo("UPDATE_RULE",
                                                         NAME_CHAR_LEN,
                                                         DRIZZLE_TYPE_VARCHAR,
                                                         0,
                                                         0,
                                                         "",
                                                         OPEN_FULL_TABLE);
  if (update == NULL)
  {
    return true;
  }

  const ColumnInfo *del_rule= new(std::nothrow) ColumnInfo("DELETE_RULE",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "",
                                                           OPEN_FULL_TABLE);
  if (del_rule == NULL)
  {
    return true;
  }

  const ColumnInfo *tab_name= new(std::nothrow) ColumnInfo("TABLE_NAME",
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

  const ColumnInfo *ref_name= new(std::nothrow) ColumnInfo("REFERENCED_TABLE_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "",
                                                           OPEN_FULL_TABLE);
  if (ref_name == NULL)
  {
    return true;
  }

  /*
   * Add the columns to the vector.
   */
  cols.push_back(cat);
  cols.push_back(sch);
  cols.push_back(name);
  cols.push_back(uniq_cat);
  cols.push_back(uniq_sch);
  cols.push_back(uniq_name);
  cols.push_back(match);
  cols.push_back(update);
  cols.push_back(del_rule);
  cols.push_back(tab_name);
  cols.push_back(ref_name);

  return false;
}

bool createSchemataColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *cat_name= new(std::nothrow) ColumnInfo("CATALOG_NAME",
                                                           FN_REFLEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0, 
                                                           1, 
                                                           "", 
                                                           SKIP_OPEN_TABLE);
  if (cat_name == NULL)
  {
    return true;
  }

  const ColumnInfo *sch_name= new(std::nothrow) ColumnInfo("SCHEMA_NAME",
                                                           NAME_CHAR_LEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0, 
                                                           0, 
                                                           "Database", 
                                                           SKIP_OPEN_TABLE);
  if (sch_name == NULL)
  {
    return true;
  }

  const ColumnInfo *cs_name= new(std::nothrow) ColumnInfo("DEFAULT_CHARACTER_SET_NAME",
                                                          64, 
                                                          DRIZZLE_TYPE_VARCHAR, 
                                                          0, 
                                                          0, 
                                                          "",
                                                          SKIP_OPEN_TABLE);
  if (cs_name == NULL)
  {
    return true;
  }

  const ColumnInfo *coll_name= new(std::nothrow) ColumnInfo("DEFAULT_COLLATION_NAME",
                                                            64, 
                                                            DRIZZLE_TYPE_VARCHAR, 
                                                            0, 
                                                            0, 
                                                            "",
                                                            SKIP_OPEN_TABLE);
  if (coll_name == NULL)
  {
    return true;
  }

  const ColumnInfo *sql_path= new(std::nothrow) ColumnInfo("SQL_PATH",
                                                           FN_REFLEN,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0, 
                                                           1, 
                                                           "", 
                                                           SKIP_OPEN_TABLE);
  if (sql_path == NULL)
  {
    return true;
  }

  cols.push_back(cat_name);
  cols.push_back(sch_name);
  cols.push_back(cs_name);
  cols.push_back(coll_name);
  cols.push_back(sql_path);

  return false;
}

bool createStatsColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *cat= new(std::nothrow) ColumnInfo("TABLE_CATALOG",
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

  const ColumnInfo *sch= new(std::nothrow) ColumnInfo("TABLE_SCHEMA",
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
  
  const ColumnInfo *name= new(std::nothrow) ColumnInfo("TABLE_NAME",
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

  const ColumnInfo *uniq= new(std::nothrow) ColumnInfo("NON_UNIQUE",
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

  const ColumnInfo *idx_sch= new(std::nothrow) ColumnInfo("INDEX_SCHEMA",
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

  const ColumnInfo *idx_name= new(std::nothrow) ColumnInfo("INDEX_NAME",
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

  const ColumnInfo *seq_in_idx= new(std::nothrow) ColumnInfo("SEQ_IN_INDEX",
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

  const ColumnInfo *col_name= new(std::nothrow) ColumnInfo("COLUMN_NAME",
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

  const ColumnInfo *coll= new(std::nothrow) ColumnInfo("COLLATION",
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

  const ColumnInfo *card= new(std::nothrow) ColumnInfo("CARDINALITY",
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

  const ColumnInfo *sub_part= new(std::nothrow) ColumnInfo("SUB_PART",
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

  const ColumnInfo *packed= new(std::nothrow) ColumnInfo("PACKED",
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

  const ColumnInfo *nullable= new(std::nothrow) ColumnInfo("NULLABLE",
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

  const ColumnInfo *idx_type= new(std::nothrow) ColumnInfo("INDEX_TYPE",
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

  const ColumnInfo *comment= new(std::nothrow) ColumnInfo("COMMENT",
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

  const ColumnInfo *idx_comment= new(std::nothrow) ColumnInfo("INDEX_COMMENT",
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

bool createTabConstraintsColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *cat= new(std::nothrow) ColumnInfo("CONSTRAINT_CATALOG",
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

  const ColumnInfo *sch= new(std::nothrow) ColumnInfo("CONSTRAINT_SCHEMA",
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

  const ColumnInfo *name= new(std::nothrow) ColumnInfo("CONSTRAINT_NAME",
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

  const ColumnInfo *tab_sch= new(std::nothrow) ColumnInfo("TABLE_SCHEMA",
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

  const ColumnInfo *tab_name= new(std::nothrow) ColumnInfo("TABLE_NAME",
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

  const ColumnInfo *type= new(std::nothrow) ColumnInfo("CONSTRAINT_TYPE",
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

bool createTablesColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *cat= new(std::nothrow) ColumnInfo("TABLE_CATALOG",
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

  const ColumnInfo *sch= new(std::nothrow) ColumnInfo("TABLE_SCHEMA",
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

  const ColumnInfo *name= new(std::nothrow) ColumnInfo("TABLE_NAME",
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

  const ColumnInfo *type= new(std::nothrow) ColumnInfo("TABLE_TYPE",
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

  const ColumnInfo *engine= new(std::nothrow) ColumnInfo("ENGINE",
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

  const ColumnInfo *version= new(std::nothrow) ColumnInfo("VERSION",
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

  const ColumnInfo *row_format= new(std::nothrow) ColumnInfo("ROW_FORMAT",
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

  const ColumnInfo *tab_rows= new(std::nothrow) ColumnInfo("TABLE_ROWS",
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

  const ColumnInfo *avg_row_len= new(std::nothrow) ColumnInfo("AVG_ROW_LENGTH",
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

  const ColumnInfo *data_len= new(std::nothrow) ColumnInfo("DATA_LENGTH",
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

  const ColumnInfo *max_data_len= new(std::nothrow) ColumnInfo("MAX_DATA_LENGTH",
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

  const ColumnInfo *idx_len= new(std::nothrow) ColumnInfo("INDEX_LENGTH",
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

  const ColumnInfo *data_free= new(std::nothrow) ColumnInfo("DATA_FREE",
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

  const ColumnInfo *auto_inc= new(std::nothrow) ColumnInfo("AUTO_INCREMENT",
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

  const ColumnInfo *create_time= new(std::nothrow) ColumnInfo("CREATE_TIME",
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

  const ColumnInfo *update_time= new(std::nothrow) ColumnInfo("UPDATE_TIME",
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

  const ColumnInfo *check_time= new(std::nothrow) ColumnInfo("CHECK_TIME",
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

  const ColumnInfo *table_coll= new(std::nothrow) ColumnInfo("TABLE_COLLATION",
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

  const ColumnInfo *checksum= new(std::nothrow) ColumnInfo("CHECKSUM",
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

  const ColumnInfo *create_opt= new(std::nothrow) ColumnInfo("CREATE_OPTIONS",
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

  const ColumnInfo *tab_comment= new(std::nothrow) ColumnInfo("TABLE_COMMENT",
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


bool createTabNamesColumns(vector<const ColumnInfo *>& cols)
{
  const ColumnInfo *cat= new(std::nothrow) ColumnInfo("TABLE_CATALOG",
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

  const ColumnInfo *sch= new(std::nothrow) ColumnInfo("TABLE_SCHEMA",
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

  const ColumnInfo *name= new(std::nothrow) ColumnInfo("TABLE_NAME",
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

  const ColumnInfo *type= new(std::nothrow) ColumnInfo("TABLE_TYPE",
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

void clearColumns(vector<const ColumnInfo *>& cols)
{
  for_each(cols.begin(), cols.end(), DeleteColumns());
  cols.clear();
}
