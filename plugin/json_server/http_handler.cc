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
 * @file Implements a class HttpHandler which handles the operations related to HTTP request and response.
 */
#include <plugin/json_server/http_handler.h>

using namespace std;

namespace drizzle_plugin
{
namespace json_server
{
  HttpHandler::HttpHandler(Json::Value& json_out,Json::Value json_in,struct evhttp_request *req)
  {
    _schema=NULL;
    _table=NULL;
    _id=NULL;
    _query="";
    _http_response_code=HTTP_OK;
    _http_response_text="OK";
    _json_out=json_out;
    _json_in=json_in;
    _req=req;
  }
  
  bool HttpHandler::handleRequest()
  { 
    evhttp_parse_query(evhttp_request_uri(_req), _req->input_headers);
    if(_req->type== EVHTTP_REQ_POST )
    {
      char buffer[1024];
      int l=0;
      do
      {
        l= evbuffer_remove(_req->input_buffer, buffer, 1024);
        _query.append(buffer, l);
      }
      while(l);
    }
    else
    {
      const char* input_query;
      input_query= (char *)evhttp_find_header(_req->input_headers, "query");
      if(input_query == NULL || strcmp(input_query,"")==0)
      {
        input_query="{}";
      }
      _query.append(input_query,strlen(input_query));
    }

    _schema = (char *)evhttp_find_header(_req->input_headers, "schema");
    _table = (char *)evhttp_find_header(_req->input_headers, "table");
    _id = (char *)evhttp_find_header(_req->input_headers, "_id");

    return false;
  }
  
  bool HttpHandler::validate(string &default_schema,string &default_table,bool allow_drop_table)
  {
  
    if(not _schema || strcmp(_schema, "") == 0)
    {
      _schema = default_schema.c_str();
    }
    if(not _table || strcmp(_table,"")==0)
    {
      _table= default_table.c_str();
    }
    if(not _table || strcmp(_table,"")==0)
    {
      generateHttpError();
      return true;
    }
    
    // Parse query object from json doc
    Json::Features json_conf;
    Json::Reader reader(json_conf);
    bool retval = reader.parse(_query,_json_in);
    if (retval != true) 
    {
     _json_out["error_type"]="json error";
     _json_out["error_message"]= reader.getFormatedErrorMessages();
    }
    
    // If _id was given as a URI parameter, copy the value to query object now.
    if ( !_json_in["query"]["_id"].asBool() )
    {
      if( _id ) 
      {
        _json_in["query"]["_id"] = (Json::Value::UInt) atol(_id);
      }
    }
    // Check parameters from URI string
    if((_json_in["query"]["_id"].isNull() || _json_in["query"]["_id"].asString()=="")  && _req->type==EVHTTP_REQ_DELETE && !allow_drop_table)
    {
      generateDropTableError();
      return true;
    }
    return !retval;
  }

  void HttpHandler::generateHttpError()
  {
    _json_out["error_type"]="http error";
    _json_out["error_message"]= "table must be specified in URI query string.";
    _http_response_code = HTTP_NOTFOUND;
    _http_response_text = "table must be specified in URI query string.";
  }

  void HttpHandler::generateDropTableError()
  {
    _json_out["error_type"]="http error";
    _json_out["error_message"]= "_id must be specified in URI query string or set --json_server.allow_drop_table =true";
    _http_response_code= HTTP_NOTFOUND;
    _http_response_text= "_id must be specified in URI query string or set --json_server.allow_drop_table =true";
  }
  
  void HttpHandler::sendResponse()
  { 
    struct evbuffer *buf = evbuffer_new();
    if(buf == NULL)
    {
      return;
    }
    Json::StyledWriter writer;
    std::string output= writer.write(_json_out);
    evbuffer_add(buf, output.c_str(), output.length());
    evhttp_send_reply( _req, _http_response_code, _http_response_text, buf);  
  }

}
}
