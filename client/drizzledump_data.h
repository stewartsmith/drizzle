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

#ifndef CLIENT_DRIZZLEDUMP_DATA_H
#define CLIENT_DRIZZLEDUMP_DATA_H

#define DRIZZLE_MAX_LINE_LENGTH 1024*1024L-1025
#include "client_priv.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

class DrizzleDumpDatabase;
class DrizzleDumpData;

class DrizzleDumpIndex
{
  public:
    std::string indexName;

    DrizzleDumpIndex(std::string &index) :
      indexName(index)
    { }

    virtual ~DrizzleDumpIndex() { }

    bool isPrimary;
    bool isUnique;
    bool isHash;

    std::vector<std::string> columns;
    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpIndex &obj);
};

class DrizzleDumpField
{
  public:
    DrizzleDumpField(std::string &field) :
      fieldName(field)
    { }

    virtual ~DrizzleDumpField() { }

    std::stringstream errmsg;

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpField &obj);
    std::string fieldName;

    std::string type;
    uint32_t length;
    bool isNull;
    bool isUnsigned;
    bool isAutoIncrement;
    bool defaultIsNull;
    bool convertDateTime;
    std::string defaultValue;
    std::string collation;

    /* For enum type */
    std::string enumValues;

    /* For decimal/double */
    uint32_t decimalPrecision;
    uint32_t decimalScale;

    virtual void setType(const char*, const char*) { }

};

class DrizzleDumpTable
{
  public:
    DrizzleDumpTable(std::string &table) :
      tableName(table)
    { }

    virtual ~DrizzleDumpTable() { }

    std::stringstream errmsg;

    virtual bool populateFields() { return false; }
    virtual bool populateIndexes() { return false; }
    virtual DrizzleDumpData* getData() { return NULL; }
    std::vector<DrizzleDumpField*> fields;
    std::vector<DrizzleDumpIndex*> indexes;

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpTable &obj);
    std::string tableName;
    std::string engineName;
    std::string collate;

    // Currently MySQL only, hard to do in Drizzle
    uint64_t autoIncrement;
    DrizzleDumpDatabase* database;
};

class DrizzleDumpDatabase
{
  public:
    DrizzleDumpDatabase(const std::string &database) :
      databaseName(database)
    { }

    virtual ~DrizzleDumpDatabase() { }

    std::stringstream errmsg;

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpDatabase &obj);

    virtual bool populateTables() { return false; }
    virtual void setCollate(const char*) { }
    std::vector<DrizzleDumpTable*> tables;

    const std::string databaseName;
    std::string collate;
};

class DrizzleDumpData
{
  public:
    std::stringstream errmsg;
    DrizzleDumpTable *table;
    drizzle_result_st *result;
    DrizzleDumpData(DrizzleDumpTable *dataTable) :
      table(dataTable)
    { }

    virtual ~DrizzleDumpData() { }
    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpData &obj);

    virtual std::ostream& checkDateTime(std::ostream &os, const char*, uint32_t) const { return os; }
    std::string convertHex(const char* from, size_t from_size) const;
    std::string escape(const char* from, size_t from_size) const;
};

#endif /* CLIENT_DRIZZLEDUMP_DATA_H */
