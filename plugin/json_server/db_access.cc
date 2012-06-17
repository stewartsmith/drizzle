#include <plugin/json_server/db_access.h>

namespace drizzle_plugin
{
namespace json_server
{
  DBAccess::DBAccess(Json::Value &json_in,Json::Value &json_out,enum evhttp_cmd_type type,const char* schema,const char* table)
  {
    _json_in= json_in;
    _json_out= json_out;
    _type=type;
    _schema=schema;
    _table=table;
  }
  
  void DBAccess::execute()
  {
      std::string sql;
      SQLGenerator* generator = new SQLGenerator(_json_in,_schema,_table);
      generator->generateSql(_type);
      sql= generator->getSQL();

      SQLExecutor* executor = new SQLExecutor("",_schema);
      SQLToJsonGenerator* jsonGenerator = new SQLToJsonGenerator(_json_out,_schema,_table,executor);
      if(executor->executeSQL(sql))
      {
        jsonGenerator->generateJson(_type);  
      }
      else
      {
        if(_type == EVHTTP_REQ_POST && executor->getErr() == drizzled::ER_TABLE_UNKNOWN)
        {
          generator->generateCreateTableSql();
          sql= generator->getSQL();
          if(executor->executeSQL(sql))
          {
            generator->generateSql(_type);
            sql= generator->getSQL();
            if(executor->executeSQL(sql))
            {
              jsonGenerator->generateJson(_type);
            }
            else
            {
              jsonGenerator->generateSQLErrorJson();
            }
          }
          else
          {
            jsonGenerator->generateSQLErrorJson();
          }
        }
        else
        {
          jsonGenerator->generateSQLErrorJson();
        }
      }
      _json_out= jsonGenerator->getJson();
   }
  
}
}
