 /* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
  * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
  *
  * Copyright (C) 2011 Stewart Smith, Henrik Ingo, Mohit Srivastava
  *       
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *               
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *                           
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  */
/**
 * @file Declare a class JsonHandler which handles the operations related to Json. 
 **/

#include <config.h>

#include <plugin/json_server/json/json.h>
#include <evhttp.h>
#include <event.h>
#include <drizzled/session.h>
#include <plugin/json_server/error.h>

using namespace drizzled;
namespace drizzle_plugin
{
namespace json_server
{
  /*
   * a class.
   * used to handle operations related to Json.
   */  
  class JsonHandler
  {
    public:
      /*
       * Generate an input query from http request object.
       *
       * @param req_arg the http request object.
       */ 
      void generate_input_query(struct evhttp_request *req_arg);
      /*
       * Generate an input json from http request object.
       *
       * @param req_arg the http request object.
       * @param _json_error_area the JsonErrorArea object to handle error.
       */ 
      void generate_input_json(struct evhttp_request *req_arg,JsonErrorArea &_json_error_area);
      /*
       * Generate an output query string.
       *
       * @param _json_error_area the JsonErrorArea object to handle error.
       */  
      void generate_output_query(JsonErrorArea& _json_error_area);
      /*
       * Generate an output Json.
       *
       * @param _json_error_area the JsonErrorArea object to handle error.
       */ 
      void generate_output_json(JsonErrorArea& _json_error_area);
      /*
       * Get an output query string.
       *
       * @return a const output query string.
       */
      const std::string& get_output_query() const
      {
        return _output_query;
      }
      /*
       * Get an input query string.
       *
       * @return a const input query string.
       */  
      const std::string& get_input_query() const
      {
        return _input_query;
      }
      /*
       * Get an output json object.
       *
       * @return a const json object.
       */
      const Json::Value get_output_json() const
      {
        return _json_out;
      }
      /*
       * Get an input json object.
       *
       * @return a const json object.
       */ 
      const Json::Value get_input_json() const
      {
        return _json_in;
      }
    
    private:
      /*
       * Stores input json object.
       */ 
      Json::Value _json_in;
      /*
       * Stores output json object.
       */ 
      Json::Value _json_out;
      /*
       * Stores input string.
       */
      std::string _input_query;
      /*
       * Stores output string.
       */ 
      std::string _output_query;
  };
}
}
