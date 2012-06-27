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
 * @file Declare a class HTTPHandler which handles the operations related to HTTP request and response. 
 */

#pragma once

#include <config.h>
#include <evhttp.h>
#include <plugin/json_server/json/json.h>
#include <drizzled/plugin.h>

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
   * used to handles http request and response. 
   */
  class HttpHandler
  {
    public:
      /**
       * Constructor
       * 
       * @param json_out an empty json output object.
       * @param json_in an empty json input object.
       * @param req the http request object to parse.
       */
      HttpHandler(Json::Value& json_out,Json::Value json_in,struct evhttp_request *req);
      /**
       * Parse http request and retrieve various http headers.
       *
       * @return false Success
       * @return true Failure
       */
      bool handleRequest();
      /**
       * Parse input json query and generate input json object.
       *
       * @param default_schema a string.
       * @param default_table a string.
       * @param allow_drop_table a boolean value.
       * @return false Success.
       * @return true Failure.
       */
      bool validate(string &default_schema,string &default_table,bool allow_drop_table);
      /**
       * Send http response back.
       *
       * @param json_out a Json::Value object.
       */
      void sendResponse();
      /**
       * Generate a http error when table is null.
       */
      void generateHttpError();
      /**
       * Generate a error occurs using DROP Table command.
       */
      void generateDropTableError();
      /**
       * Get schema being used.
       * @return a constant schema string.
       */
      const char* getSchema() const
      {
        return _schema;
      }
      /**
       * Get table being used.
       * 
       * @return a constant table string.
       */
      const char* getTable() const
      {
        return _table;
      }
      /**
       * Get query being used.
       * @return a constant query string.
       */
      const string &getQuery() const
      {
        return _query;
      }
      /**
       * Get id being used.
       * @return a constant id string.
       */
      const char* getId() const
      {
        return _id;
      }
      /**
       * Get output json object.
       * @return a constant json object.
       */
      const Json::Value getOutputJson() const 
      {
        return _json_out;
      }
      /**
       * Get input json object.
       * @return a constant json object.
       */
      const Json::Value getInputJson() const 
      {
        return _json_in;
      }
     /**
      * Set Output json object.
      * @param json_out a Json::Value object.
      */
      void setOutputJson(Json::Value& json_out)
      {
        _json_out=json_out;
      }

    private:
      /**
       * Stores schema being used.
       */
      const char *_schema;
      /**
       * Stores table being used.
       */
      const char *_table;
      /**
       * Stores query being used.
       */
      string _query;
      /**
       * Stores id primary key for a dcument.
       */
      const char *_id;
      /**
       * Stores output json object.
       */
      Json::Value _json_out;
      /**
       * Stores input json object.
       */
      Json::Value _json_in;
      /**
       * Stores http response code.
       */
      int _http_response_code;
      /**
       * Stores http response text.
       */
      const char *_http_response_text;
      /**
       * Stores http request object.
       */
      struct evhttp_request *_req;    
  };

}
}
