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
 * @file Declare a class SQLToJsonGenerator that helps to generate json string corresponds to request type.
 */

#include <config.h>
#include <plugin/json_server/json/json.h>
#include <plugin/json_server/sql_executor.h>
#include <plugin/json_server/http_handler.h>
#include <string>

using namespace std;
/**
 *  Drizzle Plugin Namespace
 */
namespace drizzle_plugin
{
/**
 *  Json Server Plugin Namespace
 */
namespace json_server
{
  /**
   * a class.
   * used to generate json string
   */
  class SQLToJsonGenerator
  {
    public:
      /**
       * a constructor.
       * intializes the members variables.
       * @param json_in a Json::Value object.
       * @param schema a constant character pointer.
       * @param table a constant character pointer.
       * @param sqlExecutor a SQLExecutor pointer.
       */ 
      SQLToJsonGenerator(Json::Value &json_out,const char* schema,const char* table,SQLExecutor *sqlExecutor);
      /**
       * a function variable.
       * used to generate error json string
       */
      void generateSQLErrorJson();
      /**
       * a function variable.
       * used to generate a json string corresponds to request type.
       * @param type a evhttp_cmd_type enum.
       */
      void generateJson(enum evhttp_cmd_type type);
      /**
       * a constant function variable.
       * used to get a output json object.
       * @return a json object. 
       */
      const Json::Value getJson() const 
      {
        return _json_out;
      }

    private:
      /**
       * a private variable.
       * stores output json object. 
       */
      Json::Value _json_out;
      /**
       * a private variable.
       * stores instance of sqlExecutor object. 
       */
      SQLExecutor* _sql_executor;
      /**
       * a private variable.
       * stores schema being used. 
       */
      const char* _schema;
      /**
       * a private variable.
       * stores table being used. 
       */
      const char* _table;
      /**
       * a private function variable.
       * used to generate json string corresponds to GET request.
       */
      void generateGetJson();
      /**
       * a private function variable.
       * used to generate json string corresponds to POST request.
       */
      void generatePostJson();
      /**
       * a private function variable.
       * used to generate json string corresponds to DELETE request.
       */
      void generateDeleteJson();

  };
}

}

