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
 * @file Header file for http_handler.cc
 *  
 */

#pragma once

#include <config.h>
#include <evhttp.h>
#include <plugin/json_server/json/json.h>
#include <drizzled/plugin.h>

using namespace std;

namespace drizzle_plugin
{
namespace json_server
{
class HttpHandler
{
  public:

    HttpHandler(Json::Value &json_out,Json::Value &json_in,struct evhttp_request *req);
    bool handleRequest();
    bool validateJson(Json::Reader reader);
    void sendResponse(Json::StyledWriter writer,Json::Value &json_out);

    const char* getSchema() const
    {
      return _schema;
    }

    const char* getTable() const
    {
      return _table;
    }

    const string &getQuery() const
    {
      return _query;
    }

    const char* getId() const
    {
      return _id;
    }

    const Json::Value getOutputJson() const 
    {
     return _json_out;
    }

    const Json::Value getInputJson() const 
    {
     return _json_in;
    }

  private:

    const char *_schema;
    const char *_table;
    string _query;
    const char *_id;
    Json::Value _json_out;
    Json::Value _json_in;
    int _http_response_code;
    const char *_http_response_text;
    struct evhttp_request *_req;    
};

}
}
