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

#ifndef CLIENT_DRIZZLEDUMP_MYSQL_H
#define CLIENT_DRIZZLEDUMP_MYSQL_H

#include "drizzledump_data.h"

class DrizzleDumpDatabaseMySQL;
class DrizzleDumpDataMySQL;

class DrizzleDumpIndexMySQL : public DrizzleDumpIndex
{
  public:
    DrizzleDumpIndexMySQL(std::string &index, DrizzleDumpConnection *connection)
    : DrizzleDumpIndex(index, connection)
    { }

    ~DrizzleDumpIndexMySQL()
    {
      columns.clear();
    }

};

class DrizzleDumpFieldMySQL : public DrizzleDumpField
{
  public:
    DrizzleDumpFieldMySQL(std::string &field, DrizzleDumpConnection *connection)
    : DrizzleDumpField(field, connection)
    { }

    ~DrizzleDumpFieldMySQL() { }

    void dateTimeConvert(void);
    void setCollate(const char* newCollate);
    void setType(const char* raw_type, const char* collation);
};

class DrizzleDumpTableMySQL : public DrizzleDumpTable
{
  public:
    DrizzleDumpTableMySQL(std::string &table, DrizzleDumpConnection *connection)
    : DrizzleDumpTable(table, connection)
    { }

    ~DrizzleDumpTableMySQL()
    {
      fields.clear();
      indexes.clear();
    }

    bool populateFields();
    bool populateIndexes();
    bool populateFkeys();
    void setEngine(const char* newEngine);
    void setCollate(const char* newCollate);
    DrizzleDumpData* getData(void);
};

class DrizzleDumpDatabaseMySQL : public DrizzleDumpDatabase
{
  public:
    DrizzleDumpDatabaseMySQL(const std::string &database,
      DrizzleDumpConnection *connection)
    : DrizzleDumpDatabase(database, connection)
    { }
    ~DrizzleDumpDatabaseMySQL()
    {
      tables.clear();
    }
    bool populateTables(void);
    bool populateTables(const std::vector<std::string> &table_names);
    void setCollate(const char* newCollate);
};

class DrizzleDumpDataMySQL : public DrizzleDumpData
{
  public:
    DrizzleDumpDataMySQL(DrizzleDumpTable *dataTable,
      DrizzleDumpConnection *connection);
    ~DrizzleDumpDataMySQL();

    /* For 0000-00-00 -> NULL conversion */
    std::string convertDate(const char* oldDate) const;
    /* For xx:xx:xx -> INT conversion */
    long convertTime(const char* oldTime) const;
    std::string checkDateTime(const char* item, uint32_t field) const;
};

#endif /* CLIENT_DRIZZLEDUMP_MYSQL_H */
