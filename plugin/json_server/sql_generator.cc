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
 * @file Implements the various memver function of class SQLGenerator 
 * which helps to generate the SQL strings corresponding to type of HTTP Request.
 *  
 */

#include <plugin/json_server/sql_generator.h>
#include <cstdio>

using namespace std;
namespace drizzle_plugin
{
namespace json_server
{
  SQLGenerator::SQLGenerator(const Json::Value json_in ,const char* schema ,const char* table)
  {
    _json_in=json_in["query"];
    _sql="";
    _schema=schema;
    _table=table;
  }

  void SQLGenerator::generateSql(enum evhttp_cmd_type type)
  {
    if(type==EVHTTP_REQ_GET)
      generateGetSql();
    else if(type==EVHTTP_REQ_POST)
      generatePostSql();
    else if(type==EVHTTP_REQ_DELETE)
      generateDeleteSql();
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
  	  _sql.append(" WHERE _id = ");
	    _sql.append(_json_in["_id"].asString());
    }
    _sql.append(";");
  
  }
 
  void SQLGenerator::generateCreateTableSql()
  { 	
    _sql="COMMIT;";
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
      if(key=="_id") 
      {
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
    _sql.append(";");
  }
 
  void SQLGenerator::generatePostSql()
  {
    _sql="COMMIT;";
 	  _sql.append("REPLACE INTO `");
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

}
}
