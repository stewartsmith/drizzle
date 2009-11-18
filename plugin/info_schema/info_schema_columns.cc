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
