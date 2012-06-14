#include <plugin/json_server/http_handler.h>

using namespace std;

namespace drizzle_plugin
{
namespace json_server
{
  HttpHandler::HttpHandler(Json::Value &json_out,Json::Value &json_in,struct evhttp_request *req)
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
      const char* _input_query;
      _input_query = (char *)evhttp_find_header(_req->input_headers, "query");
      
      if ( _input_query == NULL || strcmp(_input_query, "") == 0 )
      {
        _input_query = "{}";
      }
      _query.append(_input_query,strlen(_input_query));
    }

    _schema = (char *)evhttp_find_header(_req->input_headers, "schema");
    _table = (char *)evhttp_find_header(_req->input_headers, "table");
    _id = (char *)evhttp_find_header(_req->input_headers, "_id");
    
    
    if( _schema == NULL || strcmp(_schema, "") == 0)
    {
      _schema = "test";
    }
    
    if (_table == NULL || strcmp(_table, "")==0)
    {
      _json_out["error_type"]="http error";
      _json_out["error_message"]= "Table isn't specify in URI query string";
      _http_response_code = HTTP_NOTFOUND;
      _http_response_text = "Table should be specify in URI query string.";
      return false;
    }

    return true;
  }
  
  bool HttpHandler::validateJson(Json::Reader reader)
  {
    bool retval = reader.parse(_query,_json_in);
    if (retval != true) 
    {
      _json_out["error_type"]="json error";
      _json_out["error_message"]= reader.getFormatedErrorMessages();
    }
    if ( !_json_in["_id"].asBool() )
    {
      if( _id ) 
      {
        _json_in["_id"] = (Json::Value::UInt) atol(_id);
      }
    }
    return retval;
  }

  void HttpHandler::sendResponse(Json::StyledWriter writer,Json::Value &__json_out)
  {
    struct evbuffer *buf = evbuffer_new();
    if(buf == NULL)
    {
      return;
    }

    std::string output= writer.write(__json_out);
    evbuffer_add(buf, output.c_str(), output.length());
    evhttp_send_reply( _req, _http_response_code, _http_response_text, buf);  
  }

}
}
