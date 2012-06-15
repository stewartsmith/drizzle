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

#pragma once

#include <config.h>
#include <cstring>
#include <cstdio>
#include <boost/program_options.hpp>
#include <plugin/json_server/json/json.h>
#include <plugin/json_server/http_handler.h>

using namespace std;
namespace drizzle_plugin
{
namespace json_server
{

class SQLGenerator
{
  private:
    
    Json::Value _json_in;
    Json::Value _json_out;
    string _sql;
    const char* _schema;
    const char* _table;

    void generateGetSql() ;
    void generatePostSql() ;
    void generateDeleteSql() ;
    void generateCreateTableSql();
    void generateIsTableExistsSql();

  public:

    SQLGenerator(const Json::Value json_in,const char* schema,const char* table);
    void generateSql(enum evhttp_cmd_type type);
    const string getSQL() const;
};

}
}
