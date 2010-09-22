/* Copyright 2000-2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
 * Copyright (C) 2010 Vijay Samuel
 * Copyright (C) 2010 Andrew Hutchings

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* drizzledump.cc  - Dump a tables contents and format to an ASCII file

 * Derived from mysqldump, which originally came from:
 **
 ** The author's original notes follow :-
 **
 ** AUTHOR: Igor Romanenko (igor@frog.kiev.ua)
 ** DATE:   December 3, 1994
 ** WARRANTY: None, expressed, impressed, implied
 **          or other
 ** STATUS: Public domain

 * and more work by Monty, Jani & Sinisa
 * and all the MySQL developers over the years.
*/

#include "client_priv.h"
#include <string>
#include <iostream>
#include <stdarg.h>
#include <boost/unordered_set.hpp>
#include <algorithm>
#include <fstream>
#include <drizzled/gettext.h>
#include <drizzled/configmake.h>
#include <drizzled/error.h>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "drizzledump.h"

using namespace std;
using namespace drizzled;
namespace po= boost::program_options;

/* Exit codes */

#define EX_USAGE 1
#define EX_DRIZZLEERR 2
#define EX_CONSCHECK 3
#define EX_EOM 4
#define EX_EOF 5 /* ferror for output file was got */
#define EX_ILLEGAL_TABLE 6
#define EX_TABLE_STATUS 7

/* index into 'show fields from table' */

#define SHOW_FIELDNAME  0
#define SHOW_TYPE  1
#define SHOW_NULL  2
#define SHOW_DEFAULT  4
#define SHOW_EXTRA  5

/* Size of buffer for dump's select query */
#define QUERY_LENGTH 1536
#define DRIZZLE_MAX_LINE_LENGTH 1024*1024L-1025

/* ignore table flags */
#define IGNORE_NONE 0x00 /* no ignore */
#define IGNORE_DATA 0x01 /* don't dump data for this table */
#define IGNORE_INSERT_DELAYED 0x02 /* table doesn't support INSERT DELAYED */

static bool  verbose= false;
static bool opt_no_create_info;
static bool opt_no_data= false;
static bool use_drizzle_protocol= false;
static bool quick= true;
static bool extended_insert= true;
static bool ignore_errors= false;
static bool flush_logs= false;
static bool opt_drop= true; 
static bool opt_keywords= false;
static bool opt_compress= false;
static bool opt_delayed= false; 
static bool create_options= true; 
static bool opt_quoted= false;
static bool opt_databases= false; 
static bool opt_alldbs= false; 
static bool opt_create_db= false;
static bool opt_lock_all_tables= false;
static bool opt_set_charset= false; 
static bool opt_dump_date= true;
static bool opt_autocommit= false; 
static bool opt_disable_keys= true;
static bool opt_single_transaction= false; 
static bool opt_comments;
static bool opt_compact;
static bool opt_hex_blob= false;
static bool opt_order_by_primary=false; 
static bool opt_ignore= false;
static bool opt_complete_insert= false;
static bool opt_drop_database;
static bool opt_replace_into= false;
static bool opt_alltspcs= false;
static uint32_t show_progress_size= 0;
//static uint64_t total_rows= 0;
static drizzle_st drizzle;
static drizzle_con_st dcon;
static string insert_pat;
//static char *order_by= NULL;
static uint32_t opt_drizzle_port= 0;
static int first_error= 0;
static string extended_row;
FILE *md_result_file= 0;
FILE *stderror_file= 0;
std::vector<DrizzleDumpDatabase*> database_store;

const string progname= "drizzledump";

string password,
  enclosed,
  escaped,
  current_host,
  opt_enclosed,
  fields_terminated,
  path,
  lines_terminated,
  current_user,
  opt_password,
  opt_protocol,
  where;

//static const CHARSET_INFO *charset_info= &my_charset_utf8_general_ci;

boost::unordered_set<string> ignore_table;

static void maybe_exit(int error);
static void die(int error, const char* reason, ...);
static void maybe_die(int error, const char* reason, ...);
static void write_header(FILE *sql_file, char *db_name);
static int dump_selected_tables(const string &db, const vector<string> &table_names);
static int dump_databases(const vector<string> &db_names);
static int dump_all_databases(void);
char check_if_ignore_table(const char *table_name, char *table_type);
int get_server_type();
void dump_all_tables(void);
void generate_dump(void);

enum server_type {
  SERVER_MYSQL_FOUND,
  SERVER_DRIZZLE_FOUND,
  SERVER_UNKNOWN_FOUND
};

int connected_server_type= SERVER_UNKNOWN_FOUND;

bool DrizzleDumpDatabase::populateTables(drizzle_con_st &connection)
{
  drizzle_result_st result;
  drizzle_row_t row;
  drizzle_return_t ret;
  std::string query;

  if (drizzle_select_db(&connection, &result, databaseName.c_str(), &ret) == 
    NULL || ret != DRIZZLE_RETURN_OK)
  {
    errmsg << _("Could not set db '") << databaseName << "'";
    return false;
  }
  drizzle_result_free(&result);

  if (connected_server_type == SERVER_MYSQL_FOUND)
    query="SELECT TABLE_NAME, TABLE_COLLATION, ENGINE, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='";
  else
    query="SELECT TABLE_NAME, TABLE_COLLATION, ENGINE FROM DATA_DICTIONARY.TABLES WHERE TABLE_SCHEMA='";

  query.append(databaseName);
  query.append("' ORDER BY TABLE_NAME");

  if (drizzle_query_str(&connection, &result, query.c_str(), &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_result_error(&result);
      drizzle_result_free(&result);
    }
    else
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(&connection);
    }
    return false;
  }

  if (drizzle_result_buffer(&result) != DRIZZLE_RETURN_OK)
  {
    errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(&connection);
    return false;
  }

  while ((row= drizzle_row_next(&result)))
  {
    std::string tableName(row[0]);
    DrizzleDumpTable *table = new DrizzleDumpTable(tableName);
    table->setCollate(row[1]);
    table->setEngine(row[2]);
    if (connected_server_type == SERVER_MYSQL_FOUND)
    {
      if (row[3])
        table->autoIncrement= boost::lexical_cast<uint64_t>(row[3]);
      else
        table->autoIncrement= 0;
    }

    table->database= this;
    table->populateFields(connection);
    table->populateIndexes(connection);
    tables.push_back(table);
  }

  drizzle_result_free(&result);

  return true;
}

bool DrizzleDumpTable::populateFields(drizzle_con_st &connection)
{
  drizzle_result_st result;
  drizzle_row_t row;
  drizzle_return_t ret;
  std::string query;

  if (connected_server_type == SERVER_MYSQL_FOUND)
    query="SELECT COLUMN_NAME, COLUMN_TYPE, COLUMN_DEFAULT, 'NO', IS_NULLABLE, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, COLLATION_NAME, EXTRA FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='";
  else
    query= "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT, COLUMN_DEFAULT_IS_NULL, IS_NULLABLE, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, COLLATION_NAME, IS_AUTO_INCREMENT, ENUM_VALUES FROM DATA_DICTIONARY.COLUMNS WHERE TABLE_SCHEMA='";
  query.append(database->databaseName);
  query.append("' AND TABLE_NAME='");
  query.append(tableName);
  query.append("' ORDER BY ORDINAL_POSITION");

  if (drizzle_query_str(&connection, &result, query.c_str(), &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_result_error(&result);
      drizzle_result_free(&result);
    }
    else
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(&connection);
    }
    return false;
  }

  if (drizzle_result_buffer(&result) != DRIZZLE_RETURN_OK)
  {
    errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(&connection);
    return false;
  }
  while ((row= drizzle_row_next(&result)))
  {
    std::string fieldName(row[0]);
    DrizzleDumpField *field = new DrizzleDumpField(fieldName);
    /* Stop valgrind warning */
    field->convertDateTime= false;
    /* Also sets collation */
    field->setType(row[1], row[8]);
    if (row[2])
    {
      if (field->convertDateTime)
      {
        field->dateTimeConvert(row[2]);
      }
      else
        field->defaultValue= row[2];
    }
    else
     field->defaultValue= "";

    field->isNull= (strcmp(row[4], "YES") == 0) ? true : false;
    if (connected_server_type == SERVER_MYSQL_FOUND)
    {
      field->isAutoIncrement= (strcmp(row[9], "auto_increment") == 0) ? true : false;
      field->defaultIsNull= field->isNull;
    }
    else
    {
      field->isAutoIncrement= (strcmp(row[9], "YES") == 0) ? true : false;
      field->defaultIsNull= (strcmp(row[3], "YES") == 0) ? true : false;
      field->enumValues= (row[10]) ? row[10] : "";
    }
    field->length= (row[5]) ? boost::lexical_cast<uint32_t>(row[5]) : 0;
    field->decimalPrecision= (row[6]) ? boost::lexical_cast<uint32_t>(row[6]) : 0;
    field->decimalScale= (row[7]) ? boost::lexical_cast<uint32_t>(row[7]) : 0;


    fields.push_back(field);
  }

  drizzle_result_free(&result);
  return true;
}

void DrizzleDumpField::dateTimeConvert(const char* oldDefault)
{
  boost::match_flag_type flags = boost::match_default;

  if (strcmp(oldDefault, "CURRENT_TIMESTAMP") == 0)
  {
    defaultValue= oldDefault;
    return;
  }

  if (type.compare("INT") == 0)
  {
    /* We were a TIME, now we are an INT */
    std::string ts(oldDefault);
    boost::posix_time::time_duration td(boost::posix_time::duration_from_string(ts));
    defaultValue= boost::lexical_cast<string>(td.total_seconds());
    return;
  }

  boost::regex date_regex("([0-9]{3}[1-9]-(0[1-9]|1[012])-(0[1-9]|[12][0-9]|3[01]))");

  if (regex_search(oldDefault, date_regex, flags))
  {
    defaultValue= oldDefault;
  }
  else
  {
    defaultIsNull= true;
    defaultValue="";
  }

}

bool DrizzleDumpTable::populateIndexes(drizzle_con_st &connection)
{
  drizzle_result_st result;
  drizzle_row_t row;
  drizzle_return_t ret;
  std::string query;
  std::string lastKey;
  bool firstIndex= true;
  DrizzleDumpIndex *index;

  if (connected_server_type == SERVER_MYSQL_FOUND)
    query="SHOW INDEXES FROM ";
  else
    query= "SELECT INDEX_NAME, COLUMN_NAME, IS_USED_IN_PRIMARY, IS_UNIQUE FROM DATA_DICTIONARY.INDEX_PARTS WHERE TABLE_NAME='";
  query.append(tableName);
  if (connected_server_type == SERVER_DRIZZLE_FOUND)
    query.append("'");

  if (drizzle_query_str(&connection, &result, query.c_str(), &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_result_error(&result);
      drizzle_result_free(&result);
    }
    else
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(&connection);
    }
    return false;
  }

  if (drizzle_result_buffer(&result) != DRIZZLE_RETURN_OK)
  {
    errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(&connection);
    return false;
  }
  while ((row= drizzle_row_next(&result)))
  {
    std::string indexName((connected_server_type == SERVER_MYSQL_FOUND) ? row[2] : row[0]);
    if (indexName.compare(lastKey) != 0)
    {
      if ((connected_server_type == SERVER_MYSQL_FOUND) and
        (strcmp(row[10], "FULLTEXT") == 0))
        continue;

      if (!firstIndex)
        indexes.push_back(index);
      index = new DrizzleDumpIndex(indexName);
      if (connected_server_type == SERVER_MYSQL_FOUND)
      {
        index->isPrimary= (strcmp(row[2], "PRIMARY") == 0);
        index->isUnique= (strcmp(row[1], "0") == 0);
        index->isHash= (strcmp(row[10], "HASH") == 0);
      }
      else
      {
        index->isPrimary= (strcmp(row[0], "PRIMARY") == 0);
        index->isUnique= (strcmp(row[3], "YES") == 0);
        index->isHash= 0;
      }
      lastKey= (connected_server_type == SERVER_MYSQL_FOUND) ? row[2] : row[0];
      firstIndex= false;
    }
    index->columns.push_back((connected_server_type == SERVER_MYSQL_FOUND) ? row[4] : row[1]);
  }
  if (!firstIndex)
    indexes.push_back(index);

  drizzle_result_free(&result);
  return true;
}

void DrizzleDumpField::setType(const char* raw_type, const char* raw_collation)
{
  std::string old_type(raw_type);
  std::string extra;
  size_t pos;
  
  if ((pos= old_type.find("(")) != string::npos)
  {
    extra= old_type.substr(pos);
    old_type.erase(pos,string::npos);
  }

  if (connected_server_type == SERVER_DRIZZLE_FOUND)
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
    return;
  }

  if (connected_server_type == SERVER_MYSQL_FOUND)
  {
    std::transform(old_type.begin(), old_type.end(), old_type.begin(), ::toupper);
    if ((old_type.find("CHAR") != string::npos) or 
      (old_type.find("TEXT") != string::npos))
      setCollate(raw_collation);

    if ((old_type.compare("INT") == 0) and 
      ((extra.find("unsigned") != string::npos)))
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
      (old_type.compare("LONGTEXT") == 0))
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

    if ((old_type.find("TIME") != string::npos) or
      (old_type.find("DATE") != string::npos))
    {
      /* Intended to catch TIME/DATE/TIMESTAMP/DATETIME 
         We may have a default TIME/DATE which needs converting */
      convertDateTime= true;
    }

    if (old_type.compare("TIME") == 0)
    {
      type= "INT";
      return;
    }

    if (old_type.compare("FLOAT") == 0)
    {
      type= "DOUBLE";
      return;
    }

    type= old_type;
    return;
  }
}

void DrizzleDumpTable::setEngine(const char* newEngine)
{
  if (strcmp(newEngine, "MyISAM") == 0)
    engineName= "InnoDB";
  else
    engineName= newEngine; 
}

void DrizzleDumpDatabase::setCollate(const char* newCollate)
{
  if (newCollate)
  {
    std::string tmpCollate(newCollate);
    if (tmpCollate.find("utf8") != string::npos)
    {
      collate= tmpCollate;
      return;
    }
  }
  collate= "utf8_general_ci";
}

void DrizzleDumpTable::setCollate(const char* newCollate)
{
  if (newCollate)
  {
    std::string tmpCollate(newCollate);
    if (tmpCollate.find("utf8") != string::npos)
    {
      collate= tmpCollate;
      return;
    }
  }

  collate= "utf8_general_ci";
}

void DrizzleDumpField::setCollate(const char* newCollate)
{
  if (newCollate)
  {
    std::string tmpCollate(newCollate);
    if (tmpCollate.find("utf8") != string::npos)
    {
      collation= tmpCollate;
      return;
    }
  }
  collation= "utf8_general_ci";
}

ostream& operator <<(ostream &os,const DrizzleDumpIndex &obj)
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

ostream& operator <<(ostream &os, const DrizzleDumpField &obj)
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

ostream& operator <<(ostream &os,const DrizzleDumpDatabase &obj)
{
  os << "--" << endl
     << "-- Current Database: `" << obj.databaseName << "`" << endl
     << "--" << endl << endl;

  /* Love that this variable is the opposite of its name */
  if (not opt_create_db)
  {
    os << "CREATE DATABASE IF NOT EXISTS `" << obj.databaseName
      << "` COLLATE = " << obj.collate << ";" << endl << endl;
  }

  os << "USE `" << obj.databaseName << "`;" << endl << endl;

  std::vector<DrizzleDumpTable*>::iterator i;
  std::vector<DrizzleDumpTable*> output_tables = obj.tables;
  for (i= output_tables.begin(); i != output_tables.end(); ++i)
  {
    DrizzleDumpTable *table= *i;
    if (not opt_no_create_info)
      os << *table;
    if (not opt_no_data)
    {
      DrizzleDumpData *data= new DrizzleDumpData(dcon, table);
      os << *data;
      delete data;
    }
  }

  return os;
}

DrizzleDumpData::DrizzleDumpData(drizzle_con_st &conn, DrizzleDumpTable *dataTable) :
 table(dataTable),
 connection(&conn)
{
  drizzle_return_t ret;
  std::string query;
  query= "SELECT * FROM `";
  query.append(table->tableName);
  query.append("`");
  result= new drizzle_result_st;

  if (drizzle_query_str(connection, result, query.c_str(), &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_result_error(result);
      drizzle_result_free(result);
    }
    else
    {
      errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(connection);
    }
    return;
  }

  if (drizzle_result_buffer(result) != DRIZZLE_RETURN_OK)
  {
    errmsg << _("Could not get tables list due to error: ") <<
        drizzle_con_error(connection);
    return;
  }
}
DrizzleDumpData::~DrizzleDumpData()
{
  drizzle_result_free(result);
  if (result) delete result;
}
ostream& operator <<(ostream &os,const DrizzleDumpData &obj)
{
  bool new_insert= true;
  bool first= true;
  uint64_t rownr= 0;

  drizzle_row_t row;

  if (drizzle_result_row_count(obj.result) < 1)
  {
    os << "--" << endl
       << "-- No data to dump for table `" << obj.table->tableName << "`" << endl
       << "--" << endl << endl;
    return os;
  }
  else
  {
    os << "--" << endl
       << "-- Dumping data for table `" << obj.table->tableName << "`" << endl
       << "--" << endl << endl;
  }
  if (opt_disable_keys)
    os << "ALTER TABLE `" << obj.table->tableName << "` DISABLE KEYS;" << endl;

  streampos out_position= os.tellp();

  while((row= drizzle_row_next(obj.result)))
  {
    rownr++;
    if ((rownr % show_progress_size) == 0)
    {
      cerr << "-- %" << rownr << _(" rows dumped for table ") << obj.table->tableName << endl;
    }

    size_t* row_sizes= drizzle_row_field_sizes(obj.result);
    if (not first)
    {
      if (extended_insert)
        os << "),(";
      else
        os << ");" << endl;
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
      else if ((connected_server_type == SERVER_MYSQL_FOUND) and
        (obj.table->fields[i]->convertDateTime))
      {
        if (obj.table->fields[i]->type.compare("INT") == 0)
          os << obj.convertTime(row[i]);
        else
          os << obj.convertDate(row[i]);
      }
      else
      {
        if (obj.table->fields[i]->type.compare("INT") != 0)
        {
          /* Hex blob processing or escape text */
          if (opt_hex_blob and
            ((obj.table->fields[i]->type.compare("BLOB") == 0) or
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
      os << ");" << endl;
      new_insert= true;
      out_position= os.tellp();
    }
  }
  os << ");" << endl;

  if (opt_disable_keys)
    os << "ALTER TABLE `" << obj.table->tableName << "` ENABLE KEYS;" << endl;

  os << endl;

  return os;
}

long DrizzleDumpData::convertTime(const char* oldTime) const
{
  std::string ts(oldTime);
  boost::posix_time::time_duration td(boost::posix_time::duration_from_string(ts));
  long seconds= td.total_seconds();
  return seconds;
}

std::string DrizzleDumpData::convertDate(const char* oldDate) const
{
  boost::match_flag_type flags = boost::match_default;
  std::string output;
  boost::regex date_regex("([0-9]{3}[1-9]-(0[1-9]|1[012])-(0[1-9]|[12][0-9]|3[01]))");

  if (regex_search(oldDate, date_regex, flags))
  {
    output.push_back('\'');
    output.append(oldDate);
    output.push_back('\'');
  }
  else
    output= "NULL";

  return output;
}

std::string DrizzleDumpData::convertHex(const char* from, size_t from_size) const
{
  ostringstream output;
  if (from_size > 0)
    output << "0x";
  while (from_size > 0)
  {
    output << uppercase << hex << setw(2) << setfill('0') << int(*from);
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

ostream& operator <<(ostream &os,const DrizzleDumpTable &obj)
{
  os << "--" << endl
     << "-- Table structure for table `" << obj.tableName << "`" << endl
     << "--" << endl << endl;

  if (opt_drop)
    os << "DROP TABLE IF EXISTS `" << obj.tableName <<  "`;" << endl;

  os << "CREATE TABLE `" << obj.tableName << "` (" << endl;
  std::vector<DrizzleDumpField*>::iterator i;
  std::vector<DrizzleDumpField*> output_fields = obj.fields;
  for (i= output_fields.begin(); i != output_fields.end(); ++i)
  {
    if (i != output_fields.begin())
      os << "," << endl;
    DrizzleDumpField *field= *i;
    os << *field;
  }

  std::vector<DrizzleDumpIndex*>::iterator j;
  std::vector<DrizzleDumpIndex*> output_indexes = obj.indexes;
  for (j= output_indexes.begin(); j != output_indexes.end(); ++j)
  {
    os << "," << endl;;
    DrizzleDumpIndex *index= *j;
    os << *index;
  }
  os << endl;
  os << ") ENGINE=" << obj.engineName << " ";
  if ((connected_server_type == SERVER_MYSQL_FOUND) and (obj.autoIncrement > 0))
    os << "AUTO_INCREMENT=" << obj.autoIncrement << " ";

  os << "COLLATE = " << obj.collate << ";" << endl << endl;

  return os;
}

void dump_all_tables(void)
{
  std::vector<DrizzleDumpDatabase*>::iterator i;
  for (i= database_store.begin(); i != database_store.end(); ++i)
  {
    (*i)->populateTables(dcon);
  }
}

void generate_dump(void)
{
    std::vector<DrizzleDumpDatabase*>::iterator i;
  for (i= database_store.begin(); i != database_store.end(); ++i)
  {
    DrizzleDumpDatabase *database= *i;
    cout << *database;
  }
}

/*
  Print the supplied message if in verbose mode

  SYNOPSIS
  verbose_msg()
  fmt   format specifier
  ...   variable number of parameters
*/

static void verbose_msg(const char *fmt, ...)
{
  va_list args;


  if (!verbose)
    return;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

/*
  exit with message if ferror(file)

  SYNOPSIS
  check_io()
  file        - checked file
*/

static void check_io(FILE *file)
{
  if (ferror(file))
    die(EX_EOF, _("Got errno %d on write"), errno);
}

static void write_header(FILE *sql_file, char *db_name)
{
  if (! opt_compact)
  { 
    if (opt_comments)
    {
      fprintf(sql_file,
              "-- drizzledump %s libdrizzle %s, for %s-%s (%s)\n--\n",
              VERSION, drizzle_version(), HOST_VENDOR, HOST_OS, HOST_CPU);
      fprintf(sql_file, "-- Host: %s    Database: %s\n",
              ! current_host.empty() ? current_host.c_str() : "localhost", db_name ? db_name :
              "");
      fputs("-- ------------------------------------------------------\n",
            sql_file);
      fprintf(sql_file, "-- Server version\t%s",
              drizzle_con_server_version(&dcon));
      if (connected_server_type == SERVER_MYSQL_FOUND)
        fprintf(sql_file, " (MySQL server)");
      else if (connected_server_type == SERVER_DRIZZLE_FOUND)
        fprintf(sql_file, " (Drizzle server)");
    }
    if (opt_set_charset)
      fprintf(sql_file,
              "\nSET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION;\n");

    if (path.empty())
    {
      fprintf(md_result_file,"\nSET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;\nSET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;\n");
    }
    check_io(sql_file);
  }
} /* write_header */


static void write_footer(FILE *sql_file)
{
  if (! opt_compact)
  {
    if (path.empty())
    {
      fprintf(md_result_file,"SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;\nSET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;\n");
    }
    if (opt_set_charset)
      fprintf(sql_file, "SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION;\n");
    if (opt_comments)
    {
      if (opt_dump_date)
      {
        boost::posix_time::ptime time(boost::posix_time::second_clock::local_time());
        fprintf(sql_file, "-- Dump completed on %s\n",
          boost::posix_time::to_simple_string(time).c_str());
      }
      else
        fprintf(sql_file, "-- Dump completed\n");
    }
    check_io(sql_file);
  }
} /* write_footer */

static int get_options(void)
{

  if (path.empty() && (! enclosed.empty() || ! opt_enclosed.empty() || ! escaped.empty() || ! lines_terminated.empty() ||
                ! fields_terminated.empty()))
  {
    fprintf(stderr,
            _("%s: You must use option --tab with --fields-...\n"), progname.c_str());
    return(EX_USAGE);
  }

  if (opt_single_transaction && opt_lock_all_tables)
  {
    fprintf(stderr, _("%s: You can't use --single-transaction and "
                      "--lock-all-tables at the same time.\n"), progname.c_str());
    return(EX_USAGE);
  }
  if (! enclosed.empty() && ! opt_enclosed.empty())
  {
    fprintf(stderr, _("%s: You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n"), progname.c_str());
    return(EX_USAGE);
  }
  if ((opt_databases || opt_alldbs) && ! path.empty())
  {
    fprintf(stderr,
            _("%s: --databases or --all-databases can't be used with --tab.\n"),
            progname.c_str());
    return(EX_USAGE);
  }

  if (tty_password)
    opt_password=client_get_tty_password(NULL);
  return(0);
} /* get_options */


/*
 ** DB_error -- prints DRIZZLE error message and exits the program.
*/
static void DB_error(drizzle_result_st *res, drizzle_return_t ret,
                     const char *when)
{
  if (ret == DRIZZLE_RETURN_ERROR_CODE)
  {
    maybe_die(EX_DRIZZLEERR, _("Got error: %s (%d) %s"),
              drizzle_result_error(res),
              drizzle_result_error_code(res),
              when);
    drizzle_result_free(res);
  }
  else
    maybe_die(EX_DRIZZLEERR, _("Got error: %d %s"), ret, when);

  return;
}



/*
  Prints out an error message and kills the process.

  SYNOPSIS
  die()
  error_num   - process return value
  fmt_reason  - a format string for use by vsnprintf.
  ...         - variable arguments for above fmt_reason string

  DESCRIPTION
  This call prints out the formatted error message to stderr and then
  terminates the process.
*/
static void die(int error_num, const char* fmt_reason, ...)
{
  char buffer[1000];
  va_list args;
  va_start(args,fmt_reason);
  vsnprintf(buffer, sizeof(buffer), fmt_reason, args);
  va_end(args);

  fprintf(stderr, "%s: %s\n", progname.c_str(), buffer);
  fflush(stderr);

  ignore_errors= 0; /* force the exit */
  maybe_exit(error_num);
}


/*
  Prints out an error message and maybe kills the process.

  SYNOPSIS
  maybe_die()
  error_num   - process return value
  fmt_reason  - a format string for use by vsnprintf.
  ...         - variable arguments for above fmt_reason string

  DESCRIPTION
  This call prints out the formatted error message to stderr and then
  terminates the process, unless the --force command line option is used.

  This call should be used for non-fatal errors (such as database
  errors) that the code may still be able to continue to the next unit
  of work.

*/
static void maybe_die(int error_num, const char* fmt_reason, ...)
{
  char buffer[1000];
  va_list args;
  va_start(args,fmt_reason);
  vsnprintf(buffer, sizeof(buffer), fmt_reason, args);
  va_end(args);

  fprintf(stderr, "%s: %s\n", progname.c_str(), buffer);
  fflush(stderr);

  maybe_exit(error_num);
}



/*
  Sends a query to server, optionally reads result, prints error message if
  some.

  SYNOPSIS
  drizzleclient_query_with_error_report()
  drizzle_con       connection to use
  res             if non zero, result will be put there with
  drizzleclient_store_result()
  query           query to send to server

  RETURN VALUES
  0               query sending and (if res!=0) result reading went ok
  1               error
*/

static int drizzleclient_query_with_error_report(drizzle_con_st *con,
                                                 drizzle_result_st *result,
                                                 const char *query_str,
                                                 bool no_buffer)
{
  drizzle_return_t ret;

  if (drizzle_query_str(con, result, query_str, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      maybe_die(EX_DRIZZLEERR, _("Couldn't execute '%s': %s (%d)"),
                query_str, drizzle_result_error(result),
                drizzle_result_error_code(result));
      drizzle_result_free(result);
    }
    else
    {
      maybe_die(EX_DRIZZLEERR, _("Couldn't execute '%s': %s (%d)"),
                query_str, drizzle_con_error(con), ret);
    }
    return 1;
  }

  if (no_buffer)
    ret= drizzle_column_buffer(result);
  else
    ret= drizzle_result_buffer(result);
  if (ret != DRIZZLE_RETURN_OK)
  {
    drizzle_result_free(result);
    maybe_die(EX_DRIZZLEERR, _("Couldn't execute '%s': %s (%d)"),
              query_str, drizzle_con_error(con), ret);
    return 1;
  }

  return 0;
}

static void free_resources(void)
{
  if (md_result_file && md_result_file != stdout)
    fclose(md_result_file);
  opt_password.erase();
}


static void maybe_exit(int error)
{
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    return;
  drizzle_con_free(&dcon);
  drizzle_free(&drizzle);
  free_resources();
  exit(error);
}


/*
  db_connect -- connects to the host and selects DB.
*/

static int connect_to_db(string host, string user,string passwd)
{
  drizzle_return_t ret;

  verbose_msg(_("-- Connecting to %s, using protocol %s...\n"), ! host.empty() ? (char *)host.c_str() : "localhost", opt_protocol.c_str());
  drizzle_create(&drizzle);
  drizzle_con_create(&drizzle, &dcon);
  drizzle_con_set_tcp(&dcon, (char *)host.c_str(), opt_drizzle_port);
  drizzle_con_set_auth(&dcon, (char *)user.c_str(), (char *)passwd.c_str());
  drizzle_con_add_options(&dcon, use_drizzle_protocol ? DRIZZLE_CON_EXPERIMENTAL : DRIZZLE_CON_MYSQL);
  ret= drizzle_con_connect(&dcon);
  if (ret != DRIZZLE_RETURN_OK)
  {
    DB_error(NULL, ret, "when trying to connect");
    return(1);
  }

  int found= get_server_type();

  switch (found)
  {
    case SERVER_MYSQL_FOUND:
     verbose_msg(_("-- Connected to a MySQL server\n"));
     connected_server_type= SERVER_MYSQL_FOUND;
     break;
    case SERVER_DRIZZLE_FOUND:
     verbose_msg(_("-- Connected to a Drizzle server\n"));
     connected_server_type= SERVER_DRIZZLE_FOUND;
     break;
    default:
     verbose_msg(_("-- Connected to an unknown server type\n"));
     connected_server_type= SERVER_DRIZZLE_FOUND;
     break;
  }

  return(0);
} /* connect_to_db */


/*
 ** dbDisconnect -- disconnects from the host.
*/
static void dbDisconnect(string &host)
{
  verbose_msg(_("-- Disconnecting from %s...\n"), ! host.empty() ? host.c_str() : "localhost");
  drizzle_con_free(&dcon);
  drizzle_free(&drizzle);
} /* dbDisconnect */

static int dump_all_databases()
{
  drizzle_row_t row;
  drizzle_result_st tableres;
  int result=0;
  std::string query;

  if (connected_server_type == SERVER_MYSQL_FOUND)
    query= "SELECT SCHEMA_NAME, DEFAULT_COLLATION_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME NOT IN ('information_schema', 'performance_schema')";
  else
    query= "SELECT SCHEMA_NAME, DEFAULT_COLLATION_NAME FROM DATA_DICTIONARY.SCHEMAS WHERE SCHEMA_NAME NOT IN ('information_schema','data_dictionary')";

  if (drizzleclient_query_with_error_report(&dcon, &tableres, query.c_str(), false))
    return 1;
  while ((row= drizzle_row_next(&tableres)))
  {
    std::string database_name(row[0]);
    DrizzleDumpDatabase *database= new DrizzleDumpDatabase(database_name);
    database->setCollate(row[1]);
    database_store.push_back(database);
  }
  drizzle_result_free(&tableres);
  return result;
}
/* dump_all_databases */


static int dump_databases(const vector<string> &db_names)
{
  int result=0;
  string temp;
  for (vector<string>::const_iterator it= db_names.begin(); it != db_names.end(); ++it)
  {
    temp= *it;
    DrizzleDumpDatabase *database= new DrizzleDumpDatabase(temp);
    database_store.push_back(database);
  }
  return(result);
} /* dump_databases */

static int dump_selected_tables(const string &db, const vector<string> &table_names)
{
  DrizzleDumpDatabase *database= new DrizzleDumpDatabase(db);
  database_store.push_back(database); 

  for (vector<string>::const_iterator it= table_names.begin(); it != table_names.end(); ++it)
  {
    string temp= *it;
    DrizzleDumpTable *table= new DrizzleDumpTable(temp);
    database->tables.push_back(table);
  }
  database_store.push_back(database);

  return 0;
} /* dump_selected_tables */

static int do_flush_tables_read_lock(drizzle_con_st *drizzle_con)
{
  /*
    We do first a FLUSH TABLES. If a long update is running, the FLUSH TABLES
    will wait but will not stall the whole mysqld, and when the long update is
    done the FLUSH TABLES WITH READ LOCK will start and succeed quickly. So,
    FLUSH TABLES is to lower the probability of a stage where both drizzled
    and most client connections are stalled. Of course, if a second long
    update starts between the two FLUSHes, we have that bad stall.
  */
  return
    ( drizzleclient_query_with_error_report(drizzle_con, 0, "FLUSH TABLES", false) ||
      drizzleclient_query_with_error_report(drizzle_con, 0,
                                            "FLUSH TABLES WITH READ LOCK", false) );
}

static int do_unlock_tables(drizzle_con_st *drizzle_con)
{
  return drizzleclient_query_with_error_report(drizzle_con, 0, "UNLOCK TABLES", false);
}

static int start_transaction(drizzle_con_st *drizzle_con)
{
  return (drizzleclient_query_with_error_report(drizzle_con, 0,
                                                "SET SESSION TRANSACTION ISOLATION "
                                                "LEVEL REPEATABLE READ", false) ||
          drizzleclient_query_with_error_report(drizzle_con, 0,
                                                "START TRANSACTION "
                                                "WITH CONSISTENT SNAPSHOT", false));
}


int get_server_type()
{
  boost::match_flag_type flags = boost::match_default; 

  boost::regex mysql_regex("(5\\.[0-9]+\\.[0-9]+)");
  boost::regex drizzle_regex("(20[0-9]{2}\\.(0[1-9]|1[012])\\.[0-9]+)");

  std::string version(drizzle_con_server_version(&dcon));

  if (regex_search(version, mysql_regex, flags))
    return SERVER_MYSQL_FOUND;
  
  if (regex_search(version, drizzle_regex, flags))
    return SERVER_DRIZZLE_FOUND;

  return SERVER_UNKNOWN_FOUND;
}

int main(int argc, char **argv)
{
try
{
  int exit_code;
  drizzle_result_st result;

  po::options_description commandline_options(N_("Options used only in command line"));
  commandline_options.add_options()
  ("all-databases,A", po::value<bool>(&opt_alldbs)->default_value(false)->zero_tokens(),
  N_("Dump all the databases. This will be same as --databases with all databases selected."))
  ("all-tablespaces,Y", po::value<bool>(&opt_alltspcs)->default_value(false)->zero_tokens(),
  N_("Dump all the tablespaces."))
  ("complete-insert,c", po::value<bool>(&opt_complete_insert)->default_value(false)->zero_tokens(),
  N_("Use complete insert statements."))
  ("compress,C", po::value<bool>(&opt_compress)->default_value(false)->zero_tokens(),
  N_("Use compression in server/client protocol."))
  ("flush-logs,F", po::value<bool>(&flush_logs)->default_value(false)->zero_tokens(),
  N_("Flush logs file in server before starting dump. Note that if you dump many databases at once (using the option --databases= or --all-databases), the logs will be flushed for each database dumped. The exception is when using --lock-all-tables in this case the logs will be flushed only once, corresponding to the moment all tables are locked. So if you want your dump and the log flush to happen at the same exact moment you should use --lock-all-tables or --flush-logs"))
  ("force,f", po::value<bool>(&ignore_errors)->default_value(false)->zero_tokens(),
  N_("Continue even if we get an sql-error."))
  ("help,?", N_("Display this help message and exit."))
  ("lock-all-tables,x", po::value<bool>(&opt_lock_all_tables)->default_value(false)->zero_tokens(),
  N_("Locks all tables across all databases. This is achieved by taking a global read lock for the duration of the whole dump. Automatically turns --single-transaction and --lock-tables off."))
  ("order-by-primary", po::value<bool>(&opt_order_by_primary)->default_value(false)->zero_tokens(),
  N_("Sorts each table's rows by primary key, or first unique key, if such a key exists.  Useful when dumping a MyISAM table to be loaded into an InnoDB table, but will make the dump itself take considerably longer."))
  ("single-transaction", po::value<bool>(&opt_single_transaction)->default_value(false)->zero_tokens(),
  N_("Creates a consistent snapshot by dumping all tables in a single transaction. Works ONLY for tables stored in storage engines which support multiversioning (currently only InnoDB does); the dump is NOT guaranteed to be consistent for other storage engines. While a --single-transaction dump is in process, to ensure a valid dump file (correct table contents), no other connection should use the following statements: ALTER TABLE, DROP TABLE, RENAME TABLE, TRUNCATE TABLE, as consistent snapshot is not isolated from them. Option automatically turns off --lock-tables."))
  ("opt", N_("Same as --add-drop-table, --add-locks, --create-options, --quick, --extended-insert, --lock-tables, --set-charset, and --disable-keys. Enabled by default, disable with --skip-opt.")) 
  ("skip-opt", 
  N_("Disable --opt. Disables --add-drop-table, --add-locks, --create-options, --quick, --extended-insert, --lock-tables, --set-charset, and --disable-keys."))    
  ("tables", N_("Overrides option --databases (-B)."))
  ("show-progress-size", po::value<uint32_t>(&show_progress_size)->default_value(10000),
  N_("Number of rows before each output progress report (requires --verbose)."))
  ("verbose,v", po::value<bool>(&verbose)->default_value(false)->zero_tokens(),
  N_("Print info about the various stages."))
  ("version,V", N_("Output version information and exit."))
  ("skip-comments", N_("Turn off Comments"))
  ("skip-create", N_("Turn off create-options"))
  ("skip-extended-insert", N_("Turn off extended-insert"))
  ("skip-dump-date",N_( "Turn off dump date at the end of the output"))
  ("no-defaults", N_("Do not read from the configuration files"))
  ;

  po::options_description dump_options(N_("Options specific to the drizzle client"));
  dump_options.add_options()
  ("add-drop-database", po::value<bool>(&opt_drop_database)->default_value(false)->zero_tokens(),
  N_("Add a 'DROP DATABASE' before each create."))
  ("skip-drop-table", N_("Do not add a 'drop table' before each create."))
  ("allow-keywords", po::value<bool>(&opt_keywords)->default_value(false)->zero_tokens(),
  N_("Allow creation of column names that are keywords."))
  ("compact", po::value<bool>(&opt_compact)->default_value(false)->zero_tokens(),
  N_("Give less verbose output (useful for debugging). Disables structure comments and header/footer constructs.  Enables options --skip-add-drop-table --no-set-names --skip-disable-keys --skip-add-locks"))
  ("databases,B", po::value<bool>(&opt_databases)->default_value(false)->zero_tokens(),
  N_("To dump several databases. Note the difference in usage; In this case no tables are given. All name arguments are regarded as databasenames. 'USE db_name;' will be included in the output."))
  ("delayed-insert", po::value<bool>(&opt_delayed)->default_value(false)->zero_tokens(),
  N_("Insert rows with INSERT DELAYED;"))
  ("skip-disable-keys,K",
  N_("'ALTER TABLE tb_name DISABLE KEYS; and 'ALTER TABLE tb_name ENABLE KEYS; will not be put in the output."))
  ("hex-blob", po::value<bool>(&opt_hex_blob)->default_value(false)->zero_tokens(),
  "Dump binary strings (BINARY, VARBINARY, BLOB) in hexadecimal format.")
  ("ignore-table", po::value<string>(),
  N_("Do not dump the specified table. To specify more than one table to ignore, use the directive multiple times, once for each table.  Each table must be specified with both database and table names, e.g. --ignore-table=database.table"))
  ("insert-ignore", po::value<bool>(&opt_ignore)->default_value(false)->zero_tokens(),
  N_("Insert rows with INSERT IGNORE."))
  ("lines-terminated-by", po::value<string>(&lines_terminated)->default_value(""),
  N_("Lines in the i.file are terminated by ..."))
  ("no-autocommit", po::value<bool>(&opt_autocommit)->default_value(false)->zero_tokens(),
  N_("Wrap tables with autocommit/commit statements."))
  ("no-create-db,n", po::value<bool>(&opt_create_db)->default_value(false)->zero_tokens(),
  N_("'CREATE DATABASE IF NOT EXISTS db_name;' will not be put in the output. The above line will be added otherwise, if --databases or --all-databases option was given."))
  ("no-create-info,t", po::value<bool>(&opt_no_create_info)->default_value(false)->zero_tokens(),
  N_("Don't write table creation info."))
  ("no-data,d", po::value<bool>(&opt_no_data)->default_value(false)->zero_tokens(),
  N_("No row information."))
  ("no-set-names,N", N_("Deprecated. Use --skip-set-charset instead."))
  ("set-charset", po::value<bool>(&opt_set_charset)->default_value(false)->zero_tokens(),
  N_("Enable set-name"))
  ("slow", N_("Buffer query instead of dumping directly to stdout."))
  ("replace", po::value<bool>(&opt_replace_into)->default_value(false)->zero_tokens(),
  N_("Use REPLACE INTO instead of INSERT INTO."))
  ("result-file,r", po::value<string>(),
  N_("Direct output to a given file. This option should be used in MSDOS, because it prevents new line '\\n' from being converted to '\\r\\n' (carriage return + line feed)."))
  ("where,w", po::value<string>(&where)->default_value(""),
  N_("Dump only selected records; QUOTES mandatory!"))
  ;

  po::options_description client_options(N_("Options specific to the client"));
  client_options.add_options()
  ("host,h", po::value<string>(&current_host)->default_value("localhost"),
  N_("Connect to host."))
  ("password,P", po::value<string>(&password)->default_value(PASSWORD_SENTINEL),
  N_("Password to use when connecting to server. If password is not given it's solicited on the tty."))
  ("port,p", po::value<uint32_t>(&opt_drizzle_port)->default_value(0),
  N_("Port number to use for connection."))
  ("user,u", po::value<string>(&current_user)->default_value(""),
  N_("User for login if not current user."))
  ("protocol",po::value<string>(&opt_protocol)->default_value("mysql"),
  N_("The protocol of connection (mysql or drizzle)."))
  ;

  po::options_description hidden_options(N_("Hidden Options"));
  hidden_options.add_options()
  ("database-used", po::value<vector<string> >(), N_("Used to select the database"))
  ("Table-used", po::value<vector<string> >(), N_("Used to select the tables"))
  ;

  po::options_description all_options(N_("Allowed Options + Hidden Options"));
  all_options.add(commandline_options).add(dump_options).add(client_options).add(hidden_options);

  po::options_description long_options(N_("Allowed Options"));
  long_options.add(commandline_options).add(dump_options).add(client_options);

  std::string system_config_dir_dump(SYSCONFDIR); 
  system_config_dir_dump.append("/drizzle/drizzledump.cnf");

  std::string system_config_dir_client(SYSCONFDIR); 
  system_config_dir_client.append("/drizzle/client.cnf");

  std::string user_config_dir((getenv("XDG_CONFIG_HOME")? getenv("XDG_CONFIG_HOME"):"~/.config"));

  po::positional_options_description p;
  p.add("database-used", 1);
  p.add("Table-used",-1);

  md_result_file= stdout;

  po::variables_map vm;

  po::store(po::command_line_parser(argc, argv).options(all_options).
            positional(p).extra_parser(parse_password_arg).run(), vm);

  if (! vm.count("no-defaults"))
  {
    std::string user_config_dir_dump(user_config_dir);
    user_config_dir_dump.append("/drizzle/drizzledump.cnf"); 

    std::string user_config_dir_client(user_config_dir);
    user_config_dir_client.append("/drizzle/client.cnf");

    ifstream user_dump_ifs(user_config_dir_dump.c_str());
    po::store(parse_config_file(user_dump_ifs, dump_options), vm);

    ifstream user_client_ifs(user_config_dir_client.c_str());
    po::store(parse_config_file(user_client_ifs, client_options), vm);

    ifstream system_dump_ifs(system_config_dir_dump.c_str());
    po::store(parse_config_file(system_dump_ifs, dump_options), vm);

    ifstream system_client_ifs(system_config_dir_client.c_str());
    po::store(parse_config_file(system_client_ifs, client_options), vm);
  }

  po::notify(vm);  
  
  if ((not vm.count("database-used") && not vm.count("Table-used") 
    && not opt_alldbs && path.empty())
    || (vm.count("help")) || vm.count("version"))
  {
    printf(_("Drizzledump %s build %s, for %s-%s (%s)\n"),
      drizzle_version(), VERSION, HOST_VENDOR, HOST_OS, HOST_CPU);
    if (vm.count("version"))
      exit(0);
    puts("");
    puts(_("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n"));
    puts(_("Dumps definitions and data from a Drizzle database server"));
    printf(_("Usage: %s [OPTIONS] database [tables]\n"), progname.c_str());
    printf(_("OR     %s [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3...]\n"),
          progname.c_str());
    printf(_("OR     %s [OPTIONS] --all-databases [OPTIONS]\n"), progname.c_str());
    cout << long_options;
    if (vm.count("help"))
      exit(0);
    else
      exit(1);
  }

  /* Inverted Booleans */

  opt_drop= (vm.count("skip-drop-table")) ? false : true;
  opt_comments= (vm.count("skip-comments")) ? false : true;
  extended_insert= (vm.count("skip-extended-insert")) ? false : true;
  opt_dump_date= (vm.count("skip-dump-date")) ? false : true;
  opt_disable_keys= (vm.count("skip-disable-keys")) ? false : true;
  quick= (vm.count("slow")) ? false : true;
  opt_quoted= (vm.count("skip-quote-names")) ? false : true;

  if (vm.count("protocol"))
  {
    std::transform(opt_protocol.begin(), opt_protocol.end(),
      opt_protocol.begin(), ::tolower);

    if (not opt_protocol.compare("mysql"))
      use_drizzle_protocol=false;
    else if (not opt_protocol.compare("drizzle"))
      use_drizzle_protocol=true;
    else
    {
      cout << _("Error: Unknown protocol") << " '" << opt_protocol << "'" << endl;
      exit(-1);
    }
  }

  if (vm.count("port"))
  {
    /* If the port number is > 65535 it is not a valid port
     *        This also helps with potential data loss casting unsigned long to a
     *               uint32_t. 
     */
    if (opt_drizzle_port > 65535)
    {
      fprintf(stderr, _("Value supplied for port is not valid.\n"));
      exit(-1);
    }
  }

  if(vm.count("password"))
  {
    if (!opt_password.empty())
      opt_password.erase();
    if (password == PASSWORD_SENTINEL)
    {
      opt_password= "";
    }
    else
    {
      opt_password= password;
      tty_password= false;
    }
  }
  else
  {
      tty_password= true;
  }

  if (vm.count("result-file"))
  {
    if (!(md_result_file= fopen(vm["result-file"].as<string>().c_str(), "w")))
      exit(1);
  }

  if (vm.count("no-set-names"))
  {
    opt_set_charset= 0;
  }
 
  if (! path.empty())
  { 
    opt_disable_keys= 0;
  }

  if (vm.count("skip-opt"))
  {
    extended_insert= opt_drop= quick= create_options= 0;
    opt_disable_keys= opt_set_charset= 0;
  }

  if (opt_compact)
  { 
    opt_comments= opt_drop= opt_disable_keys= 0;
    opt_set_charset= 0;
  }

  if (vm.count("opt"))
  {
    extended_insert= opt_drop= quick= create_options= 1;
    opt_disable_keys= opt_set_charset= 1;
  }

  if (vm.count("tables"))
  { 
    opt_databases= false;
  }

  if (vm.count("ignore-table"))
  {
    if (!strchr(vm["ignore-table"].as<string>().c_str(), '.'))
    {
      fprintf(stderr, _("Illegal use of option --ignore-table=<database>.<table>\n"));
      exit(EXIT_ARGUMENT_INVALID);
    }
    string tmpptr(vm["ignore-table"].as<string>());
    ignore_table.insert(tmpptr); 
  }

  if (vm.count("skip-create"))
  {
    opt_create_db= opt_no_create_info= create_options= false;
  }
 
  exit_code= get_options();
  if (exit_code)
  {
    free_resources();
    exit(exit_code);
  }

  if (connect_to_db(current_host, current_user, opt_password))
  {
    free_resources();
    exit(EX_DRIZZLEERR);
  }

  if (connected_server_type == SERVER_MYSQL_FOUND)
  {
    if (drizzleclient_query_with_error_report(&dcon, &result, "SET NAMES 'utf8'", false))
      goto err;
    drizzle_result_free(&result);
  }

  if (path.empty() && vm.count("database-used"))
  {
    string database_used= *vm["database-used"].as< vector<string> >().begin();
    write_header(md_result_file, (char *)database_used.c_str());
  }

  if ((opt_lock_all_tables) && do_flush_tables_read_lock(&dcon))
    goto err;
  if (opt_single_transaction && start_transaction(&dcon))
    goto err;
  if (opt_lock_all_tables)
  {
    if (drizzleclient_query_with_error_report(&dcon, &result, "FLUSH LOGS", false))
      goto err;
    drizzle_result_free(&result);
    flush_logs= 0; /* not anymore; that would not be sensible */
  }
  if (opt_single_transaction && do_unlock_tables(&dcon)) /* unlock but no commit! */
    goto err;

  if (opt_alldbs)
  {
    dump_all_databases();
    dump_all_tables();
  }
  if (vm.count("database-used") && vm.count("Table-used") && ! opt_databases)
  {
    string database_used= *vm["database-used"].as< vector<string> >().begin();
    /* Only one database and selected table(s) */
    dump_selected_tables(database_used, vm["Table-used"].as< vector<string> >());
  }

  if (vm.count("Table-used") && opt_databases)
  {
/*
 * This is not valid!
    vector<string> database_used= vm["database-used"].as< vector<string> >();
    vector<string> table_used= vm["Table-used"].as< vector<string> >();

    for (vector<string>::iterator it= table_used.begin();
       it != table_used.end();
       ++it)
    {
      database_used.insert(database_used.end(), *it);
    }
    dump_databases(database_used);
    dump_tables();
*/
  }
  
  if (vm.count("database-used") && ! vm.count("Table-used"))
  {
    dump_databases(vm["database-used"].as< vector<string> >());
    dump_all_tables();
  }

  generate_dump();

  /* ensure dumped data flushed */
  if (md_result_file && fflush(md_result_file))
  {
    if (!first_error)
      first_error= EX_DRIZZLEERR;
    goto err;
  }

  /*
    No reason to explicitely COMMIT the transaction, neither to explicitely
    UNLOCK TABLES: these will be automatically be done by the server when we
    disconnect now. Saves some code here, some network trips, adds nothing to
    server.
  */
err:
  dbDisconnect(current_host);
  if (path.empty())
    write_footer(md_result_file);
  free_resources();

  if (stderror_file)
    fclose(stderror_file);
}

  catch(exception &err)
  {
    cerr << err.what() << endl;
  }
  
  return(first_error);
} /* main */
