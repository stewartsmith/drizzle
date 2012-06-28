/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2012 Mohit Srivastava
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
/**
 * @file Implements a class SQLToJsonGenerator which generate the JSON strings corresponding to type of HTTP Request
 *  
 */
#include <plugin/json_server/sql_to_json_generator.h>

using namespace std;
using namespace drizzled;

namespace drizzle_plugin
{
namespace json_server
{ 
  SQLToJsonGenerator::SQLToJsonGenerator(Json::Value& json_out,const char* schema,const char* table,SQLExecutor* sqlExecutor)
  {
    _schema=schema;
    _table=table;
    _sql_executor=sqlExecutor;
    _json_out=json_out;
  
  }
  
  void SQLToJsonGenerator::generateSQLErrorJson()
  {  
    sql::Exception _exception= _sql_executor->getException(); 
    _json_out["error_type"]= "sql error";
    _json_out["error_message"]= _exception.getErrorMessage();
    _json_out["error_code"]= _exception.getErrorCode();
    _json_out["internal_sql_query"]= _sql_executor->getSql();
    _json_out["schema"]= _schema;
    _json_out["sqlstate"]= _exception.getSQLState();
    _json_out["table"]= _table;
  }
  
  void SQLToJsonGenerator::generateJson(enum evhttp_cmd_type type)
  {
    if(type==EVHTTP_REQ_GET)
      generateGetJson();
    else if(type==EVHTTP_REQ_POST)
      generatePostJson();
    else if(type==EVHTTP_REQ_DELETE)
      generateDeleteJson();
  }
  
  void SQLToJsonGenerator::generateGetJson()
  {
    sql::ResultSet *_result_set= _sql_executor->getResultSet();
    sql::Exception exception= _sql_executor->getException();
    // Handle each row of the result set
    while (_result_set->next())
    {
      Json::Value json_row;
      bool got_error = false; 
      // Handle each column of a row
      for (size_t x= 0; x < _result_set->getMetaData().getColumnCount() && got_error == false; x++)
      {
        // Only output non-null rows
        if (not _result_set->isNull(x))
        {
          Json::Value  json_doc;
          Json::Features json_conf;
          Json::Reader readrow(json_conf);
          std::string col_name = _result_set->getColumnInfo(x).col_name;
          bool r = readrow.parse(_result_set->getString(x), json_doc);
          if (r != true) 
          {
            _json_out["error_type"]="json parse error on row value";
            _json_out["error_internal_sql_column"]=col_name;
            // Just put the string there as it is, better than nothing.
            json_row[col_name]= _result_set->getString(x);
            got_error=true;
            break;
          }
          else 
          {
            json_row[col_name]= json_doc;
          }
        }
      }
        // When done, append this row to result set tree
      _json_out["result_set"].append(json_row);
    }
    _json_out["sqlstate"]= exception.getSQLState();
  }
  
  void SQLToJsonGenerator::generatePostJson()
  {
    sql::Exception exception= _sql_executor->getException();
    _json_out["sqlstate"]= exception.getSQLState();
  }
  
  void SQLToJsonGenerator::generateDeleteJson()
  {
    sql::Exception exception= _sql_executor->getException();
   _json_out["sqlstate"]= exception.getSQLState();
  }

}
}
