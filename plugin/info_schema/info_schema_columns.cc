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

void clearColumns(vector<const ColumnInfo *>& cols)
{
  vector<const ColumnInfo *>::iterator iter= cols.begin();

  while (iter != cols.end())
  {
    delete (*iter);
    ++iter;
  }

  cols.clear();
}
