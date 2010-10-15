/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Andrew Hutchings
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef CLIENT_DRIZZLEDUMP_DRIZZLE_H
#define CLIENT_DRIZZLEDUMP_DRIZZLE_H

#include "drizzledump_data.h"

class DrizzleDumpDatabaseDrizzle;
class DrizzleDumpDataDrizzle;

class DrizzleDumpForeignKeyDrizzle : public DrizzleDumpForeignKey
{
  public:
    DrizzleDumpForeignKeyDrizzle(std::string name, DrizzleDumpConnection* connection) : DrizzleDumpForeignKey(name, connection)
    { }

    ~DrizzleDumpForeignKeyDrizzle()
    {
    }
};

class DrizzleDumpIndexDrizzle : public DrizzleDumpIndex
{
  public:
    DrizzleDumpIndexDrizzle(std::string &index, DrizzleDumpConnection *connection)
    : DrizzleDumpIndex(index, connection)
    { }

    ~DrizzleDumpIndexDrizzle()
    {
      columns.clear();
    }
};

class DrizzleDumpFieldDrizzle : public DrizzleDumpField
{
  public:
    DrizzleDumpFieldDrizzle(std::string &field, DrizzleDumpConnection *connection)
    : DrizzleDumpField(field, connection)
    { }

    ~DrizzleDumpFieldDrizzle() { }

    void setType(const char* raw_type, const char* collation);
};

class DrizzleDumpTableDrizzle : public DrizzleDumpTable
{
  public:
    DrizzleDumpTableDrizzle(std::string &table, DrizzleDumpConnection *connection)
    : DrizzleDumpTable(table, connection)
    { }

    ~DrizzleDumpTableDrizzle()
    {
      fields.clear();
      indexes.clear();
      fkeys.clear();
    }
    bool populateFields();
    bool populateIndexes();
    bool populateFkeys();
    DrizzleDumpData* getData(void);
};

class DrizzleDumpDatabaseDrizzle : public DrizzleDumpDatabase
{
  public:
    DrizzleDumpDatabaseDrizzle(const std::string &database,
      DrizzleDumpConnection *connection)
    : DrizzleDumpDatabase(database, connection)
    { }

    ~DrizzleDumpDatabaseDrizzle()
    {
      tables.clear();
    }
    bool populateTables(void);
    bool populateTables(const std::vector<std::string> &table_names);
    void setCollate(const char* newCollate);

};

class DrizzleDumpDataDrizzle : public DrizzleDumpData
{
  public:
    DrizzleDumpDataDrizzle(DrizzleDumpTable *dataTable,
      DrizzleDumpConnection *connection);
    ~DrizzleDumpDataDrizzle();
};


#endif /* CLIENT_DRIZZLEDUMP_DRIZZLE_H */
