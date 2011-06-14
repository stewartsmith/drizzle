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
#include "drizzledump_mysql.h"
#include "client_priv.h"
#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <drizzled/gettext.h>

extern bool verbose;
extern bool ignore_errors;

bool DrizzleDumpDatabaseMySQL::populateTables()
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;

  if (not dcon->setDB(databaseName))
    return false;

  if (verbose)
    std::cerr << _("-- Retrieving table structures for ") << databaseName << "..." << std::endl;

  query="SELECT TABLE_NAME, TABLE_COLLATION, ENGINE, AUTO_INCREMENT, TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_TYPE != 'VIEW' AND TABLE_SCHEMA='";
  query.append(databaseName);
  query.append("' ORDER BY TABLE_NAME");

  result= dcon->query(query);

  if (result == NULL)
    return false;

  while ((row= drizzle_row_next(result)))
  {
    size_t* row_sizes= drizzle_row_field_sizes(result);
    std::string tableName(row[0]);
    std::string displayName(tableName);
    cleanTableName(displayName);
    if (not ignoreTable(displayName))
      continue;

    DrizzleDumpTableMySQL *table = new DrizzleDumpTableMySQL(tableName, dcon);
    table->displayName= displayName;
    table->setCollate(row[1]);
    table->setEngine(row[2]);
    if (row[3])
      table->autoIncrement= boost::lexical_cast<uint64_t>(row[3]);
    else
      table->autoIncrement= 0;

    if ((row[4]) and (strstr(row[4], "InnoDB free") == NULL))
      table->comment= DrizzleDumpData::escape(row[4], row_sizes[4]);
    else
      table->comment= "";

    table->database= this;
    if ((not table->populateFields()) or (not table->populateIndexes()) or
     (not table->populateFkeys()))
    {
      delete table;
      if (not ignore_errors)
        return false;
      else
        continue;
    }
    tables.push_back(table);
  }

  dcon->freeResult(result);

  return true;
}

bool DrizzleDumpDatabaseMySQL::populateTables(const std::vector<std::string> &table_names)
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;

  if (not dcon->setDB(databaseName))
    return false;

  if (verbose)
    std::cerr << _("-- Retrieving table structures for ") << databaseName << "..." << std::endl;
  for (std::vector<std::string>::const_iterator it= table_names.begin(); it != table_names.end(); ++it)
  {
    std::string tableName= *it;
    std::string displayName(tableName);
    cleanTableName(displayName);
    if (not ignoreTable(displayName))
      continue;

    query="SELECT TABLE_NAME, TABLE_COLLATION, ENGINE, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='";
    query.append(databaseName);
    query.append("' AND TABLE_NAME = '");
    query.append(tableName);
    query.append("'");

    result= dcon->query(query);

    if (result == NULL)
      return false;

    if ((row= drizzle_row_next(result)))
    {
      DrizzleDumpTableMySQL *table = new DrizzleDumpTableMySQL(tableName, dcon);
      table->displayName= displayName;
      table->setCollate(row[1]);
      table->setEngine(row[2]);
      if (row[3])
        table->autoIncrement= boost::lexical_cast<uint64_t>(row[3]);
      else
        table->autoIncrement= 0;

      table->database= this;
      if ((not table->populateFields()) or (not table->populateIndexes()))
      {
        delete table;
        if (not ignore_errors)
          return false;
        else
          continue;
      }
      tables.push_back(table);
      dcon->freeResult(result);
    }
    else
    {
      dcon->freeResult(result);
      if (not ignore_errors)
        return false;
      else
        continue;
    }
  }

  return true;

}

bool DrizzleDumpTableMySQL::populateFields()
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;

  if (verbose)
    std::cerr << _("-- Retrieving fields for ") << tableName << "..." << std::endl;

  query="SELECT COLUMN_NAME, COLUMN_TYPE, COLUMN_DEFAULT, IS_NULLABLE, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, COLLATION_NAME, EXTRA, COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='";
  query.append(database->databaseName);
  query.append("' AND TABLE_NAME='");
  query.append(tableName);
  query.append("' ORDER BY ORDINAL_POSITION");

  result= dcon->query(query);

  if (result == NULL)
    return false;

  while ((row= drizzle_row_next(result)))
  {
    std::string fieldName(row[0]);
    DrizzleDumpFieldMySQL *field = new DrizzleDumpFieldMySQL(fieldName, dcon);
    /* Stop valgrind warning */
    field->convertDateTime= false;
    field->isNull= (strcmp(row[3], "YES") == 0) ? true : false;
    /* Also sets collation */
    field->setType(row[1], row[8]);
    if (field->type.compare("ENUM") == 0)
      field->isNull= true;

    if ((row[2]) and (field->type.compare("TEXT") != 0))
    {
      field->defaultValue= row[2];
      if (field->convertDateTime)
      {
        field->dateTimeConvert();
      }
    }
    else
    {
      field->defaultValue= "";
    }

    field->isAutoIncrement= (strcmp(row[8], "auto_increment") == 0) ? true : false;
    field->defaultIsNull= field->isNull;

    /* Seriously MySQL, why is BIT length in NUMERIC_PRECISION? */
    if ((strncmp(row[1], "bit", 3) == 0) and (row[5] != NULL))
      field->length= ((boost::lexical_cast<uint32_t>(row[5]) - 1) / 8) + 1;
    else
      field->length= (row[4]) ? boost::lexical_cast<uint32_t>(row[4]) : 0;

    /* Also, CHAR(0) is valid?? */
    if (((field->type.compare("VARBINARY") == 0) 
      or (field->type.compare("VARCHAR") == 0))
      and (field->length == 0))
    {
      field->length= 1;
    }

    field->decimalPrecision= (row[5]) ? boost::lexical_cast<uint32_t>(row[5]) : 0;
    field->decimalScale= (row[6]) ? boost::lexical_cast<uint32_t>(row[6]) : 0;
    field->comment= (row[9]) ? row[9] : "";
    fields.push_back(field);
  }

  dcon->freeResult(result);
  return true;
}


void DrizzleDumpFieldMySQL::dateTimeConvert(void)
{
  boost::match_flag_type flags = boost::match_default;

  if (strcmp(defaultValue.c_str(), "CURRENT_TIMESTAMP") == 0)
    return;

  if (type.compare("INT") == 0)
  {
    /* We were a TIME, now we are an INT */
    std::string ts(defaultValue);
    boost::posix_time::time_duration td(boost::posix_time::duration_from_string(ts));
    defaultValue= boost::lexical_cast<std::string>(td.total_seconds());
    return;
  }

  boost::regex date_regex("(0000|-00)");

  if (regex_search(defaultValue, date_regex, flags))
  {
    defaultIsNull= true;
    defaultValue="";
  }
}


bool DrizzleDumpTableMySQL::populateIndexes()
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;
  std::string lastKey;
  bool firstIndex= true;
  DrizzleDumpIndex *index;

  if (verbose)
    std::cerr << _("-- Retrieving indexes for ") << tableName << "..." << std::endl;

  query="SHOW INDEXES FROM ";
  query.append(tableName);

  result= dcon->query(query);

  if (result == NULL)
    return false;

  while ((row= drizzle_row_next(result)))
  {
    std::string indexName(row[2]);
    if (indexName.compare(lastKey) != 0)
    {
      if (strcmp(row[10], "FULLTEXT") == 0)
        continue;

      if (!firstIndex)
        indexes.push_back(index);
      index = new DrizzleDumpIndexMySQL(indexName, dcon);
      index->isPrimary= (strcmp(row[2], "PRIMARY") == 0);
      index->isUnique= (strcmp(row[1], "0") == 0);
      index->isHash= (strcmp(row[10], "HASH") == 0);
      lastKey= row[2];
      firstIndex= false;
    }
    uint32_t length= (row[7]) ? boost::lexical_cast<uint32_t>(row[7]) : 0;
    index->columns.push_back(std::make_pair(row[4], length));
  }
  if (!firstIndex)
    indexes.push_back(index);

  dcon->freeResult(result);
  return true;
}

bool DrizzleDumpTableMySQL::populateFkeys()
{
  drizzle_result_st *result;
  drizzle_row_t row;
  std::string query;
  DrizzleDumpForeignKey *fkey;

  if (verbose)
    std::cerr << _("-- Retrieving foreign keys for ") << tableName << "..." << std::endl;

  query= "SHOW CREATE TABLE `";
  query.append(database->databaseName);
  query.append("`.`");
  query.append(tableName);
  query.append("`");
  result= dcon->query(query);

  if (result == NULL)
    return false;

  if ((row= drizzle_row_next(result)))
  {
    boost::match_flag_type flags = boost::match_default;
    boost::regex constraint_regex("CONSTRAINT `(.*?)` FOREIGN KEY \\((.*?)\\) REFERENCES `(.*?)` \\((.*?)\\)( ON (UPDATE|DELETE) (CASCADE|RESTRICT|SET NULL))?( ON (UPDATE|DELETE) (CASCADE|RESTRICT|SET NULL))?");

    boost::match_results<std::string::const_iterator> constraint_results;

    std::string search_body(row[1]);
    std::string::const_iterator start, end;
    start= search_body.begin();
    end= search_body.end();
    while (regex_search(start, end, constraint_results, constraint_regex, flags))
    {
      fkey= new DrizzleDumpForeignKey(constraint_results[1], dcon);
      fkey->parentColumns= constraint_results[2];
      fkey->childTable= constraint_results[3];
      fkey->childColumns= constraint_results[4];
        
      if (constraint_results[5].compare("") != 0)
      {
        if (constraint_results[6].compare("UPDATE") == 0)
          fkey->updateRule= constraint_results[7];
        else if (constraint_results[6].compare("DELETE") == 0)
          fkey->deleteRule= constraint_results[7];
      }
      if (constraint_results[8].compare("") != 0)
      {
        if (constraint_results[9].compare("UPDATE") == 0)
          fkey->updateRule= constraint_results[10];
        else if (constraint_results[9].compare("DELETE") == 0)
          fkey->deleteRule= constraint_results[10];
      }
      fkey->matchOption= "";

      fkeys.push_back(fkey);

      start= constraint_results[0].second;
      flags |= boost::match_prev_avail; 
      flags |= boost::match_not_bob;
    }
  }
  dcon->freeResult(result);
  return true;
}

void DrizzleDumpFieldMySQL::setType(const char* raw_type, const char* raw_collation)
{
  std::string old_type(raw_type);
  std::string extra;
  size_t pos;
  
  if (((pos= old_type.find("(")) != std::string::npos) or
    ((pos= old_type.find(" ")) != std::string::npos))
  {
    extra= old_type.substr(pos);
    old_type.erase(pos, std::string::npos);
  }

  std::transform(old_type.begin(), old_type.end(), old_type.begin(), ::toupper);
  if ((old_type.find("CHAR") != std::string::npos) or 
    (old_type.find("TEXT") != std::string::npos))
    setCollate(raw_collation);

  if ((old_type.compare("BIGINT") == 0) and
    ((extra.find("unsigned") != std::string::npos)))
  {
    rangeCheck= true;
  }

  if ((old_type.compare("INT") == 0) and 
    ((extra.find("unsigned") != std::string::npos)))
  {
    type= "BIGINT";
    return;
  }
    
  if ((old_type.compare("TINYINT") == 0) or
    (old_type.compare("SMALLINT") == 0) or
    (old_type.compare("MEDIUMINT") == 0))
  {
    type= "INT";
    return;
  }

  if ((old_type.compare("TINYBLOB") == 0) or
    (old_type.compare("MEDIUMBLOB") == 0) or
    (old_type.compare("LONGBLOB") == 0))
  {
    type= "BLOB";
    return;
  }

  if ((old_type.compare("TINYTEXT") == 0) or
    (old_type.compare("MEDIUMTEXT") == 0) or
    (old_type.compare("LONGTEXT") == 0) or
    (old_type.compare("SET") == 0))
  {
    type= "TEXT";
    return;
  }

  if (old_type.compare("CHAR") == 0)
  {
    type= "VARCHAR";
    return;
  }

  if (old_type.compare("BINARY") == 0)
  {
    type= "VARBINARY";
    
    return;
  }

  if (old_type.compare("ENUM") == 0)
  {
    type= old_type;
    /* Strip out the braces, we add them again during output */
    enumValues= extra.substr(1, extra.length()-2);
    return;
  }

  if ((old_type.find("TIME") != std::string::npos) or
    (old_type.find("DATE") != std::string::npos))
  {
    /* Intended to catch TIME/DATE/TIMESTAMP/DATETIME 
       We may have a default TIME/DATE which needs converting */
    convertDateTime= true;
    isNull= true;
  }

  if ((old_type.compare("TIME") == 0) or (old_type.compare("YEAR") == 0))
  {
    type= "INT";
    return;
  }

  if (old_type.compare("FLOAT") == 0)
  {
    type= "DOUBLE";
    return;
  }

  if (old_type.compare("BIT") == 0)
  {
    type= "VARBINARY";
    return;
  }

  type= old_type;
  return;
}

void DrizzleDumpTableMySQL::setEngine(const char* newEngine)
{
  if ((strcmp(newEngine, "MyISAM") == 0) || (strcmp(newEngine, "MEMORY") == 0))
    engineName= "InnoDB";
  else
    engineName= newEngine; 
}

DrizzleDumpData* DrizzleDumpTableMySQL::getData(void)
{
  try
  {
    return new DrizzleDumpDataMySQL(this, dcon);
  }
  catch(...)
  {
    return NULL;
  }
}

void DrizzleDumpDatabaseMySQL::setCollate(const char* newCollate)
{
  if (newCollate)
  {
    std::string tmpCollate(newCollate);
    if (tmpCollate.find("utf8") != std::string::npos)
    {
      collate= tmpCollate;
      return;
    }
  }
  collate= "utf8_general_ci";
}

void DrizzleDumpTableMySQL::setCollate(const char* newCollate)
{
  if (newCollate)
  {
    std::string tmpCollate(newCollate);
    if (tmpCollate.find("utf8") != std::string::npos)
    {
      collate= tmpCollate;
      return;
    }
  }

  collate= "utf8_general_ci";
}

void DrizzleDumpFieldMySQL::setCollate(const char* newCollate)
{
  if (newCollate)
  {
    std::string tmpCollate(newCollate);
    if (tmpCollate.find("utf8") != std::string::npos)
    {
      collation= tmpCollate;
      return;
    }
  }
  collation= "utf8_general_ci";
}

DrizzleDumpDataMySQL::DrizzleDumpDataMySQL(DrizzleDumpTable *dataTable,
  DrizzleDumpConnection *connection)
  : DrizzleDumpData(dataTable, connection)
{
  std::string query;
  query= "SELECT * FROM `";
  query.append(table->displayName);
  query.append("`");

  result= dcon->query(query);
  if (result == NULL)
    throw std::exception();
}

DrizzleDumpDataMySQL::~DrizzleDumpDataMySQL()
{
  drizzle_result_free(result);
  delete result;
}

long DrizzleDumpDataMySQL::convertTime(const char* oldTime) const
{
  std::string ts(oldTime);
  boost::posix_time::time_duration td(boost::posix_time::duration_from_string(ts));
  long seconds= td.total_seconds();
  return seconds;
}

std::string DrizzleDumpDataMySQL::convertDate(const char* oldDate) const
{
  boost::match_flag_type flags = boost::match_default;
  std::string output;
  boost::regex date_regex("(0000|-00)");

  if (not regex_search(oldDate, date_regex, flags))
  {
    output.push_back('\'');
    output.append(oldDate);
    output.push_back('\'');
  }
  else
    output= "NULL";

  return output;
}

std::string DrizzleDumpDataMySQL::checkDateTime(const char* item, uint32_t field) const
{
  std::string ret;

  if (table->fields[field]->convertDateTime)
  {
    if (table->fields[field]->type.compare("INT") == 0)
      ret= boost::lexical_cast<std::string>(convertTime(item));
    else
      ret= convertDate(item);
  }
  return ret;
}

