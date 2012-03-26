/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Stewart Smith, Henrik Ingo, Mohit Srivastava
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
 * @file Implements an HTTP server that will parse JSON and SQL queries
 * 
 * @todo Refactoring ideas:
 *  - Anything HTML should really be a separate file, not strings embedded
 *    in C++.
 *  - Put all json handling into try/catch blocks, the parser likes to throw
 *    exceptions which crash drizzled if not caught.
 *  - Need to implement a worker thread pool. Make workers proper OO classes.
 * 
 * @todo Implement HTTP response codes other than just 200 as defined in
 *       http://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html
 *
 * @todo Shouldn't we be using event2/http.h? Why does this even work without it?
 */

#include <config.h>

#include <unistd.h>
#include <fcntl.h>

#include <drizzled/module/module.h>
#include <drizzled/module/context.h>
#include <drizzled/plugin/plugin.h>
#include <drizzled/plugin.h>
#include <drizzled/plugin/daemon.h>
#include <drizzled/sys_var.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>
#include <algorithm>
#include <iostream>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <drizzled/constrained_value.h>
#include <evhttp.h>
#include <event.h>
#include <drizzled/execute.h>
#include <drizzled/sql/result_set.h>

#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/client.h>
#include <drizzled/catalog/local.h>

#include <drizzled/pthread_globals.h>
#include <boost/bind.hpp>


#include <drizzled/version.h>
#include <plugin/json_server/json/json.h>

namespace po= boost::program_options;
using namespace drizzled;
using namespace std;

namespace drizzle_plugin
{
namespace json_server
{

static port_constraint port;

static in_port_t getPort(void)
{
  return port.get();
}

extern "C" void process_request(struct evhttp_request *req, void* );
extern "C" void process_root_request(struct evhttp_request *req, void* );
extern "C" void process_api01_version_req(struct evhttp_request *req, void* );
extern "C" void process_api01_sql_req(struct evhttp_request *req, void* );
extern "C" void process_api02_json_req(struct evhttp_request *req, void* );
extern "C" void process_api02_json_get_req(struct evhttp_request *req, void* );
extern "C" void process_api02_json_post_req(struct evhttp_request *req, void* );
/* extern "C" void process_api02_json_put_req(struct evhttp_request *req, void* ); */
extern "C" void process_api02_json_delete_req(struct evhttp_request *req, void* );
extern "C" void process_request(struct evhttp_request *req, void* )
{
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;
  evbuffer_add_printf(buf, "Requested: %s\n", evhttp_request_uri(req));
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

extern "C" void process_root_request(struct evhttp_request *req, void* )
{
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;

  std::string output;

  output.append("<html><head><title>JSON DATABASE interface demo</title></head>\n"
                "<body>\n"
                "<script lang=\"javascript\">\n"
                "function to_table(obj) {\n"
                " var str = '<table border=\"1\">';\n"
                "for (var r=0; r< obj.length; r++) {\n"
                " str+='<tr>';\n"
                "  for (var c=0; c < obj[r].length; c++) {\n"
                "    str+= '<td>' + obj[r][c] + '</td>';\n"
                "  }\n"
                " str+='</tr>';\n"
                "}\n"
                "str+='</table>';\n"
                "return str;\n"
                "}\n"
                "function to_table_from_json(obj) {\n"
                " var str = '<table border=\"1\">';\n"
                "for (var r=0; r< obj.length; r++) {\n"
                " str+='<tr>';\n"
                " str+='<td>' + obj[r]['_id'] + '</td>';\n"
                " str+='<td>' + JSON.stringify(obj[r]['document']) + '</td>';\n"
                " str+='</tr>';\n"
                "}\n"
                "str+='</table>';\n"
                "return str;\n"
                "}\n"
                "function run_sql_query()\n"
                "{\n"
                "var url = window.location;\n"
                "var query= document.getElementById(\"sql_query\").value;\n"
                "var xmlHttp = new XMLHttpRequest();\n"
                "xmlHttp.onreadystatechange = function () {\n"
                "document.getElementById(\"responseText\").value = xmlHttp.responseText;\n"
                "if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {\n"
                "var info = eval ( \"(\" + xmlHttp.responseText + \")\" );\n"
                "document.getElementById( \"resultset\").innerHTML= to_table(info.result_set);\n"
                "}\n"
                "};\n"
                "xmlHttp.open(\"POST\", url + \"sql\", true);\n"
                "xmlHttp.send(query);\n"
                "}\n"
                "\n\n"
                "function run_json_query()\n"
                "{\n"
//"alert('run_json_query');"
                    "var url = window.location;\n"
                    "var method= document.getElementById(\"json_method\").value;\n"
                    "var query= document.getElementById(\"json_query\").value;\n"
                    "var schema= document.getElementById(\"schema\").value;\n"
                    "var table= document.getElementById(\"table\").value;\n"
                    "var xmlHttp = new XMLHttpRequest();\n"
                    "xmlHttp.onreadystatechange = function () {\n"
//"alert(xmlHttp.responseText);"
                    "document.getElementById(\"responseText\").value = xmlHttp.responseText;\n"
                    "if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {\n"
                    "var info = eval ( \"(\" + xmlHttp.responseText + \")\" );\n"
                    "document.getElementById( \"resultset\").innerHTML= to_table_from_json(info.result_set);\n"
                    "}\n"
                    "};\n"
                    "if( method == \"POST\" ) {\n"
                        "xmlHttp.open(method, url + \"json?schema=\" + schema + \"&table=\" + table, true);\n"
                        "xmlHttp.send(query);\n"
                    "} else {\n"
                        "xmlHttp.open(method, url + \"json?schema=\" + schema + \"&table=\" + table + \"&query=\" + encodeURIComponent(query), true);\n"
                        "xmlHttp.send();\n"
                    "}\n"
                "}\n"
                "\n\n"
                "function update_version()\n"
                "{drizzle_version(window.location);}\n\n"
                "function drizzle_version($url)\n"
                "{\n"
                "var xmlHttp = new XMLHttpRequest();\n"
                "xmlHttp.onreadystatechange = function () {\n"
                "if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {\n"
                "var info = eval ( \"(\" + xmlHttp.responseText + \")\" );\n"
                "document.getElementById( \"drizzleversion\").innerHTML= info.version;\n"
                "}\n"
                "};\n"
                "xmlHttp.open(\"GET\", $url + \"version\", true);\n"
                "xmlHttp.send(null);\n"
                "}\n"
                "</script>\n"
                "<p>Drizzle server version: <a id=\"drizzleversion\"></a></p>\n"
                "<p><textarea rows=\"3\" cols=\"80\" id=\"sql_query\">\n"
                "SELECT * from DATA_DICTIONARY.GLOBAL_STATUS;\n"
                "</textarea>\n"
                "<button type=\"button\" onclick=\"run_sql_query();\">Execute SQL Query</button>\n"
                "</p><p>\n"
                "<textarea rows=\"8\" cols=\"80\" id=\"json_query\">\n"
                "{\"_id\" : 1}\n"
                "</textarea>\n"
                "<button type=\"button\" onclick=\"run_json_query();\">Execute JSON Query</button>\n"
                "<br />\n"
                "<select id=\"json_method\"><option value\"GET\">GET</option>"
                "<option value\"POST\">POST</option>"
                "<option value\"PUT\">PUT</option>"
                "<option value\"DELETE\">DELETE</option></select>"
                "<script lang=\"javascript\">document.write(window.location);</script>json?schema=\n"
                "<input type=\"text\" id=\"schema\" value=\"test\"/>"
                "&amp;table=<input type=\"text\" id=\"table\" value=\"jsonkv\"/>\n"
                "</p><hr />\n<div id=\"resultset\"></div>\n"
                "<hr /><p><textarea rows=\"12\" cols=\"80\" id=\"responseText\" ></textarea></p>"
                "<script lang=\"javascript\">update_version(); run_sql_query();</script>\n"
                "</body></html>\n");

  evbuffer_add(buf, output.c_str(), output.length());
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

extern "C" void process_api01_version_req(struct evhttp_request *req, void* )
{
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;

  Json::Value root;
  root["version"]= ::drizzled::version();

  Json::StyledWriter writer;
  std::string output= writer.write(root);

  evbuffer_add(buf, output.c_str(), output.length());
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

extern "C" void process_api01_sql_req(struct evhttp_request *req, void* )
{
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;

  std::string input;
  char buffer[1024];
  int l=0;
  do {
    l= evbuffer_remove(req->input_buffer, buffer, 1024);
    input.append(buffer, l);
  }while(l);

  drizzled::Session::shared_ptr _session= drizzled::Session::make_shared(drizzled::plugin::Listen::getNullClient(),
                                           drizzled::catalog::local());
  drizzled::identifier::user::mptr user_id= identifier::User::make_shared();
  user_id->setUser("");
  _session->setUser(user_id);
  _session->set_schema("test");

  drizzled::Execute execute(*(_session.get()), true);

  drizzled::sql::ResultSet result_set(1);

  /* Execute wraps the SQL to run within a transaction */
  execute.run(input, result_set);
  drizzled::sql::Exception exception= result_set.getException();

  drizzled::error_t err= exception.getErrorCode();

  Json::Value root;
  root["sqlstate"]= exception.getSQLState();

  if ((err != drizzled::EE_OK) && (err != drizzled::ER_EMPTY_QUERY))
  {
    root["error_message"]= exception.getErrorMessage();
    root["error_code"]= exception.getErrorCode();
    root["schema"]= "test";
  }

  while (result_set.next())
  {
    Json::Value json_row;
    for (size_t x= 0; x < result_set.getMetaData().getColumnCount(); x++)
    {
      if (not result_set.isNull(x))
      {
        json_row[x]= result_set.getString(x);
      }
    }
    root["result_set"].append(json_row);
  }

  root["query"]= input;

  Json::StyledWriter writer;
  std::string output= writer.write(root);

  evbuffer_add(buf, output.c_str(), output.length());
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

extern "C" void process_api02_json_req(struct evhttp_request *req, void* )
{
    if( req->type == EVHTTP_REQ_GET )
    {
        process_api02_json_get_req( req, NULL);
//    } elseif ( req->type == EVHTTP_REQ_PUT ) {
        //process_api02_json_put_req( req, NULL);
    } else if ( req->type == EVHTTP_REQ_POST ) {
        process_api02_json_post_req( req, NULL);
    } else if ( req->type == EVHTTP_REQ_DELETE ) {
        process_api02_json_delete_req( req, NULL);
    }
}

/**
 * Transform a HTTP GET to SELECT and return results based on input json document
 * 
 * @todo allow DBA to set default schema (also in post,del methods)
 * @todo allow DBA to set whether to use strict mode for parsing json (should get rid of white space), especially for POST of course.
 * 
 * @param req should contain a "table" parameter in request uri. "query", "_id" and "schema" are optional.
 * @return a json document is returned to client with evhttp_send_reply()
 */
void process_api02_json_get_req(struct evhttp_request *req, void* )
{
  int http_response_code = HTTP_OK;
  const char *http_response_text;
  http_response_text = "OK";
  
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;

  Json::Value json_out;

  std::string input;
  // Schema and table are given in request uri.
  // TODO: If we want to be really NoSQL, we will some day allow to use synonyms like "collection" instead of "table".
  // For GET, also the query is in the uri
  const char *schema;
  const char *table;
  const char *query;
  const char *id;
  evhttp_parse_query(evhttp_request_uri(req), req->input_headers);
  schema = (char *)evhttp_find_header(req->input_headers, "schema");
  table = (char *)evhttp_find_header(req->input_headers, "table");
  query = (char *)evhttp_find_header(req->input_headers, "query");
  id = (char *)evhttp_find_header(req->input_headers, "_id");
  
  // query can be null if _id was given
  if ( query == NULL || strcmp(query, "") == 0 )
  {
      // Empty JSON object
      query = "{}";
  }
  input.append(query, strlen(query));

  // Set test as default schema
  if (strcmp( schema, "") || schema == NULL)
  {
      schema = "test";
  }
  
  // Parse "input" into "json_in".
  Json::Value  json_in;
  Json::Features json_conf;
  Json::Reader reader(json_conf);
  bool retval = reader.parse(input, json_in);
  if (retval != true) {
    json_out["error_type"]="json error";
    json_out["error_message"]= reader.getFormatedErrorMessages();
  }
  else if (strcmp( table, "") == 0 || table == NULL) {
    json_out["error_type"]="http error";
    json_out["error_message"]= "You must specify \"table\" in the request uri query string.";
    http_response_code = HTTP_NOTFOUND;
    http_response_text = "You must specify \"table\" in the request uri query string.";
  }
  else {    
    // It is allowed to specify _id in the uri and leave it out from the json query.
    // In that case we put the value from uri into json_in here.
    // If both are specified, the one existing in json_in wins. (This is still valid, no error.)
    if ( ! json_in["_id"].asBool() )
    {
      if( id ) {
        json_in["_id"] = (Json::Value::UInt) atol(id);
      }
    }
    
    // TODO: In a later stage we'll allow the situation where _id isn't given but some other column for where.
    // TODO: Need to do json_in[].type() first and juggle it from there to be safe. See json/value.h
    // TODO: Don't SELECT * but only fields given in json query document
    char sqlformat[1024];;
    char buffer[1024];
    if ( json_in["_id"].asBool() )
    {
      // Now we build an SQL query, using _id from json_in
      sprintf(sqlformat, "%s", "SELECT * FROM `%s`.`%s` WHERE _id=%i;");
      sprintf(buffer, sqlformat, schema, table, json_in["_id"].asInt());
    }
    else {
      // If neither _id nor query are given, we return the full table. (No error, maybe this is what you really want? Blame yourself.)
      sprintf(sqlformat, "%s", "SELECT * FROM `%s`.`%s`;");
      sprintf(buffer, sqlformat, schema, table);
    }

    std::string sql = "";
    sql.append(buffer, strlen(buffer));
    
    // We have sql string. Use Execute API to run it and convert results back to JSON.
    drizzled::Session::shared_ptr _session= drizzled::Session::make_shared(drizzled::plugin::Listen::getNullClient(),
                                            drizzled::catalog::local());
    drizzled::identifier::user::mptr user_id= identifier::User::make_shared();
    user_id->setUser("");
    _session->setUser(user_id);
    //_session->set_schema("test");

    drizzled::Execute execute(*(_session.get()), true);

    drizzled::sql::ResultSet result_set(1);

    /* Execute wraps the SQL to run within a transaction */
    execute.run(sql, result_set);
    drizzled::sql::Exception exception= result_set.getException();

    drizzled::error_t err= exception.getErrorCode();

    json_out["sqlstate"]= exception.getSQLState();

    if ((err != drizzled::EE_OK) && (err != drizzled::ER_EMPTY_QUERY))
    {
        json_out["error_type"]="sql error";
        json_out["error_message"]= exception.getErrorMessage();
        json_out["error_code"]= exception.getErrorCode();
        json_out["internal_sql_query"]= sql;
        json_out["schema"]= "test";
    }

    while (result_set.next())
    {
        Json::Value json_row;
        bool got_error = false; 
        for (size_t x= 0; x < result_set.getMetaData().getColumnCount() && got_error == false; x++)
        {
            if (not result_set.isNull(x))
            {
                // The values are now serialized json. We must first
                // parse them to make them part of this structure, only to immediately
                // serialize them again in the next step. For large json documents
                // stored into the blob this must be very, very inefficient.
                // TODO: Implement a smarter way to push the blob value directly to the client. Probably need to hand code some string appending magic.
                // TODO: Massimo knows of a library to create JSON in streaming mode.
                Json::Value  json_doc;
                Json::Reader readrow(json_conf);
                std::string col_name = result_set.getColumnInfo(x).col_name;
                bool r = readrow.parse(result_set.getString(x), json_doc);
                if (r != true) {
                    json_out["error_type"]="json parse error on row value";
                    json_out["error_internal_sql_column"]=col_name;
                    json_out["error_message"]= reader.getFormatedErrorMessages();
                    // Just put the string there as it is, better than nothing.
                    json_row[col_name]= result_set.getString(x);
                    got_error=true;
                    break;
                }
                else {
                    json_row[col_name]= json_doc;
                }
            }
        }
        // When done, append this to result set tree
        json_out["result_set"].append(json_row);
    }

    json_out["query"]= json_in;
  }
  // Return either the results or an error message, in json.
  Json::StyledWriter writer;
  std::string output= writer.write(json_out);
  evbuffer_add(buf, output.c_str(), output.length());
  evhttp_send_reply(req, http_response_code, http_response_text, buf);
}

/**
 * Input json document or update existing one from HTTP POST (and PUT?).
 * 
 * If json document specifies _id field, then record is updated. If it doesn't
 * exist, then a new record is created with that _id.
 * 
 * If _id field is not specified, then a new record is created using 
 * auto_increment value. The _id of the created value is returned in the http
 * response.
 * 
 * @todo If there are multiple errors, last one overwrites the previous in json_out. Make them lists.
 * 
 * @param req should contain a "table" parameter in request uri. "schema" is optional.
 * @return a json document is returned to client with evhttp_send_reply()
 */
void process_api02_json_post_req(struct evhttp_request *req, void* )
{
  bool table_exists = true;
  Json::Value json_out;

  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;

  // Read from http to string "input".
  std::string input;
  char buffer[1024];
  int l=0;
  do {
    l= evbuffer_remove(req->input_buffer, buffer, 1024);
    input.append(buffer, l);
  }while(l);

  // Schema and table are given in request uri.
  // TODO: If we want to be really NoSQL, we will some day allow to use synonyms like "collection" instead of "table".
  const char *schema;
  const char *table;
  const char *id;
  evhttp_parse_query(evhttp_request_uri(req), req->input_headers);
  schema = (char *)evhttp_find_header(req->input_headers, "schema");
  table = (char *)evhttp_find_header(req->input_headers, "table");
  id = (char *)evhttp_find_header(req->input_headers, "_id");
  
  // Set test as default schema
  if (strcmp( schema, "") || schema == NULL)
  {
      schema = "test";
  }
  
  // Parse "input" into "json_in".
  Json::Value  json_in;
  Json::Features json_conf;
  Json::Reader reader(json_conf);
  bool retval = reader.parse(input, json_in);
  if (retval != true) {
    json_out["error_type"]="json error";
    json_out["error_message"]= reader.getFormatedErrorMessages();
  } 
  else {
    // It is allowed to specify _id in the uri and leave it out from the json query.
    // In that case we put the value from uri into json_in here.
    // If both are specified, the one existing in json_in wins. (This is still valid, no error.)
    if ( ! json_in["_id"].asBool() )
    {
      if( id ) {
        json_in["_id"] = (Json::Value::UInt) atol(id);
      }
    }

    // For POST method, we check if table exists.
    // If it doesn't, we automatically CREATE TABLE that matches the structure
    // in the given json document. (This means, your first JSON document must
    // contain all top-level keys you'd like to use.)
    drizzled::Session::shared_ptr _session= drizzled::Session::make_shared(drizzled::plugin::Listen::getNullClient(),
                                            drizzled::catalog::local());
    drizzled::identifier::user::mptr user_id= identifier::User::make_shared();
    user_id->setUser("");
    _session->setUser(user_id);
    drizzled::Execute execute(*(_session.get()), true);

    drizzled::sql::ResultSet result_set(1);
    std::string sql="select count(*) from information_schema.tables where table_schema = '";
    sql.append(schema);
    sql.append("' AND table_name = '");
    sql.append(table); sql.append("';"); 
    /* Execute wraps the SQL to run within a transaction */
    execute.run(sql, result_set);

    drizzled::sql::Exception exception= result_set.getException();

    drizzled::error_t err= exception.getErrorCode();
    while(result_set.next())
    {
      if(result_set.getString(0)=="0")
      {
        table_exists = false;
      }
    }
    if(table_exists == false)
    {
      std::string tmp = "CREATE TABLE ";
      tmp.append(schema);
      tmp.append(".");
      tmp.append(table);
      tmp.append(" (_id BIGINT PRIMARY KEY auto_increment,");
      // Iterate over json_in keys
      Json::Value::Members createKeys( json_in.getMemberNames() );
      for ( Json::Value::Members::iterator it = createKeys.begin(); it != createKeys.end(); ++it )
      {
        const std::string &key = *it;
        if(key=="_id") {
           continue;
        }
        tmp.append(key);
        tmp.append(" TEXT");
        if( it !=createKeys.end()-1 && key !="_id")
        {
          tmp.append(",");
        }
      }  
      tmp.append(")"); 
      vector<string> csql;       
      csql.clear();
      csql.push_back("COMMIT");
      csql.push_back (tmp);
      sql.clear();       
      BOOST_FOREACH(string& it, csql)
      {
        sql.append(it);
        sql.append("; ");
      }
      drizzled::sql::ResultSet createtable_result_set(1);
      execute.run(sql, createtable_result_set);
        
      exception= createtable_result_set.getException();
      err= exception.getErrorCode();
    }
    // Now we "parse" the json_in object and build an SQL query
    sql.clear();
    sql.append("REPLACE INTO `");
    sql.append(schema);
    sql.append("`.`");
    sql.append(table);
    sql.append("` SET ");    
    // Iterate over json_in keys
    Json::Value::Members keys( json_in.getMemberNames() );
    for ( Json::Value::Members::iterator it = keys.begin(); it != keys.end(); ++it )
    {
      if ( it != keys.begin() )
      {
        sql.append(", ");
      }
      // TODO: Need to do json_in[].type() first and juggle it from there to be safe. See json/value.h
      const std::string &key = *it;
      sql.append(key); sql.append("=");
      Json::StyledWriter writeobject;
      switch ( json_in[key].type() )
      {
        case Json::nullValue:
          sql.append("NULL");
          break;
        case Json::intValue:
        case Json::uintValue:
        case Json::realValue:
        case Json::booleanValue:
          sql.append(json_in[key].asString());
          break;
        case Json::stringValue:
          sql.append("'\"");
          // TODO: MUST be sql quoted!
          sql.append(json_in[key].asString());
          sql.append("\"'");
          break;
        case Json::arrayValue:
        case Json::objectValue:
          sql.append("'");
          sql.append(writeobject.write(json_in[key]));
          sql.append("'");
          break;
        default:
          sql.append("'Error in json_server.cc. This should never happen.'");
          json_out["error_type"]="json error";
          json_out["error_message"]= "json_in object had a value that wasn't of any of the types that we recognize.";
        break;
      }
      sql.append(" ");
    }
    sql.append(";");
    drizzled::sql::ResultSet replace_result_set(1);

    // Execute wraps the SQL to run within a transaction
    execute.run(sql, replace_result_set);
   
    exception= replace_result_set.getException();

    err= exception.getErrorCode();

    json_out["sqlstate"]= exception.getSQLState();

    // TODO: I should be able to return number of rows inserted/updated.
    // TODO: Return last_insert_id();
    if ((err != drizzled::EE_OK) && (err != drizzled::ER_EMPTY_QUERY))
    {
      json_out["error_type"]="sql error";
      json_out["error_message"]= exception.getErrorMessage();
      json_out["error_code"]= exception.getErrorCode();
      json_out["internal_sql_query"]= sql;
      json_out["schema"]= "test";
    }
    json_out["query"]= json_in;
  }
  // Return either the results or an error message, in json.
  Json::StyledWriter writer;
  std::string output= writer.write(json_out);
  evbuffer_add(buf, output.c_str(), output.length());
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

/*
void process_api02_json_put_req(struct evhttp_request *req, void* )
{
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}
*/

void process_api02_json_delete_req(struct evhttp_request *req, void* )
{
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;

  Json::Value json_out;

  std::string input;
  char buffer[1024];

  // Schema and table are given in request uri.
  // TODO: If we want to be really NoSQL, we will some day allow to use synonyms like "collection" instead of "table".
  // For GET, also the query is in the uri
  const char *schema;
  const char *table;
  const char *query;
  const char *id;
  evhttp_parse_query(evhttp_request_uri(req), req->input_headers);
  schema = (char *)evhttp_find_header(req->input_headers, "schema");
  table = (char *)evhttp_find_header(req->input_headers, "table");
  query = (char *)evhttp_find_header(req->input_headers, "query");
  id = (char *)evhttp_find_header(req->input_headers, "_id");

  // query can be null if _id was given
  if ( query == NULL || strcmp(query, "") == 0 )
  {
      // Empty JSON object
      query = "{}";
  }
  input.append(query, strlen(query));

  // Set test as default schema
  if ( strcmp( schema, "") || schema == NULL)
  {
      schema = "test";
  }

  // Parse "input" into "json_in".
  Json::Value  json_in;
  Json::Features json_conf;
  json_conf.strictMode();
  Json::Reader reader(json_conf);
  bool retval = reader.parse(input, json_in);
  if (retval != true) {
    json_out["error_type"]="json error";
    json_out["error_message"]= reader.getFormatedErrorMessages();
  }
  else {
    // It is allowed to specify _id in the uri and leave it out from the json query.
    // In that case we put the value from uri into json_in here.
    // If both are specified, the one existing in json_in wins. (This is still valid, no error.)
    if ( ! json_in["_id"].asBool() )
    {
      if( id ) {
        json_in["_id"] = (Json::Value::UInt) atol(id);
      }
    }
    // Now we "parse" the json_in object and build an SQL query
    char sqlformat[1024] = "DELETE FROM `%s`.`%s` WHERE _id=%i;";
    sprintf(buffer, sqlformat, schema, table, json_in["_id"].asInt());
    std::string sql = "";
    sql.append(buffer, strlen(buffer));

    // We have sql string. Use Execute API to run it and convert results back to JSON.
    drizzled::Session::shared_ptr _session= drizzled::Session::make_shared(drizzled::plugin::Listen::getNullClient(),
                                            drizzled::catalog::local());
    drizzled::identifier::user::mptr user_id= identifier::User::make_shared();
    user_id->setUser("");
    _session->setUser(user_id);

    drizzled::Execute execute(*(_session.get()), true);

    drizzled::sql::ResultSet result_set(1);

    /* Execute wraps the SQL to run within a transaction */
    execute.run(sql, result_set);
    drizzled::sql::Exception exception= result_set.getException();

    drizzled::error_t err= exception.getErrorCode();

    json_out["sqlstate"]= exception.getSQLState();

   if ((err != drizzled::EE_OK) && (err != drizzled::ER_EMPTY_QUERY))
    {
        json_out["error_type"]="sql error";
        json_out["error_message"]= exception.getErrorMessage();
        json_out["error_code"]= exception.getErrorCode();
        json_out["internal_sql_query"]= sql;
        json_out["schema"]= "test";
    }
    json_out["query"]= json_in;
  }
  // Return either the results or an error message, in json.
  Json::StyledWriter writer;
  std::string output= writer.write(json_out);
  evbuffer_add(buf, output.c_str(), output.length());
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
  
 }


static void shutdown_event(int fd, short, void *arg)
{
  struct event_base *base= (struct event_base *)arg;
  event_base_loopbreak(base);
  close(fd);
}


static void run(struct event_base *base)
{
  internal::my_thread_init();

  event_base_dispatch(base);
}


class JsonServer : public drizzled::plugin::Daemon
{
private:
  drizzled::thread_ptr json_thread;
  in_port_t _port;
  struct evhttp *httpd;
  struct event_base *base;
  int wakeup_fd[2];
  struct event wakeup_event;

public:
  JsonServer(in_port_t port_arg) :
    drizzled::plugin::Daemon("json_server"),
    _port(port_arg),
    httpd(NULL),
    base(NULL)
  { }

  bool init()
  {
    if (pipe(wakeup_fd) < 0)
    {
      sql_perror("pipe");
      return false;
    }

    int returned_flags;
    if ((returned_flags= fcntl(wakeup_fd[0], F_GETFL, 0)) < 0)
    {
      sql_perror("fcntl:F_GETFL");
      return false;
    }

    if (fcntl(wakeup_fd[0], F_SETFL, returned_flags | O_NONBLOCK) < 0)

    {
      sql_perror("F_SETFL");
      return false;
    }

    if ((base= event_init()) == NULL)
    {
      sql_perror("event_init()");
      return false;
    }

    if ((httpd= evhttp_new(base)) == NULL)
    {
      sql_perror("evhttp_new()");
      return false;
    }


    if ((evhttp_bind_socket(httpd, "0.0.0.0", getPort())) == -1)
    {
      sql_perror("evhttp_bind_socket()");
      return false;
    }

    // These URLs are available. Bind worker method to each of them. 
    // Please group by api version. Also unchanged functions must be copied to next version!
    evhttp_set_cb(httpd, "/", process_root_request, NULL);
    // API 0.1
    evhttp_set_cb(httpd, "/0.1/version", process_api01_version_req, NULL);
    evhttp_set_cb(httpd, "/0.1/sql", process_api01_sql_req, NULL);
    // API 0.2
    evhttp_set_cb(httpd, "/0.2/version", process_api01_version_req, NULL);
    evhttp_set_cb(httpd, "/0.2/sql", process_api01_sql_req, NULL);
    evhttp_set_cb(httpd, "/0.2/json", process_api02_json_req, NULL);
    // API "latest" and also available in top level
    evhttp_set_cb(httpd, "/latest/version", process_api01_version_req, NULL);
    evhttp_set_cb(httpd, "/latest/sql", process_api01_sql_req, NULL);
    evhttp_set_cb(httpd, "/latest/json", process_api02_json_req, NULL);
    evhttp_set_cb(httpd, "/version", process_api01_version_req, NULL);
    evhttp_set_cb(httpd, "/sql", process_api01_sql_req, NULL);
    evhttp_set_cb(httpd, "/json", process_api02_json_req, NULL);    
    // Catch all does nothing and returns generic message.
    //evhttp_set_gencb(httpd, process_request, NULL);

    event_set(&wakeup_event, wakeup_fd[0], EV_READ | EV_PERSIST, shutdown_event, base);
    event_base_set(base, &wakeup_event);
    if (event_add(&wakeup_event, NULL) < 0)
    {
      sql_perror("event_add");
      return false;
    }

    json_thread.reset(new boost::thread((boost::bind(&run, base))));

    if (not json_thread)
      return false;

    return true;
  }

  ~JsonServer()
  {
    // If we can't write(), we will just muddle our way through the shutdown
    char buffer[1];
    buffer[0]= 4;
    if ((write(wakeup_fd[1], &buffer, 1)) == 1)
    {
      json_thread->join();
      evhttp_free(httpd);
      event_base_free(base);
    }
  }
};

static int json_server_init(drizzled::module::Context &context)
{
  context.registerVariable(new sys_var_constrained_value_readonly<in_port_t>("port", port));

  JsonServer *server;
  context.add(server= new JsonServer(port));

  if (server and not server->init())
  {
    return -2;
  }

  return bool(server) ? 0 : 1;
}

static void init_options(drizzled::module::option_context &context)
{
  context("port",
          po::value<port_constraint>(&port)->default_value(8086),
          _("Port number to use for connection or 0 for default (port 8086) "));
}

} /* namespace json_server */
} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "json_server",
  "0.1",
  "Stewart Smith, Henrik Ingo, Mohit Srivastava",
  N_("JSON HTTP interface"),
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::json_server::json_server_init,
  NULL,
  drizzle_plugin::json_server::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
