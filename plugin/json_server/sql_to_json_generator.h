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
 * @file Declare a class SQLToJsonGenerator that generates the outpu json object.
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
   * Generate output json object.
   */
  class SQLToJsonGenerator
  {
    public:
      /**
       * Constructor
       * 
       * @param json_in The input json object.
       * @param schema The schema that was used.
       * @param table The table that was used.
       * @param sqlExecutor SQLExecutor instance that contains the result set or error from an executed sql query.
       */ 
      SQLToJsonGenerator(Json::Value& json_out,const char* schema,const char* table,SQLExecutor* sqlExecutor);
      /**
       * Used to generate error json string.
       */
      void generateSQLErrorJson();
      /**
       * Used to generate a json string corresponds to request type.
       * @param type GET, POST or DELETE.
       */
      void generateJson(enum evhttp_cmd_type type);
      /**
       * Used to get the output json object.
       * @return a json object. 
       */
      Json::Value getJson() const 
      {
        return _json_out;
      }

    private:
      /**
       * Stores output json object. 
       * 
       * @todo Note that building the returned json string as a Json::Value object
       * first - basically this is a full copy of the result_set - is inefficient for
       * larger result sets. Better would be to find a streaming json parser.
       * However, it should be notable that Execute API already stores the result set
       * in a std::vector anyway, so we are not making the situation worse here.
       */
      Json::Value _json_out;
      /**
       * Stores instance of sqlExecutor object. 
       */
      SQLExecutor* _sql_executor;
      /**
       * Stores schema being used. 
       */
      const char* _schema;
      /**
       * Stores table being used. 
       */
      const char* _table;
      /**
       * Used to generate json string corresponds to GET request.
       */
      void generateGetJson();
      /**
       * Used to generate json string corresponds to POST request.
       */
      void generatePostJson();
      /**
       * Used to generate json string corresponds to DELETE request.
       */
      void generateDeleteJson();

  };
}

}

