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
#include <plugin/json_server/db_access.h>
#include <plugin/json_server/http_handler.h>
#include <plugin/json_server/http_server.h>

namespace po= boost::program_options;
using namespace drizzled;
using namespace std;

namespace drizzle_plugin
{
namespace json_server
{
static const string DEFAULT_SCHEMA = "test";
static const string DEFAULT_TABLE = "";
static const string JSON_SERVER_VERSION = "0.3";
static const uint32_t DEFAULT_MAX_THREADS= 32;
static const bool DEFAULT_ALLOW_DROP_TABLE=false;
bool allow_drop_table;
string default_schema;
string default_table;
uint32_t max_threads;
uint32_t clone_max_threads=0;
bool updateSchema(Session *, set_var* var); 
bool updateTable(Session *, set_var* var); 
void updateMaxThreads(Session *, sql_var_t);
static port_constraint port;

static in_port_t getPort(void)
{
  return port.get();
}

extern "C" void process_request(struct evhttp_request *req, void* );
extern "C" void process_root_request(struct evhttp_request *req, void* );
extern "C" void process_api01_version_req(struct evhttp_request *req, void* );
extern "C" void process_version_req(struct evhttp_request *req, void* );
extern "C" void process_sql_req(struct evhttp_request *req, void* );
extern "C" void process_json_req(struct evhttp_request *req, void* );
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
                "<select id=\"json_method\"><option value=\"GET\">GET</option>"
                "<option value=\"POST\">POST</option>"
                "<option value=\"PUT\">PUT</option>"
                "<option value=\"DELETE\">DELETE</option></select>"
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

extern "C" void process_version_req(struct evhttp_request *req, void* )
{
  struct evbuffer *buf = evbuffer_new();
  if (buf == NULL) return;

  Json::Value root;
  root["version"]= ::drizzled::version();
  root["json_server_version"]=JSON_SERVER_VERSION;

  Json::StyledWriter writer;
  std::string output= writer.write(root);

  evbuffer_add(buf, output.c_str(), output.length());
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

extern "C" void process_sql_req(struct evhttp_request *req, void* )
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


/**
 * Transform a HTTP request for sql transaction and return results based on input json document.
 * 
 * @todo allow DBA to set whether to use strict mode for parsing json (should get rid of white space), especially for POST of course.
 * 
 * @param req should contain a "table" parameter in request uri. "query", "_id" and "schema" are optional.
 */
extern "C" void process_json_req(struct evhttp_request *req, void* )
{
  Json::Value json_out;
  Json::Value json_in; 
  std::string sql;
  const char* schema;
  const char* table;

  HttpHandler* handler = new HttpHandler(json_out,json_in,req);  
  if(!handler->handleRequest())
  { 
    if(!handler->validate(default_schema,default_table,allow_drop_table))
    {
      json_in= handler->getInputJson();
      schema=handler->getSchema();
      table=handler->getTable();

      DBAccess* dbAccess = new DBAccess(json_in,json_out,req->type,schema,table);
      dbAccess->execute();
      json_out= dbAccess->getOutputJson();
      delete(dbAccess);
    }
    else
    { 
      json_out= handler->getOutputJson();
    }
  }
  else
  {
    json_out= handler->getOutputJson();
  }
  handler->setOutputJson(json_out);
  handler->sendResponse();
  delete(handler);
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


class JsonServer : public drizzled::plugin::Daemon , public HTTPServer
{
private:
  std::vector<drizzled::thread_ptr> json_threads;
  in_port_t _port;
  struct evhttp *httpd;
  struct event_base *base;
  int wakeup_fd[2];
  struct event wakeup_event;
  int nfd;

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
    if ((nfd=BindSocket("0.0.0.0", getPort())) == -1)
    {
      sql_perror("evhttp_bind_socket()");
      return false;
    }
    
    // Create Max_thread number of threads.
    if(not createThreads(max_threads))
    {
      return false;
    }
    
    return true;
  }

  bool createThreads(uint32_t num_threads)
  {
    for(uint32_t i =0;i<num_threads;i++)
    {
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

      if(evhttp_accept_socket(httpd,nfd))
      {
        sql_perror("evhttp_accept_socket()");
        return false;
      }

      // These URLs are available. Bind worker method to each of them. 
      evhttp_set_cb(httpd, "/", process_root_request, NULL);
      // API 0.1
      evhttp_set_cb(httpd, "/0.1/version", process_api01_version_req, NULL);
      // API 0.2
      evhttp_set_cb(httpd, "/0.2/version", process_api01_version_req, NULL);
      // API 0.3
      evhttp_set_cb(httpd, "/0.3/version", process_version_req, NULL);
      evhttp_set_cb(httpd, "/0.3/sql", process_sql_req, NULL);
      evhttp_set_cb(httpd, "/0.3/json", process_json_req, NULL);
      // API "latest" and also available in top level
      evhttp_set_cb(httpd, "/latest/version", process_version_req, NULL);
      evhttp_set_cb(httpd, "/latest/sql", process_sql_req, NULL);
      evhttp_set_cb(httpd, "/latest/json", process_json_req, NULL);
      evhttp_set_cb(httpd, "/version", process_version_req, NULL);
      evhttp_set_cb(httpd, "/sql", process_sql_req, NULL);
      evhttp_set_cb(httpd, "/json", process_json_req, NULL);
        

        event_set(&wakeup_event, wakeup_fd[0], EV_READ | EV_PERSIST, shutdown_event, base);
        event_base_set(base, &wakeup_event);
        if (event_add(&wakeup_event, NULL) < 0)
        {
          sql_perror("event_add");
          return false;
        }
        drizzled::thread_ptr local_thread;
        local_thread.reset(new boost::thread((boost::bind(&run, base))));
        json_threads.push_back(local_thread);

        if (not json_threads[i])
          return false;
    }
    return true;
  }

  ~JsonServer()
  {
    // If we can't write(), we will just muddle our way through the shutdown
    char buffer[1];
    buffer[0]= 4;
    if ((write(wakeup_fd[1], &buffer, 1)) == 1)
    {
      for(uint32_t i=0;i<max_threads;i++)
      {
        json_threads[i]->join();
      }
      evhttp_free(httpd);
      event_base_free(base);
    }
  }
};
JsonServer *server=NULL;

void updateMaxThreads(Session *, sql_var_t)
{
  if (clone_max_threads < max_threads)
  {
    if(server->createThreads(max_threads - clone_max_threads))
    {
      clone_max_threads=max_threads;//success
    }
    else
    {
      //char buf[100];
      //sprintf(buf,"json_server unable to create more threads");
      //my_error(ER_SCRIPT,MYF(0),buf);
      errmsg_printf(error::ERROR,_("json_server unable to create more threads"));
    }
  }
  else
  {
    max_threads = clone_max_threads;
    //my_error(ER_SCRIPT,MYF(0),"json_server_max_threads cannot be smaller than previous configured value");
    errmsg_printf(error::ERROR, _("json_server_max_threadscannot be smaller than previous configured value"));//error
  }
}


bool updateSchema(Session *, set_var* var)
{
  if (not var->value->str_value.empty())
  {
    std::string new_schema(var->value->str_value.data());
    default_schema=new_schema;
      return false; //success
  }
  errmsg_printf(error::ERROR, _("json_server_schema cannot be NULL"));
  return true; // error
}

bool updateTable(Session *, set_var* var)
{
  std::string new_table(var->value->str_value.data());
  default_table=new_table;
  return false;
}

static int json_server_init(drizzled::module::Context &context)
{
 
  server = new JsonServer(port);
  context.add(server);
  context.registerVariable(new sys_var_constrained_value_readonly<in_port_t>("port", port));
  context.registerVariable(new sys_var_std_string("schema", default_schema, NULL, &updateSchema));
  context.registerVariable(new sys_var_std_string("table", default_table, NULL, &updateTable));
  context.registerVariable(new sys_var_bool_ptr("allow_drop_table", &allow_drop_table));
  context.registerVariable(new sys_var_uint32_t_ptr("max_threads",&max_threads,&updateMaxThreads));
  
  clone_max_threads=max_threads;


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
  context("schema",
          po::value<string>(&default_schema)->default_value(DEFAULT_SCHEMA),
          _("Schema in use by json server"));
  context("table",
          po::value<string>(&default_table)->default_value(DEFAULT_TABLE),
          _("table in use by json server"));
  context("allow_drop_table",
          po::value<bool>(&allow_drop_table)->default_value(DEFAULT_ALLOW_DROP_TABLE),
          _("allow to drop table"));
  context("max_threads",
          po::value<uint32_t>(&max_threads)->default_value(DEFAULT_MAX_THREADS),
          _("Maximum threads in use by json server"));

}

} /* namespace json_server */
} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "json_server",
  "0.3",
  "Stewart Smith, Henrik Ingo, Mohit Srivastava",
  N_("JSON HTTP interface"),
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::json_server::json_server_init,
  NULL,
  drizzle_plugin::json_server::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
