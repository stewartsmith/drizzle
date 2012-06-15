#include <plugin/json_server/db_access.h>

namespace drizzle_plugin
{
namespace json_server
{
  DBAccess::DBAccess(Json::Value &json_in,Json::Value &json_out)
  {
    _json_in= json_in;
    _json_out= json_out;
  }
  
  void DBAccess::execute(enum evhttp_cmd_type type,const char* schema,const char* table)
  {
      std::string sql;
      SQLGenerator* generator = new SQLGenerator(_json_in,schema,table);
      generator->generateSql(type);
      sql= generator->getSQL();

      SQLExecutor* executor = new SQLExecutor("",schema);
      SQLToJsonGenerator* jsonGenerator = new SQLToJsonGenerator(_json_out,schema,table,executor);
      if(executor->executeSQL(sql))
      {
        jsonGenerator->generateJson(type);  
      }
      else
      {
        jsonGenerator->generateSQLErrorJson();
      }
      _json_out= jsonGenerator->getJson();
   }
  
}
}
