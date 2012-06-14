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
