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

class DrizzleDumpField
{
  private:
   std::string fieldName;

  public:
   std::stringstream errmsg;

   DrizzleDumpField() :
    fieldName()
   { }

  enum field_types
  {
    FIELD_TYPE_INT,
    FIELD_TYPE_BIGINT,
    FIELD_TYPE_VARCHAR,
    FIELD_TYPE_VARBINARY,
    FIELD_TYPE_ENUM,
    FIELD_TYPE_BLOB,
    FIELD_TYPE_DECIMAL,
    FIELD_TYPE_DOUBLE,
    FIELD_TYPE_DATE,
    FIELD_TYPE_DATETIME,
    FIELD_TYPE_TIMESTAMP,
    FIELD_TYPE_NONE
  };

  enum field_types type;
  uint32_t length;
  bool isNull;
  bool isUnsigned;
  bool isAutoIncrement;
  std::string defaultValue;
  bool quoteDefault;
  std::string collation;

  /* For enum type */
  std::string enumValues;

  /* For decimal/double */
  uint32_t decimalPrecision;
  uint32_t decimalScale;
};

class DrizzleDumpTable
{
  private:
   const std::string tableName;

  public:
   std::stringstream errmsg;

   DrizzleDumpTable(std::string &table) :
    tableName(table)
   { }
   bool populateFields(drizzle_con_st &connection);
   std::vector<DrizzleDumpField*> fields;

   const std::string& getName() const { return tableName; }
};

class DrizzleDumpDatabase
{
  private:
   const std::string databaseName;
   std::string collate;

  public:
   std::stringstream errmsg;

   DrizzleDumpDatabase(const std::string &database) :
    databaseName(database)
   { }

   friend std::ostream& operator <<(std::ostream &os, const DrizzleDumpDatabase &obj);

   bool populateTables(drizzle_con_st &connection);
   std::vector<DrizzleDumpTable*> tables;

   const std::string& getName() const { return databaseName; }
   const std::string& getCollate() const { return collate; }
   void setCollate(std::string new_collate) { collate= new_collate; }
};

#endif /* CLIENT_DRIZZLEDUMP_H */
