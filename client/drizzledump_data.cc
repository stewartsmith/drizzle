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
#include "client_priv.h"
#include <drizzled/definitions.h>
#include <drizzled/gettext.h>
#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/unordered_set.hpp>
#include <boost/lexical_cast.hpp>

#define EX_DRIZZLEERR 2

extern bool opt_no_create_info;
extern bool opt_no_data;
extern bool opt_create_db;
extern bool opt_disable_keys;
extern bool extended_insert;
extern bool opt_replace_into;
extern bool opt_drop;
extern bool verbose;
extern bool opt_databases;
extern bool opt_alldbs;
extern uint32_t show_progress_size;
extern bool opt_ignore;
extern bool opt_compress;
extern bool opt_drop_database;
extern bool opt_autocommit;
extern bool ignore_errors;
extern std::string opt_destination_database;

extern boost::unordered_set<std::string> ignore_table;
extern void maybe_exit(int error);

enum destinations {
  DESTINATION_DB,
  DESTINATION_FILES,
  DESTINATION_STDOUT
};

extern int opt_destination;

// returns true on keep, false on ignore Olaf: sounds backwards/wrong, shouldn't it be the other way around?
bool DrizzleDumpDatabase::ignoreTable(std::string tableName)
{
  return ignore_table.find(databaseName + "." + tableName) == ignore_table.end();
}

void DrizzleDumpDatabase::cleanTableName(std::string &tableName)
{
  std::string replace("``");
  std::string find("`");
  size_t j = 0;
  for (;(j = tableName.find(find, j)) != std::string::npos;)
  {
    tableName.replace(j, find.length(), replace);
    j+= replace.length();
  }

}

std::ostream& operator <<(std::ostream &os, const DrizzleDumpForeignKey &obj)
{
  os << "  CONSTRAINT `" << obj.constraintName << "` FOREIGN KEY ("
    << obj.parentColumns << ") REFERENCES `" << obj.childTable << "` ("
    << obj.childColumns << ")";

  if (not obj.deleteRule.empty())
    os << " ON DELETE " << obj.deleteRule;

  if (not obj.updateRule.empty())
    os << " ON UPDATE " << obj.updateRule;

  return os;
}

std::ostream& operator <<(std::ostream &os, const DrizzleDumpIndex &obj)
{
  if (obj.isPrimary)
  {
    os << "  PRIMARY KEY ";
  }
  else if (obj.isUnique)
  {
    os << "  UNIQUE KEY `" << obj.indexName << "` ";
  }
  else
  {
    os << "  KEY `" << obj.indexName << "` ";
  }

  os << "(";
  
  std::vector<DrizzleDumpIndex::columnData>::iterator i;
  std::vector<DrizzleDumpIndex::columnData> fields = obj.columns;
  for (i= fields.begin(); i != fields.end(); ++i)
  {
    if (i != fields.begin())
      os << ",";
    os << "`" << (*i).first << "`";
    if ((*i).second > 0)
      os << "(" << (*i).second << ")";
  }

  os << ")";

  return os;
}

std::ostream& operator <<(std::ostream &os, const DrizzleDumpField &obj)
{
  os << "  `" << obj.fieldName << "` ";
  os << obj.type;
  if (((obj.type.compare("VARCHAR") == 0) or
   (obj.type.compare("VARBINARY") == 0)) and
   (obj.length > 0))
  {
    os << "(" << obj.length << ")";
  }
  else if (((obj.type.compare("DECIMAL") == 0) or
    (obj.type.compare("DOUBLE") == 0)) and
    ((obj.decimalPrecision + obj.decimalScale) > 0))
  {
    os << "(" << obj.decimalPrecision << "," << obj.decimalScale << ")";
  }
  else if (obj.type.compare("ENUM") == 0)
  {
    os << "(" << obj.enumValues << ")";
  }

  if (not obj.isNull)
  {
    os << " NOT NULL";
  }

  if ((not obj.collation.empty()) and (obj.collation.compare("binary") != 0))
  {
    os << " COLLATE " << obj.collation;
  }

  if (obj.isAutoIncrement)
    os << " AUTO_INCREMENT";

  if (not obj.defaultValue.empty())
  {
    if (obj.defaultValue.compare("CURRENT_TIMESTAMP") != 0)
    {
      if (obj.defaultValue.compare(0, 2, "b'") == 0)
      {
        os << " DEFAULT " << obj.defaultValue;
      }
      else
      {
        os << " DEFAULT '" << obj.defaultValue << "'";
      }
    }
    else
    {
     os << " DEFAULT CURRENT_TIMESTAMP";
    }
  }
  else if ((obj.defaultIsNull))
  {
    os << " DEFAULT NULL";
  }

  if (not obj.comment.empty())
  {
    os << " COMMENT '" << DrizzleDumpData::escape(obj.comment.c_str(), obj.comment.length()) << "'";
  }

  return os;
}

std::ostream& operator <<(std::ostream &os, const DrizzleDumpDatabase &obj)
{
  if ((opt_destination == DESTINATION_DB) or opt_databases or opt_alldbs)
  {
    if (verbose)
    {
      std::cerr << "--" << std::endl
        << "-- Current Database: `" << obj.databaseName << "`" << std::endl
        << "--" << std::endl << std::endl;
    }

    /* Love that this variable is the opposite of its name */
    if (not opt_create_db)
    {
      if (opt_drop_database)
      {
        os << "DROP DATABASE IF EXISTS `"
          << ((opt_destination_database.empty()) ? obj.databaseName
          : opt_destination_database) << "`" << std::endl;
      }

      os << "CREATE DATABASE IF NOT EXISTS `"
        << ((opt_destination_database.empty()) ? obj.databaseName
        : opt_destination_database) << "`";
      if (not obj.collate.empty())
       os << " COLLATE = " << obj.collate;

      os << ";" << std::endl << std::endl;
    }
    os << "USE `" << ((opt_destination_database.empty()) ? obj.databaseName
      : opt_destination_database) << "`;" << std::endl << std::endl;
  }

  std::vector<DrizzleDumpTable*>::iterator i;
  std::vector<DrizzleDumpTable*> output_tables = obj.tables;
  for (i= output_tables.begin(); i != output_tables.end(); ++i)
  {
    DrizzleDumpTable *table= *i;
    if (not opt_no_create_info)
      os << *table;
    if (not opt_no_data)
    {
      obj.dcon->setDB(obj.databaseName);
      DrizzleDumpData *data= table->getData();
      if (data == NULL)
      {
        std::cerr << "Error: Could not get data for table " << table->displayName << std::endl;
        if (not ignore_errors)
          maybe_exit(EX_DRIZZLEERR);
        else
          continue;
      }
      os << *data;
      delete data;
    }
  }

  return os;
}


std::ostream& operator <<(std::ostream &os, const DrizzleDumpData &obj)
{
  bool new_insert= true;
  bool first= true;
  uint64_t rownr= 0;
  size_t byte_counter= 0;

  drizzle_row_t row;

  if (verbose)
    std::cerr << _("-- Retrieving data for ") << obj.table->displayName << "..." << std::endl;

  if (drizzle_result_row_count(obj.result) < 1)
  {
    if (verbose)
    {
      std::cerr << "--" << std::endl
        << "-- No data to dump for table `" << obj.table->displayName << "`"
        << std::endl << "--" << std::endl << std::endl;
    }
    return os;
  }
  else if (verbose)
  {
    std::cerr << "--" << std::endl
      << "-- Dumping data for table `" << obj.table->displayName << "`"
      << std::endl << "--" << std::endl << std::endl;
  }
  if (opt_disable_keys)
    os << "ALTER TABLE `" << obj.table->displayName << "` DISABLE KEYS;" << std::endl;

  /* Another option that does the opposite of its name, makes me sad :( */
  if (opt_autocommit)
    os << "START TRANSACTION;" << std::endl;

  while((row= drizzle_row_next(obj.result)))
  {
    rownr++;
    if (verbose and (rownr % show_progress_size) == 0)
    {
      std::cerr << "-- " << rownr << _(" rows dumped for table ") << obj.table->displayName << std::endl;
    }

    size_t* row_sizes= drizzle_row_field_sizes(obj.result);
    for (uint32_t i= 0; i < drizzle_result_column_count(obj.result); i++)
      byte_counter+= row_sizes[i];

    if (not first and not new_insert)
    {
      if (extended_insert)
        os << "),(";
      else
        os << ");" << std::endl;
      byte_counter+= 3;
    }
    else
      first= false;

    if (new_insert)
    {
      if (opt_replace_into)
        os << "REPLACE ";
      else
      {
        os << "INSERT ";
        if (opt_ignore)
          os << "IGNORE ";
      }
      os << "INTO `" << obj.table->displayName << "` VALUES (";
      byte_counter+= 28 + obj.table->displayName.length();
      if (extended_insert)
        new_insert= false;
    }
    for (uint32_t i= 0; i < drizzle_result_column_count(obj.result); i++)
    {
      if (not row[i])
      {
        os << "NULL";
        if (i != obj.table->fields.size() - 1)
          os << ",";
        continue;
      }

      if ((obj.table->fields[i]->rangeCheck) and
        (obj.table->fields[i]->type.compare("BIGINT") == 0) and
        (boost::lexical_cast<uint64_t>(row[i]) > INT64_MAX))
      {
        std::cerr << "Error: Data for column " << obj.table->fields[i]->fieldName << " is greater than max BIGINT, cannot migrate automatically" << std::endl;
        if (not ignore_errors)
          maybe_exit(EX_DRIZZLEERR);
        else
          continue;
      }

      /* time/date conversion for MySQL connections */
      else if (obj.table->fields[i]->convertDateTime)
      {
        os << obj.checkDateTime(row[i], i);
      }
      else
      {
        if ((obj.table->fields[i]->type.compare("INT") != 0) and
          (obj.table->fields[i]->type.compare("BIGINT") != 0))
        {
          /* Hex blob processing or escape text */
          if (((obj.table->fields[i]->type.compare("BLOB") == 0) or
            (obj.table->fields[i]->type.compare("VARBINARY") == 0)))
          {
            os << obj.convertHex((unsigned char*)row[i], row_sizes[i]);
            byte_counter+= row_sizes[i];
          }
          else if ((obj.table->fields[i]->type.compare("ENUM") == 0) and
            (strcmp(row[i], "") == 0))
          {
            os << "NULL";
          }
          else if (obj.table->fields[i]->type.compare("BOOLEAN") == 0)
          {
            if (strncmp(row[i], "1", 1) == 0)
              os << "TRUE";
            else
              os << "FALSE";
          }
          else
            os << "'" << DrizzleDumpData::escape(row[i], row_sizes[i]) << "'";
          byte_counter+= 3;
        }
        else
          os << row[i];
      }
      if (i != obj.table->fields.size() - 1)
        os << ",";
    }
    /* Break insert up if it is too long */
    if ((extended_insert and
      (byte_counter >= DRIZZLE_MAX_LINE_LENGTH)) or (not extended_insert))
    {
      os << ");" << std::endl;
      new_insert= true;
      byte_counter= 0;
    }
  }
  if (not new_insert)
    os << ");" << std::endl;

  if (opt_autocommit)
    os << "COMMIT;" << std::endl;

  if (opt_disable_keys)
    os << "ALTER TABLE `" << obj.table->tableName << "` ENABLE KEYS;" << std::endl;

  os << std::endl;

  return os;
}

std::string DrizzleDumpData::convertHex(const unsigned char* from, size_t from_size) const
{
  std::ostringstream output;
  if (from_size > 0)
    output << "0x";
  else
    output << "''";

  while (from_size > 0)
  {
    /* Would be nice if std::hex liked uint8_t, ah well */
    output << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (unsigned short)(*from);
    (void) *from++;
    from_size--;
  }

  return output.str();
}

/* Ripped out of libdrizzle, hopefully a little safer */
std::string DrizzleDumpData::escape(const char* from, size_t from_size)
{
  std::string output;

  while (from_size > 0)
  {
    if (!(*from & 0x80))
    {
      switch (*from)
      {
         case 0:
           output.append("\\0");
           break;
         case '\n':
           output.append("\\n");
           break;
         case '\r':
           output.append("\\r");
           break;
         case '\\':
           output.append("\\\\");
           break;
         case '\'':
           output.append("\\'");
           break;
         case '"':
           output.append("\\\"");
           break;
         case '\032':
           output.append("\\Z");
           break;
         default:
           output.push_back(*from);
           break;
       }
    }
    else
      output.push_back(*from);
    (void) *from++;
    from_size--;
  }

  return output;
}

std::ostream& operator <<(std::ostream &os, const DrizzleDumpTable &obj)
{
  if (verbose)
  {
    std::cerr << "--" << std::endl
      << "-- Table structure for table `" << obj.displayName << "`" << std::endl
      << "--" << std::endl << std::endl;
  }

  if (opt_drop)
    os << "DROP TABLE IF EXISTS `" << obj.displayName <<  "`;" << std::endl;

  os << "CREATE TABLE `" << obj.displayName << "` (" << std::endl;
  std::vector<DrizzleDumpField*>::iterator i;
  std::vector<DrizzleDumpField*> output_fields = obj.fields;
  for (i= output_fields.begin(); i != output_fields.end(); ++i)
  {
    if (i != output_fields.begin())
      os << "," << std::endl;
    DrizzleDumpField *field= *i;
    os << *field;
  }

  std::vector<DrizzleDumpIndex*>::iterator j;
  std::vector<DrizzleDumpIndex*> output_indexes = obj.indexes;
  for (j= output_indexes.begin(); j != output_indexes.end(); ++j)
  {
    os << "," << std::endl;
    DrizzleDumpIndex *index= *j;
    os << *index;
  }

  std::vector<DrizzleDumpForeignKey*>::iterator k;
  std::vector<DrizzleDumpForeignKey*> output_fkeys = obj.fkeys;
  for (k= output_fkeys.begin(); k != output_fkeys.end(); ++k)
  {
    os << "," << std::endl;
    DrizzleDumpForeignKey *fkey= *k;
    os << *fkey;
  }

  os << std::endl;
  os << ") ENGINE='" << obj.engineName << "' ";
  if (obj.autoIncrement > 0)
  {
    os << "AUTO_INCREMENT=" << obj.autoIncrement << " ";
  }

  os << "COLLATE='" << obj.collate << "'";

  if (not obj.comment.empty())
  {
    os << " COMMENT='" << obj.comment << "'";
  }

  if (not obj.replicate)
  {
    os << " REPLICATE=FALSE";
  }

  os << ";" << std::endl << std::endl;

  return os;
}

DrizzleDumpConnection::DrizzleDumpConnection(std::string &host, uint16_t port, 
  std::string &username, std::string &password, bool drizzle_protocol) :
  hostName(host),
  drizzleProtocol(drizzle_protocol)
{
  drizzle_return_t ret;

  if (host.empty())
    host= "localhost";

  std::string protocol= (drizzle_protocol) ? "Drizzle" : "MySQL";
  if (verbose)
  {
    std::cerr << _("-- Connecting to ") << host  << _(" using protocol ")
      << protocol << "..." << std::endl;
  }
  drizzle_create(&drizzle);
  drizzle_con_create(&drizzle, &connection);
  drizzle_con_set_tcp(&connection, (char *)host.c_str(), port);
  drizzle_con_set_auth(&connection, (char *)username.c_str(),
    (char *)password.c_str());
  drizzle_con_add_options(&connection, 
    drizzle_protocol ? DRIZZLE_CON_EXPERIMENTAL : DRIZZLE_CON_MYSQL);
  ret= drizzle_con_connect(&connection);
  if (ret != DRIZZLE_RETURN_OK)
  {
    errorHandler(NULL, ret, "when trying to connect");
    throw std::exception();
  }

  ServerDetect server_detect= ServerDetect(&connection);

  serverType= server_detect.getServerType();
  serverVersion= server_detect.getServerVersion();
}

drizzle_result_st* DrizzleDumpConnection::query(std::string &str_query)
{
  drizzle_return_t ret;
  drizzle_result_st* result= new drizzle_result_st;
  if (drizzle_query_str(&connection, result, str_query.c_str(), &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      std::cerr << _("Error executing query: ") <<
        drizzle_result_error(result) << std::endl;
      drizzle_result_free(result);
    }
    else
    {
      std::cerr << _("Error executing query: ") <<
        drizzle_con_error(&connection) << std::endl;
    }
    return NULL;
  }

  if (drizzle_result_buffer(result) != DRIZZLE_RETURN_OK)
  {
    std::cerr << _("Could not buffer result: ") <<
        drizzle_con_error(&connection) << std::endl;
    return NULL;
  }
  return result;
}

void DrizzleDumpConnection::freeResult(drizzle_result_st* result)
{
  drizzle_result_free(result);
  delete result;
}

bool DrizzleDumpConnection::queryNoResult(std::string &str_query)
{
  drizzle_return_t ret;
  drizzle_result_st result;

  if (drizzle_query_str(&connection, &result, str_query.c_str(), &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      std::cerr << _("Error executing query: ") <<
        drizzle_result_error(&result) << std::endl;
      drizzle_result_free(&result);
    }
    else
    {
      std::cerr << _("Error executing query: ") <<
        drizzle_con_error(&connection) << std::endl;
    }
    return false;
  }

  drizzle_result_free(&result);
  return true;
}

bool DrizzleDumpConnection::setDB(std::string databaseName)
{
  drizzle_return_t ret;
  drizzle_result_st result;
  if (drizzle_select_db(&connection, &result, databaseName.c_str(), &ret) == 
    NULL || ret != DRIZZLE_RETURN_OK)
  {
    std::cerr << _("Error: Could not set db '") << databaseName << "'" << std::endl;
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
      drizzle_result_free(&result);
    return false;
  }
  drizzle_result_free(&result);
  return true;
}

void DrizzleDumpConnection::errorHandler(drizzle_result_st *res,
  drizzle_return_t ret, const char *when)
{
  if (res == NULL)
  {
    std::cerr << _("Got error: ") << drizzle_con_error(&connection) << " "
      << when << std::endl;
  }
  else if (ret == DRIZZLE_RETURN_ERROR_CODE)
  {
    std::cerr << _("Got error: ") << drizzle_result_error(res)
      << " (" << drizzle_result_error_code(res) << ") " << when << std::endl;
    drizzle_result_free(res);
  }
  else
  {
    std::cerr << _("Got error: ") << ret << " " << when << std::endl;
  }

  return;
}

DrizzleDumpConnection::~DrizzleDumpConnection()
{
  if (verbose)
    std::cerr << _("-- Disconnecting from ") << hostName << "..." << std::endl;
  drizzle_con_free(&connection);
  drizzle_free(&drizzle);
}
