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

#include "drizzledump_data.h"
#include "drizzledump_drizzle.h"
#include "client_priv.h"
#include <string>
#include <iostream>
#include <drizzled/gettext.h>
#include <boost/lexical_cast.hpp>

bool DrizzleDumpDatabaseDrizzle::populateTables()
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;

  dcon->setDB(databaseName);

  query="SELECT TABLE_NAME, TABLE_COLLATION, ENGINE FROM DATA_DICTIONARY.TABLES WHERE TABLE_SCHEMA='";
  query.append(databaseName);
  query.append("' ORDER BY TABLE_NAME");

  result= dcon->query(query);

  while ((row= drizzle_row_next(result)))
  {
    std::string tableName(row[0]);
    DrizzleDumpTable *table = new DrizzleDumpTableDrizzle(tableName, dcon);
    table->collate= row[1];
    table->engineName= row[2];
    table->autoIncrement= 0;
    table->database= this;
    table->populateFields();
    table->populateIndexes();
    tables.push_back(table);
  }

  dcon->freeResult(result);

  return true;
}

void DrizzleDumpDatabaseDrizzle::setCollate(const char* newCollate)
{
  if (newCollate)
    collate= newCollate;
  else
    collate= "utf8_general_ci";
}

bool DrizzleDumpTableDrizzle::populateFields()
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;

  query= "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT, COLUMN_DEFAULT_IS_NULL, IS_NULLABLE, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, COLLATION_NAME, IS_AUTO_INCREMENT, ENUM_VALUES FROM DATA_DICTIONARY.COLUMNS WHERE TABLE_SCHEMA='";
  query.append(database->databaseName);
  query.append("' AND TABLE_NAME='");
  query.append(tableName);
  query.append("' ORDER BY ORDINAL_POSITION");

  result= dcon->query(query);

  while ((row= drizzle_row_next(result)))
  {
    std::string fieldName(row[0]);
    DrizzleDumpField *field = new DrizzleDumpFieldDrizzle(fieldName, dcon);
    /* Stop valgrind warning */
    field->convertDateTime= false;
    /* Also sets collation */
    field->setType(row[1], row[8]);
    if (row[2])
      field->defaultValue= row[2];
    else
      field->defaultValue= "";

    field->isNull= (strcmp(row[4], "YES") == 0) ? true : false;
    field->isAutoIncrement= (strcmp(row[9], "YES") == 0) ? true : false;
    field->defaultIsNull= (strcmp(row[3], "YES") == 0) ? true : false;
    field->enumValues= (row[10]) ? row[10] : "";
    field->length= (row[5]) ? boost::lexical_cast<uint32_t>(row[5]) : 0;
    field->decimalPrecision= (row[6]) ? boost::lexical_cast<uint32_t>(row[6]) : 0;
    field->decimalScale= (row[7]) ? boost::lexical_cast<uint32_t>(row[7]) : 0;


    fields.push_back(field);
  }

  dcon->freeResult(result);
  return true;
}


bool DrizzleDumpTableDrizzle::populateIndexes()
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;
  std::string lastKey;
  bool firstIndex= true;
  DrizzleDumpIndex *index;

  query= "SELECT INDEX_NAME, COLUMN_NAME, IS_USED_IN_PRIMARY, IS_UNIQUE FROM DATA_DICTIONARY.INDEX_PARTS WHERE TABLE_NAME='";
  query.append(tableName);
  query.append("'");

  result= dcon->query(query);

  while ((row= drizzle_row_next(result)))
  {
    std::string indexName(row[0]);
    if (indexName.compare(lastKey) != 0)
    {
      if (!firstIndex)
        indexes.push_back(index);
      index = new DrizzleDumpIndexDrizzle(indexName, dcon);
      index->isPrimary= (strcmp(row[0], "PRIMARY") == 0);
      index->isUnique= (strcmp(row[3], "YES") == 0);
      index->isHash= 0;
      lastKey= row[0];
      firstIndex= false;
    }
    index->columns.push_back(row[1]);
  }
  if (!firstIndex)
    indexes.push_back(index);

  dcon->freeResult(result);
  return true;
}

DrizzleDumpData* DrizzleDumpTableDrizzle::getData(void)
{
  return new DrizzleDumpDataDrizzle(this, dcon);
}


void DrizzleDumpFieldDrizzle::setType(const char* raw_type, const char* raw_collation)
{
  collation= raw_collation;
  if (strcmp(raw_type, "BLOB") == 0)
  {
    if (strcmp(raw_collation, "binary") != 0)
      type= "TEXT";
    else
      type= raw_type;
    return;
  }

  if (strcmp(raw_type, "VARCHAR") == 0)
  {
    if (strcmp(raw_collation, "binary") != 0)
      type= "VARCHAR";
    else
      type= "VARBINARY";
    return;
  }

  if (strcmp(raw_type, "INTEGER") == 0)
  {
    type= "INT";
    return;
  }

  type= raw_type;
}

DrizzleDumpDataDrizzle::DrizzleDumpDataDrizzle(DrizzleDumpTable *dataTable,
  DrizzleDumpConnection *connection)
  : DrizzleDumpData(dataTable, connection)
{
  std::string query;
  query= "SELECT * FROM `";
  query.append(table->tableName);
  query.append("`");

  result= dcon->query(query);
}

DrizzleDumpDataDrizzle::~DrizzleDumpDataDrizzle()
{
  dcon->freeResult(result);
}
