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
       * a constructor
       * intializes a member variables.
       * @param json_out a Json::Value object.
       * @param json_in a Json::Value object.
       * @param req a evhttp_request pointer.
       */
      HttpHandler(Json::Value &json_out,Json::Value &json_in,struct evhttp_request *req);
      /**
       * a function variable.
       * used to parse http request and retrieves various http headers.
       * @return false Success
       * @return true Failure
       */
      void handleRequest();
      /**
       * a function variable.
       * used to parse input json and generate input json object.
       * @param reader a Json::Reader object.
       * @return false Success.
       * @return true Failure.
       */
      bool validateJson(Json::Reader reader);
      /**
       * a function variable.
       * used to send http response back.
       * @param writer a Json::Writer object.
       */
      void sendResponse(Json::StyledWriter writer,Json::Value &json_out);
      /**
       * a function variable.
       * used to generate a http error when table is null.
       */
      void generateHttpError();
      /**
       * a constant function variable.
       * used to get schema being used.
       * @return a constant schema string.
       */
      const char* getSchema() const
      {
        return _schema;
      }
      /**
       * a constant function variable.
       * used to get table being used.
       * @return a constant table string.
       */
      const char* getTable() const
      {
        return _table;
      }
      /**
       * a constant function variable.
       * used to get query being used.
       * @return a constant query string.
       */
      const string &getQuery() const
      {
        return _query;
      }
      /**
       * a constant function variable.
       * used to get id being used.
       * @return a constant id string.
       */
      const char* getId() const
      {
        return _id;
      }
      /**
       * a constant function variable.
       * used to get output json object.
       * @return a constant json object.
       */
      const Json::Value getOutputJson() const 
      {
        return _json_out;
      }
      /**
       * a constant function variable.
       * used to get input json object.
       * @return a constant json object.
       */
      const Json::Value getInputJson() const 
      {
        return _json_in;
      }

    private:
      /**
       * a private variable.
       * stores schema being used.
       */
      const char *_schema;
      /**
       * a private variable.
       * stores table being used.
       */
      const char *_table;
      /**
       * a private variable.
       * stores query being used.
       */
      string _query;
      /**
       * a private variable.
       * stores id primary key for a dcument.
       */
      const char *_id;
      /**
       * a private variable.
       * stores output json object.
       */
      Json::Value _json_out;
      /**
       * a private variable.
       * stores input json object.
       */
      Json::Value _json_in;
      /**
       * a private variable.
       * stores http response code.
       */
      int _http_response_code;
      /**
       * a private variable.
       * stores http response text.
       */
      const char *_http_response_text;
      /**
       * a private variable.
       * stores http request object.
       */
      struct evhttp_request *_req;    
  };

}
}
