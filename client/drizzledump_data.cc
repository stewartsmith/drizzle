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
#include <drizzled/gettext.h>
#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/unordered_set.hpp>

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

extern boost::unordered_set<std::string> ignore_table;
extern void maybe_exit(int error);

enum destinations {
  DESTINATION_DB,
  DESTINATION_FILES,
  DESTINATION_STDOUT
};

extern int opt_destination;

/* returns true on keep, false on ignore */
bool DrizzleDumpDatabase::ignoreTable(std::string tableName)
{
  std::string dbTable(databaseName);
  dbTable.append(".");
  dbTable.append(tableName);

  boost::unordered_set<std::string>::iterator iter= ignore_table.find(dbTable);
  return (iter == ignore_table.end());
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
  
  std::vector<std::string>::iterator i;
  std::vector<std::string> fields = obj.columns;
  for (i= fields.begin(); i != fields.end(); ++i)
  {
    if (i != fields.begin())
      os << ",";
    std::string field= *i;
    os << "`" << field << "`";
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
  else if ((obj.type.compare("DECIMAL") == 0) or
    (obj.type.compare("DOUBLE") == 0))
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
     os << " DEFAULT '" << obj.defaultValue << "'";
    else
     os << " DEFAULT CURRENT_TIMESTAMP";
  }
  else if ((obj.collation.empty()) and (obj.defaultIsNull))
  {
    os << " DEFAULT NULL";
  }

  return os;
}

std::ostream& operator <<(std::ostream &os, const DrizzleDumpDatabase &obj)
{
  if ((opt_destination == DESTINATION_DB) or opt_databases or opt_alldbs)
  {
    os << "--" << std::endl
       << "-- Current Database: `" << obj.databaseName << "`" << std::endl
       << "--" << std::endl << std::endl;

    /* Love that this variable is the opposite of its name */
    if (not opt_create_db)
    {
      os << "CREATE DATABASE IF NOT EXISTS `" << obj.databaseName << "`";
      if (not obj.collate.empty())
       os << " COLLATE = " << obj.collate;

      os << ";" << std::endl << std::endl;
    }

    os << "USE `" << obj.databaseName << "`;" << std::endl << std::endl;
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
        maybe_exit(EX_DRIZZLEERR);
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

  drizzle_row_t row;

  if (verbose)
    std::cerr << _("-- Retrieving data for ") << obj.table->displayName << "..." << std::endl;

  if (drizzle_result_row_count(obj.result) < 1)
  {
    os << "--" << std::endl
       << "-- No data to dump for table `" << obj.table->displayName << "`" << std::endl
       << "--" << std::endl << std::endl;
    return os;
  }
  else
  {
    os << "--" << std::endl
       << "-- Dumping data for table `" << obj.table->displayName << "`" << std::endl
       << "--" << std::endl << std::endl;
  }
  if (opt_disable_keys)
    os << "ALTER TABLE `" << obj.table->displayName << "` DISABLE KEYS;" << std::endl;

  std::streampos out_position= os.tellp();

  while((row= drizzle_row_next(obj.result)))
  {
    rownr++;
    if ((rownr % show_progress_size) == 0)
    {
      std::cerr << "-- %" << rownr << _(" rows dumped for table ") << obj.table->displayName << std::endl;
    }

    size_t* row_sizes= drizzle_row_field_sizes(obj.result);
    if (not first)
    {
      if (extended_insert)
        os << "),(";
      else
        os << ");" << std::endl;
    }
    else
      first= false;

    if (new_insert)
    {
      if (opt_replace_into)
        os << "REPLACE ";
      else
        os << "INSERT ";
      os << "INTO `" << obj.table->displayName << "` VALUES (";
      if (extended_insert)
        new_insert= false;
    }
    for (uint32_t i= 0; i < drizzle_result_column_count(obj.result); i++)
    {
      if (not row[i])
      {
        os << "NULL";
      }
      /* time/date conversion for MySQL connections */
      else if (obj.table->fields[i]->convertDateTime)
      {
        os << obj.checkDateTime(os, row[i], i);
      }
      else
      {
        if (obj.table->fields[i]->type.compare("INT") != 0)
        {
          /* Hex blob processing or escape text */
          if (((obj.table->fields[i]->type.compare("BLOB") == 0) or
            (obj.table->fields[i]->type.compare("VARBINARY") == 0)))
            os << obj.convertHex(row[i], row_sizes[i]);
          else
            os << "'" << obj.escape(row[i], row_sizes[i]) << "'";
        }
        else
          os << row[i];
      }
      if (i != obj.table->fields.size() - 1)
        os << ",";
    }
    /* Break insert up if it is too long */
    if (extended_insert and
      ((os.tellp() - out_position) >= DRIZZLE_MAX_LINE_LENGTH))
    {
      os << ");" << std::endl;
      new_insert= true;
      out_position= os.tellp();
    }
  }
  os << ");" << std::endl;

  if (opt_disable_keys)
    os << "ALTER TABLE `" << obj.table->tableName << "` ENABLE KEYS;" << std::endl;

  os << std::endl;

  return os;
}

std::string DrizzleDumpData::convertHex(const char* from, size_t from_size) const
{
  std::ostringstream output;
  if (from_size > 0)
    output << "0x";
  while (from_size > 0)
  {
    output << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << int(*from);
    *from++;
    from_size--;
  }

  return output.str();
}

/* Ripped out of libdrizzle, hopefully a little safer */
std::string DrizzleDumpData::escape(const char* from, size_t from_size) const
{
  std::string output;

  while (from_size > 0)
  {
    if (!(*from & 0x80))
    {
      switch (*from)
      {
         case 0:
         case '\n':
         case '\r':
         case '\\':
         case '\'':
         case '"':
         case '\032':
           output.push_back('\\');
         default:
           break;
       }
    }
    output.push_back(*from);
    *from++;
    from_size--;
  }

  return output;
}

std::ostream& operator <<(std::ostream &os, const DrizzleDumpTable &obj)
{
  os << "--" << std::endl
     << "-- Table structure for table `" << obj.displayName << "`" << std::endl
     << "--" << std::endl << std::endl;

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
    os << "," << std::endl;;
    DrizzleDumpIndex *index= *j;
    os << *index;
  }
  os << std::endl;
  os << ") ENGINE=" << obj.engineName << " ";
  if ((obj.dcon->getServerType() == DrizzleDumpConnection::SERVER_MYSQL_FOUND)
    and (obj.autoIncrement > 0))
  {
    os << "AUTO_INCREMENT=" << obj.autoIncrement << " ";
  }

  os << "COLLATE = " << obj.collate << ";" << std::endl << std::endl;

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
    throw;
  }

  boost::match_flag_type flags = boost::match_default; 

  boost::regex mysql_regex("(5\\.[0-9]+\\.[0-9]+)");
  boost::regex drizzle_regex("(20[0-9]{2}\\.(0[1-9]|1[012])\\.[0-9]+)");

  std::string version(getServerVersion());

  if (regex_search(version, mysql_regex, flags))
    serverType= SERVER_MYSQL_FOUND;
  else if (regex_search(version, drizzle_regex, flags))
    serverType= SERVER_DRIZZLE_FOUND;
  else
    serverType= SERVER_UNKNOWN_FOUND;
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

void DrizzleDumpConnection::errorHandler(drizzle_result_st *res, drizzle_return_t ret,
                     const char *when)
{
  if (ret == DRIZZLE_RETURN_ERROR_CODE)
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
