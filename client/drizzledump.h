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

#ifndef CLIENT_DRIZZLEDUMP_H
#define CLIENT_DRIZZLEDUMP_H

class DrizzleDumpDatabase;

class DrizzleDumpIndex
{
  public:
    std::string indexName;

    DrizzleDumpIndex(std::string &index) :
      indexName(index)
    { }

    bool isPrimary;
    bool isUnique;
    bool isHash;

    std::vector<std::string> columns;
    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpIndex &obj);
};

class DrizzleDumpField
{
  public:
    std::stringstream errmsg;

    DrizzleDumpField(std::string &field) :
      fieldName(field)
    { }

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpField &obj);
    std::string fieldName;

    std::string type;
    uint32_t length;
    bool isNull;
    bool isUnsigned;
    bool isAutoIncrement;
    bool isPrimary;
    bool defaultIsNull;
    std::string defaultValue;
    std::string collation;

    /* For enum type */
    std::string enumValues;

    /* For decimal/double */
    uint32_t decimalPrecision;
    uint32_t decimalScale;

    void setCollate(const char* newCollate);
    void setType(const char* raw_type, const char* collation);
};

class DrizzleDumpTable
{
  public:
    std::stringstream errmsg;

    DrizzleDumpTable(std::string &table) :
      tableName(table)
    { }
    bool populateFields(drizzle_con_st &connection);
    bool populateIndexes(drizzle_con_st &connection);
    std::vector<DrizzleDumpField*> fields;
    std::vector<DrizzleDumpIndex*> indexes;

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpTable &obj);
    std::string tableName;
    std::string engineName;
    std::string collate;

    void setCollate(const char* newCollate);
    void setEngine(const char* newEngine);

    // Currently MySQL only, hard to do in Drizzle
    uint64_t autoIncrement;
    DrizzleDumpDatabase* database;
};

class DrizzleDumpDatabase
{
  public:
    std::stringstream errmsg;

    DrizzleDumpDatabase(const std::string &database) :
      databaseName(database)
    { }

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpDatabase &obj);

    bool populateTables(drizzle_con_st &connection);
    std::vector<DrizzleDumpTable*> tables;

    void setCollate(const char* newCollate);
    const std::string databaseName;
    std::string collate;
};

#endif /* CLIENT_DRIZZLEDUMP_H */
