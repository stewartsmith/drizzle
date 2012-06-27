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
 * @file Implementation of various functions of class DBAccess.
 * 
 */
#include <plugin/json_server/db_access.h>

namespace drizzle_plugin
{
namespace json_server
{
  DBAccess::DBAccess(Json::Value &json_in,Json::Value &json_out,enum evhttp_cmd_type type,const char* schema,const char* table)
  {
    _json_in= json_in;
    _json_out= json_out;
    _type=type;
    _schema=schema;
    _table=table;
  }
  
  void DBAccess::execute()
  {
      std::string sql;
      SQLGenerator* generator = new SQLGenerator(_json_in,_schema,_table);
      generator->generateSql(_type);
      sql= generator->getSQL();

      SQLExecutor* executor = new SQLExecutor(_schema);
      SQLToJsonGenerator* jsonGenerator = new SQLToJsonGenerator(_json_out,_schema,_table,executor);
      if(executor->executeSQL(sql))
      {
        jsonGenerator->generateJson(_type);  
      }
      else
      { 
        // For POST request, if a table didn't exist, we create the table 
        // transparently then retry the INSERT.
        if(_type == EVHTTP_REQ_POST && executor->getErr() == drizzled::ER_TABLE_UNKNOWN)
        {
          generator->generateCreateTableSql();
          sql= generator->getSQL();
          if(executor->executeSQL(sql))
          {
            generator->generateSql(_type);
            sql= generator->getSQL();
            if(executor->executeSQL(sql))
            {
              jsonGenerator->generateJson(_type);
            }
            else
            {
              jsonGenerator->generateSQLErrorJson();
            }
          }
          else
          {
            jsonGenerator->generateSQLErrorJson();
          }
        }
        else
        {
          jsonGenerator->generateSQLErrorJson();
        }
      }
      _json_out= jsonGenerator->getJson();
      delete(jsonGenerator);
      delete(executor);
      delete(generator);
   }
  
}
}
