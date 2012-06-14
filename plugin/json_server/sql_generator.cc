#include <plugin/json_server/sql_generator.h>
#include <cstdio>

using namespace std;
namespace drizzle_plugin
{
namespace json_server
{

SQLGenerator::SQLGenerator(const Json::Value json_in ,const char* schema ,const char* table)
{
  _json_in=json_in;
  _sql="";
  _schema=schema;
  _table=table;
}

void SQLGenerator::generateSql(const char* s)
{
  if(strcmp(s,"GET")==0)
    generateGetSql();
  else if(strcmp(s,"POST")==0)
    generatePostSql();
  else if(strcmp(s,"DELETE")==0)
    generateDeleteSql();
  else if(strcmp(s,"CREATETABLE")==0)
    generateCreateTableSql();
  else if(strcmp(s,"TABLEEXIST")==0)
    generateIsTableExistsSql();
}

void SQLGenerator::generateGetSql()
{
 _sql="SELECT * FROM `";
 _sql.append(_schema);
 _sql.append("`.`");
 _sql.append(_table);
 _sql.append("`"); 
  if ( _json_in["_id"].asBool() )
    {
      // Now we build an SQL query, using _id from json_in
	_sql.append(" WHERE _id = ");
	_sql.append(_json_in["_id"].asString());
    }
    _sql.append(";");
  
}


void SQLGenerator::generateIsTableExistsSql()
{
    _sql="select count(*) from information_schema.tables where table_schema = '";
    _sql.append(_schema);
    _sql.append("' AND table_name = '");
    _sql.append(_table); 
    _sql.append("';");
}

void SQLGenerator::generateCreateTableSql()
{ 	
      _sql="COMMIT ;";
      _sql.append("CREATE TABLE ");
      _sql.append(_schema);
      _sql.append(".");
      _sql.append(_table);
      _sql.append(" (_id BIGINT PRIMARY KEY auto_increment,");
      // Iterate over json_in keys
      Json::Value::Members createKeys(_json_in.getMemberNames() );
      for ( Json::Value::Members::iterator it = createKeys.begin(); it != createKeys.end(); ++it )
      {
        const std::string &key = *it;
        if(key=="_id") {
           continue;
        }
        _sql.append(key);
        _sql.append(" TEXT");
        if( it !=createKeys.end()-1 && key !="_id")
        {
          _sql.append(",");
        }
      }
      _sql.append(")");
      _sql.append("; ");
}

void SQLGenerator::generatePostSql()
{
 	_sql="REPLACE INTO `";
	_sql.append(_schema);
    	_sql.append("`.`");
    	_sql.append(_table);
    	_sql.append("` SET ");
	
	Json::Value::Members keys( _json_in.getMemberNames() );
    
	for ( Json::Value::Members::iterator it = keys.begin(); it != keys.end(); ++it )
    	{
      		if ( it != keys.begin() )
      			{
        			_sql.append(", ");
      			}
      // TODO: Need to do json_in[].type() first and juggle it from there to be safe. See json/value.h
      		const std::string &key = *it;
      		_sql.append(key); 
		      _sql.append("=");
      		Json::StyledWriter writeobject;
      		switch ( _json_in[key].type() )
      			{
        			case Json::nullValue:
          				_sql.append("NULL");
          				break;
        			case Json::intValue:
        			case Json::uintValue:
        			case Json::realValue:
        			case Json::booleanValue:
          				_sql.append(_json_in[key].asString());
					break;
        			case Json::stringValue:
          				_sql.append("'\"");
          				_sql.append(_json_in[key].asString());
          				_sql.append("\"'");
          				break;
        			case Json::arrayValue:
        			case Json::objectValue:
          				_sql.append("'");
          				_sql.append(writeobject.write(_json_in[key]));
          				_sql.append("'");
          				break;
        			default:	
        				break;
			  }
      		_sql.append(" ");
    		}
    	_sql.append(";");

}

void SQLGenerator::generateDeleteSql()
{
	if ( _json_in["_id"].asBool() )
    	{
		_sql= "DELETE FROM `";
 		_sql.append(_schema);
 		_sql.append("`.`");
 		_sql.append(_table);
 		_sql.append("`");
        	_sql.append(" WHERE _id = ");
        	_sql.append(_json_in["_id"].asString());
    		_sql.append(";");
	}
	else
	{
		_sql="COMMIT ;";
		_sql.append("DROP TABLE `");
		_sql.append(_schema);
		_sql.append("`.`");
		_sql.append(_table);
		_sql.append("`;");
	}

}

const string SQLGenerator::getSQL() const
{
	return _sql;
}


}

}
