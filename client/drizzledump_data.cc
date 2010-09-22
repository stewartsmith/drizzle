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

extern bool opt_no_create_info;
extern bool opt_no_data;
extern bool opt_create_db;
extern bool opt_disable_keys;
extern bool extended_insert;
extern bool opt_replace_into;
extern bool opt_drop; 
extern uint32_t show_progress_size;
extern int connected_server_type;

enum server_type {
  SERVER_MYSQL_FOUND,
  SERVER_DRIZZLE_FOUND,
  SERVER_UNKNOWN_FOUND
};

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
  os << "--" << std::endl
     << "-- Current Database: `" << obj.databaseName << "`" << std::endl
     << "--" << std::endl << std::endl;

  /* Love that this variable is the opposite of its name */
  if (not opt_create_db)
  {
    os << "CREATE DATABASE IF NOT EXISTS `" << obj.databaseName
      << "` COLLATE = " << obj.collate << ";" << std::endl << std::endl;
  }

  os << "USE `" << obj.databaseName << "`;" << std::endl << std::endl;

  std::vector<DrizzleDumpTable*>::iterator i;
  std::vector<DrizzleDumpTable*> output_tables = obj.tables;
  for (i= output_tables.begin(); i != output_tables.end(); ++i)
  {
    DrizzleDumpTable *table= *i;
    if (not opt_no_create_info)
      os << *table;
    if (not opt_no_data)
    {
      DrizzleDumpData *data= table->getData();
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

  if (drizzle_result_row_count(obj.result) < 1)
  {
    os << "--" << std::endl
       << "-- No data to dump for table `" << obj.table->tableName << "`" << std::endl
       << "--" << std::endl << std::endl;
    return os;
  }
  else
  {
    os << "--" << std::endl
       << "-- Dumping data for table `" << obj.table->tableName << "`" << std::endl
       << "--" << std::endl << std::endl;
  }
  if (opt_disable_keys)
    os << "ALTER TABLE `" << obj.table->tableName << "` DISABLE KEYS;" << std::endl;

  std::streampos out_position= os.tellp();

  while((row= drizzle_row_next(obj.result)))
  {
    rownr++;
    if ((rownr % show_progress_size) == 0)
    {
      std::cerr << "-- %" << rownr << _(" rows dumped for table ") << obj.table->tableName << std::endl;
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
      os << "INTO `" << obj.table->tableName << "` VALUES (";
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
     << "-- Table structure for table `" << obj.tableName << "`" << std::endl
     << "--" << std::endl << std::endl;

  if (opt_drop)
    os << "DROP TABLE IF EXISTS `" << obj.tableName <<  "`;" << std::endl;

  os << "CREATE TABLE `" << obj.tableName << "` (" << std::endl;
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
  if ((connected_server_type == SERVER_MYSQL_FOUND) and (obj.autoIncrement > 0))
    os << "AUTO_INCREMENT=" << obj.autoIncrement << " ";

  os << "COLLATE = " << obj.collate << ";" << std::endl << std::endl;

  return os;
}
