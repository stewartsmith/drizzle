#include <config.h>
#include <plugin/json_server/json/json.h>
#include <plugin/json_server/sql_executor.h>
#include <plugin/json_server/http_handler.h>
#include <string>

using namespace std;

namespace drizzle_plugin
{
namespace json_server
{
class SQLToJsonGenerator
{
  public:

    SQLToJsonGenerator(Json::Value &json_out,const char* schema,const char* table,SQLExecutor *sqlExecutor);
    void generateSQLErrorJson();
    void generateJson(const char* s);
    const Json::Value getJson() const 
    {
     return _json_out;
    }

  private:
    Json::Value _json_out;
    SQLExecutor* _sql_executor;
    HttpHandler* _http_handler;
    const char* _schema;
    const char* _table;

    void generateGetJson();
    void generatePostJson();
    void generateDeleteJson();

};
}

}

