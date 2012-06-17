#include <config.h>
#include <plugin/json_server/json/json.h>
#include <plugin/json_server/sql_generator.h>
#include <plugin/json_server/sql_executor.h>
#include <plugin/json_server/sql_to_json_generator.h>
#include <plugin/json_server/http_handler.h>

namespace drizzle_plugin
{
namespace json_server
{
  class DBAccess
  {
    private:

      Json::Value _json_in;
      Json::Value _json_out;
      enum evhttp_cmd_type _type;
      const char* _schema;
      const char* _table;
      
    public:

      const Json::Value getOutputJson() const
      {
        return _json_out;
      }
      const Json::Value getInputJson() const
      {
        return _json_in;
      }
      
      DBAccess(Json::Value &json_in,Json::Value &json_out,enum evhttp_cmd_type type,const char* schema,const char* table);
      void execute();
  
  };
}
}
