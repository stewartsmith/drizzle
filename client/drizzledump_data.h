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
#include "server_detect.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <algorithm>

class DrizzleDumpConnection;
class DrizzleDumpDatabase;
class DrizzleDumpData;

class DrizzleDumpForeignKey
{
  public:
    DrizzleDumpConnection *dcon;
    std::string constraintName;

    DrizzleDumpForeignKey(std::string name, DrizzleDumpConnection* connection) :
      dcon(connection),
      constraintName(name)
    { }

    virtual ~DrizzleDumpForeignKey() { }

    std::string parentColumns;
    std::string childColumns;
    std::string childTable;
    std::string matchOption;
    std::string deleteRule;
    std::string updateRule;

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpForeignKey &obj);
};

class DrizzleDumpIndex
{
  public:
    DrizzleDumpConnection *dcon;
    std::string indexName;

    DrizzleDumpIndex(std::string &index, DrizzleDumpConnection* connection) :
      dcon(connection),
      indexName(index),
      isPrimary(false),
      isUnique(false),
      isHash(false)
    { }

    virtual ~DrizzleDumpIndex() { }

    bool isPrimary;
    bool isUnique;
    bool isHash;

    /* Stores the column name and the length of the index part */
    typedef std::pair<std::string,uint32_t> columnData;
    std::vector<columnData> columns;
    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpIndex &obj);
};

class DrizzleDumpField
{
  public:
    DrizzleDumpField(std::string &field, DrizzleDumpConnection* connection) :
      dcon(connection),
      fieldName(field),
      isNull(false),
      isUnsigned(false),
      isAutoIncrement(false),
      defaultIsNull(false),
      convertDateTime(false),
      rangeCheck(false)
    { }

    virtual ~DrizzleDumpField() { }
    DrizzleDumpConnection *dcon;

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
    bool rangeCheck;
    std::string defaultValue;
    std::string collation;
    std::string comment;

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
    DrizzleDumpTable(std::string &table, DrizzleDumpConnection* connection) :
      dcon(connection),
      tableName(table),
      replicate(true),
      database(NULL)
    { }

    virtual ~DrizzleDumpTable() { }
    DrizzleDumpConnection *dcon;

    std::stringstream errmsg;

    virtual bool populateFields() { return false; }
    virtual bool populateIndexes() { return false; }
    virtual bool populateFkeys() { return false; }
    virtual DrizzleDumpData* getData() { return NULL; }
    std::vector<DrizzleDumpField*> fields;
    std::vector<DrizzleDumpIndex*> indexes;
    std::vector<DrizzleDumpForeignKey*> fkeys;

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpTable &obj);
    std::string tableName;
    std::string displayName;
    std::string engineName;
    std::string collate;
    std::string comment;
    bool replicate;

    // Currently MySQL only, hard to do in Drizzle
    uint64_t autoIncrement;
    DrizzleDumpDatabase* database;
};

class DrizzleDumpDatabase
{
  public:
    DrizzleDumpDatabase(const std::string &database, DrizzleDumpConnection* connection) :
      dcon(connection),
      databaseName(database)
    { }
    DrizzleDumpConnection *dcon;

    virtual ~DrizzleDumpDatabase() { }

    std::stringstream errmsg;

    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpDatabase &obj);

    virtual bool populateTables(void) { return false; }
    virtual bool populateTables(const std::vector<std::string> &table_names) { return table_names.empty(); }
    virtual void setCollate(const char*) { }
    void cleanTableName(std::string &tableName);
    bool ignoreTable(std::string tableName);
    std::vector<DrizzleDumpTable*> tables;

    const std::string databaseName;
    std::string collate;
};

class DrizzleDumpData
{
  public:
    DrizzleDumpConnection *dcon;
    std::stringstream errmsg;
    DrizzleDumpTable *table;
    drizzle_result_st *result;
    DrizzleDumpData(DrizzleDumpTable *dataTable, DrizzleDumpConnection *connection) :
      dcon(connection),
      table(dataTable),
      result(NULL)
    { }

    virtual ~DrizzleDumpData() { }
    friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpData &obj);

    virtual std::string checkDateTime(const char*, uint32_t) const { return std::string(""); }
    std::string convertHex(const unsigned char* from, size_t from_size) const;
    static std::string escape(const char* from, size_t from_size);
};

class DrizzleDumpConnection
{
  private:
    drizzle_st drizzle;
    drizzle_con_st connection;
    std::string hostName;
    bool drizzleProtocol;
    ServerDetect::server_type serverType;
    std::string serverVersion;

  public:
    DrizzleDumpConnection(std::string &host, uint16_t port,
      std::string &username, std::string &password, bool drizzle_protocol);
    ~DrizzleDumpConnection();
    void errorHandler(drizzle_result_st *res,  drizzle_return_t ret, const char *when);
    drizzle_result_st* query(std::string &str_query);
    bool queryNoResult(std::string &str_query);

    drizzle_result_st* query(const char* ch_query)
    {
      std::string str_query(ch_query);
      return query(str_query);
    }
    bool queryNoResult(const char* ch_query)
    {
      std::string str_query(ch_query);
      return queryNoResult(str_query);
    }

    void freeResult(drizzle_result_st* result);
    bool setDB(std::string databaseName);
    bool usingDrizzleProtocol(void) const { return drizzleProtocol; }
    bool getServerType(void) const { return serverType; }
    std::string getServerVersion(void) const { return serverVersion; }
};

class DrizzleStringBuf : public std::streambuf
{
  public:
    DrizzleStringBuf(int size) :
      buffSize(size)
    {
      resize= 1;
      ptr.resize(buffSize);
      setp(&ptr[0], &ptr.back());
    }
    virtual ~DrizzleStringBuf() 
    {
        sync();
    }

    void writeString(std::string &str)
    {
      if (not connection->queryNoResult(str))
        throw std::exception();
    }

    void setConnection(DrizzleDumpConnection *conn) { connection= conn; }

  private:
    DrizzleDumpConnection *connection;
    size_t buffSize;
    uint32_t resize;
    std::vector<char> ptr;

    int	overflow(int c)
    {
        if (c != EOF)
        {
          size_t len = size_t(pptr() - pbase());
          resize++;
          ptr.resize(buffSize*resize);
          setp(&ptr[0], &ptr.back());
          /* setp resets current pointer, put it back */
          pbump(len);
          sputc(c);
        }

        return 0;
    }

    int	sync()
    {
      size_t len = size_t(pptr() - pbase());
      std::string temp(pbase(), len);

      /* Drop newlines */
      temp.erase(std::remove(temp.begin(), temp.end(), '\n'), temp.end());

      if (temp.compare(0, 2, "--") == 0)
      {
        /* Drop comments */
        setp(pbase(), epptr());
      }
      if (temp.find(";") != std::string::npos)
      {
        writeString(temp);
        setp(pbase(), epptr());
      }
      return 0;
    }
};

#endif /* CLIENT_DRIZZLEDUMP_DATA_H */
