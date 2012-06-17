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
 * @file Header file for sql_to_json_generator.cc
 *  
 */
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
    void generateJson(enum evhttp_cmd_type type);
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

