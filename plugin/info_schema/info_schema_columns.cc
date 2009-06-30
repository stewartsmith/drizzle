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
