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
 * @file An interface that helps to access sql_generator, sql_executor, sql_to_json_generator
 * 
 */

#include <config.h>
#include <plugin/json_server/json/json.h>
#include <plugin/json_server/sql_generator.h>
#include <plugin/json_server/sql_executor.h>
#include <plugin/json_server/sql_to_json_generator.h>
#include <plugin/json_server/http_handler.h>
/**
 *  Drizzle Plugin Namespace
 */ 
namespace drizzle_plugin
{
/**
  * Json Server Plugin namespace
  */
namespace json_server
{
  /**
   * a class 
   * used to access various members and variables of sql_generator, sql_executor, sql_to_json_generator.
   */
  class DBAccess
  {
    private:
      /**
       * a private variable.
       * stores input json object. 
       */
      Json::Value _json_in;
      /**
       * a private variable.
       * stores output json object.
       */
      Json::Value _json_out;
      /**
       * a private variable.
       * store type of request.
       */
      enum evhttp_cmd_type _type;
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
      
    public:
      /**
       * a function variable.
       * used to get output json object
       */ 
      const Json::Value getOutputJson() const
      {
        return _json_out;
      }
      /**
       * a function variable
       * used to get input json object.
       */
      const Json::Value getInputJson() const
      {
        return _json_in;
      }
      /**
       * A constructor.
       * intializes the member variables.
       * @param json_in a Json::Value object.
       * @param json_out a Json::Value object.
       * @param type a evttp_cmd_type enum.
       * @param schema a constant character pointer.
       * @param table a constant character pointer.
       */
      DBAccess(Json::Value &json_in,Json::Value &json_out,enum evhttp_cmd_type type,const char* schema,const char* table);
      /**
       * a function variable.
       * used to execute operations need to access sql_generator, sql_executor and sql_to_json_generator.
       */ 
      void execute();
  
  };
}
}
