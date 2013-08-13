 /* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
  *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
  *
  *  Copyright (C) 2011-2013 Stewart Smith, Henrik Ingo, Mohit Srivastava
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
/*
 * @file Implements a class JsonHandler which handles the operations related to Json.
 */ 
#include <plugin/json_server/json_handler.h>
#include <string.h>
using namespace drizzled;
namespace drizzle_plugin
{
namespace json_server
{
  void JsonHandler::generate_input_query(struct evhttp_request *req_arg)
  {
    evhttp_parse_query(evhttp_request_uri(req_arg), req_arg->input_headers);
    if(req_arg->type== EVHTTP_REQ_POST )
    {
      char buffer[1024];
      int l=0;
      do
      {
        l= evbuffer_remove(req_arg->input_buffer, buffer, 1024);
        _input_query.append(buffer, l);
      }
      while(l);
    }
    else
    {
      const char* _query;
      _query= (char *)evhttp_find_header(req_arg->input_headers, "query");
      if(_query == NULL || strcmp(_query,"")==0)
      {
        _query="{}";
      }
      _input_query.append(_query,strlen(_query));
    }

  }

  void JsonHandler::generate_input_json(struct evhttp_request *req_arg,JsonErrorArea &_json_error_area)
  {
    generate_input_query(req_arg);
    Json::Features _json_conf;
    Json::Reader reader(_json_conf);
    bool retval = reader.parse(_input_query,_json_in);
    if(retval!=true)
    {
      _json_error_area.set_error(JsonErrorArea::ER_JSON,drizzled::EE_OK,reader.getFormatedErrorMessages().c_str()); 
    }
  }

  void JsonHandler::generate_output_json(JsonErrorArea& _json_error_area)
  {
    if(_json_error_area.is_error())
    {
      if(_json_error_area.is_sqlerror())
      {
        _json_out["error_type"]=_json_error_area.get_error_type_string();
        _json_out["error_no"]=_json_error_area.get_error_no();
        _json_out["error_message"]=_json_error_area.get_error_msg();
        _json_out["sql_state"]=_json_error_area.get_sql_state();
      
      }
      else
      {
        _json_out["error_type"]=_json_error_area.get_error_type_string();
        _json_out["error_message"]=_json_error_area.get_error_msg();
      }
    }
    else
    {
      _json_out["sql_state"]=_json_error_area.get_sql_state();
    }
    
  }
  
  void JsonHandler::generate_output_query(JsonErrorArea& _json_error_area)
  {
    generate_output_json(_json_error_area);
    Json::StyledWriter writer;
    _output_query= writer.write(_json_out);
  }
}
}
