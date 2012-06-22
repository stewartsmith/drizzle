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
 * @file Declare a class SQLGenerator that helps to generate sql string corresponding to a request.   
 */
#pragma once

#include <config.h>
#include <cstring>
#include <cstdio>
#include <boost/program_options.hpp>
#include <plugin/json_server/json/json.h>
#include <plugin/json_server/http_handler.h>

using namespace std;
/**
 *  Drizzle Plugin Namespace
 */
namespace drizzle_plugin
{
/**
 *  Json Server Plugin Namespace
 */
namespace json_server
{
  /**
   * Generate sql string from input json.
   */
  class SQLGenerator
  {
    private:
      /**
       * Stores input json object. 
       */
      Json::Value _json_in;
      /**
       * Stores output json object. 
       */
      Json::Value _json_out;
      /**
       * Stores sql string. 
       */
      string _sql;
      /**
       * Stores schema being used. 
       */
      const char* _schema;
      /**
       * Stores table being used. 
       */
      const char* _table;
      /**
       * Generate sql string corresponds to GET request.
       */
      void generateGetSql() ;
      /**
       * Generate sql string corresponds to POST request.
       */
      void generatePostSql() ;
      /**
       * Generate sql string corresponds to DELETE request.
       */
      void generateDeleteSql() ; 

    public:
      /**
       * Constructor.
       * 
       * @param json_in a Json::Value object.
       * @param schema a constant character pointer.
       * @param table a constant character pointer.
       */
      SQLGenerator(const Json::Value json_in,const char* schema,const char* table);
      /**
       * Generate sql string that corresponds to a request type.
       * @param type GET, POST or DELETE
       */
      void generateSql(enum evhttp_cmd_type type);
      /**
       * Generate CREATE TABLE sql string (for POST, when needed)
       */
      void generateCreateTableSql();
      /**
       * Get sql string.
       * @return a constant sql string.
       */
      const string getSQL() const
      {
	      return _sql;
      }
  };
}
}
