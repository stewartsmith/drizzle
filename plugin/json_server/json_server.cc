/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Stewart Smith, Henrik Ingo
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
 *  - The mapping of /0.1/ and /0.2/ URLs is now a lot of copy paste, probably
 *    needs to evolve to something smarter at some point.
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

  output.append("<html><head><title>JSON DATABASE interface demo</title></head>"
                "<body>"
                "<script lang=\"javascript\">"
                "function to_table(obj) {"
                " var str = '<table>';"
                "for (var r=0; r< obj.length; r++) {"
                " str+='<tr>';"
                "  for (var c=0; c < obj[r].length; c++) {"
                "    str+= '<td>' + obj[r][c] + '</td>';"
                "  }"
                " str+='</tr>';"
                "}"
                "str+='</table>';"
                "return str;"
                "}"
                "function run_query()\n"
                "{"
                "var url = window.location;\n"
                "var query= document.getElementById(\"query\").value;\n"
                "var xmlHttp = new XMLHttpRequest();\n"
                "xmlHttp.onreadystatechange = function () {\n"
                "if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {\n"
                "var info = eval ( \"(\" + xmlHttp.responseText + \")\" );\n"
                "document.getElementById( \"resultset\").innerHTML= to_table(info.result_set);\n"
                "}\n"
                "};\n"
                "xmlHttp.open(\"POST\", url + \"sql\", true);"
                "xmlHttp.send(query);"
                "}"
                "\n\n"
                "function update_version()\n"
                "{drizzle_version(window.location);}\n\n"
                "function drizzle_version($url)"
                "{"
                "var xmlHttp = new XMLHttpRequest();\n"
                "xmlHttp.onreadystatechange = function () {\n"
                "if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {\n"
                "var info = eval ( \"(\" + xmlHttp.responseText + \")\" );\n"
                "document.getElementById( \"drizzleversion\").innerHTML= info.version;\n"
                "}\n"
                "};\n"
                "xmlHttp.open(\"GET\", $url + \"version\", true);"
                "xmlHttp.send(null);"
                "}"
                "</script>"
                "<p>Drizzle server version: <a id=\"drizzleversion\"></a></p>"
                "<p><textarea rows=\"3\" cols=\"40\" id=\"query\">"
                "SELECT * from DATA_DICTIONARY.GLOBAL_STATUS;"
                "</textarea>"
                "<button type=\"button\" onclick=\"run_query();\">Execute Query</button>"
                "<div id=\"resultset\"/>"
                "<script lang=\"javascript\">update_version(); run_query();</script>"
                "</body></html>");

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

  // Parse "input" into "json_in". Strict mode means comments are discarded.
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
    // Now we "parse" the json_in object and build an SQL query
    // TODO: implement get first, we need to have both put and get. (Use REPLACE INTO for put, so you get updates too.)
    // TODO: learn how delete is done: a delete command or just "put" empty json object?
    char sql[1024] = "SELECT v FROM jsonkv WHERE _id=%s;";
    sprintf(sql, json_in["_id"].asCString());

    // We have sql string. Use Execute API to run it and convert results back to JSON.
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

    json_out["sqlstate"]= exception.getSQLState();

    if ((err != drizzled::EE_OK) && (err != drizzled::ER_EMPTY_QUERY))
    {
        json_out["error_type"]="sql error";
        json_out["error_message"]= exception.getErrorMessage();
        json_out["error_code"]= exception.getErrorCode();
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
        json_out["result_set"].append(json_row);
    }

    json_out["query"]= input;
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
    drizzled::plugin::Daemon("JSON Server"),
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
          // TODO: research whether to call this MQL, Mongo or whatever...
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
  "json-server",
  "0.1",
  "Stewart Smith, Henrik Ingo",
  "JSON HTTP interface",
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::json_server::json_server_init,             /* Plugin Init */
  NULL, /* depends */
  drizzle_plugin::json_server::init_options    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
