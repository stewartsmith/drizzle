/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Stewart Smith
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


#include <config.h>
#include <drizzled/module/module.h>
#include <drizzled/module/context.h>
#include <drizzled/plugin/plugin.h>
#include <drizzled/plugin.h>
#include <drizzled/sys_var.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/query_id.h>
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
#include <pthread.h>
#include <drizzled/execute.h>
#include <drizzled/sql/result_set.h>

#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/client.h>
#include <drizzled/catalog/local.h>

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
struct event_base *base= NULL;
struct evhttp *httpd;

static in_port_t getPort(void)
{
  return port.get();
}

extern "C" void process_request(struct evhttp_request *req, void* );
extern "C" void process_root_request(struct evhttp_request *req, void* );
extern "C" void process_api01_version_req(struct evhttp_request *req, void* );
extern "C" void process_api01_sql_req(struct evhttp_request *req, void* );

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
                "var url = document.getElementById(\"baseurl\").innerHTML;\n"
                "var query= document.getElementById(\"query\").value;\n"
                "var xmlHttp = new XMLHttpRequest();\n"
                "xmlHttp.onreadystatechange = function () {\n"
                "if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {\n"
                "var info = eval ( \"(\" + xmlHttp.responseText + \")\" );\n"
                "document.getElementById( \"resultset\").innerHTML= to_table(info.result_set);\n"
                "}\n"
                "};\n"
                "xmlHttp.open(\"POST\", url + \"/0.1/sql\", true);"
                "xmlHttp.send(query);"
                "}"
                "\n\n"
                "function update_version()\n"
                "{drizzle_version(document.getElementById(\"baseurl\").innerHTML);}\n\n"
                "function drizzle_version($url)"
                "{"
                "var xmlHttp = new XMLHttpRequest();\n"
                "xmlHttp.onreadystatechange = function () {\n"
                "if (xmlHttp.readyState == 4 && xmlHttp.status == 200) {\n"
                "var info = eval ( \"(\" + xmlHttp.responseText + \")\" );\n"
                "document.getElementById( \"drizzleversion\").innerHTML= info.version;\n"
                "}\n"
                "};\n"
                "xmlHttp.open(\"GET\", $url + \"/0.1/version\", true);"
                "xmlHttp.send(null);"
                "}"
                "</script>"
                "<p>Drizzle Server at: <a id=\"baseurl\">http://localhost:8765</a></p>"
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
  _session->set_db("test");

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

extern "C" void *libevent_thread(void*);

extern "C" void *libevent_thread(void*)
{
  internal::my_thread_init();

  event_base_dispatch(base);
  evhttp_free(httpd);
  return NULL;
}

static int json_server_init(drizzled::module::Context &context)
{
  context.registerVariable(new sys_var_constrained_value_readonly<in_port_t>("port", port));

  base= event_init();
  httpd= evhttp_new(base);
  if (httpd == NULL)
    return -1;

  int r= evhttp_bind_socket(httpd, "0.0.0.0", getPort());

  if (r != 0)
    return -2;

  evhttp_set_cb(httpd, "/", process_root_request, NULL);
  evhttp_set_cb(httpd, "/0.1/version", process_api01_version_req, NULL);
  evhttp_set_cb(httpd, "/0.1/sql", process_api01_sql_req, NULL);
  evhttp_set_gencb(httpd, process_request, NULL);

  pthread_t libevent_loop_thread;

  pthread_create(&libevent_loop_thread, NULL, libevent_thread, NULL);

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("port",
          po::value<port_constraint>(&port)->default_value(80),
          _("Port number to use for connection or 0 for default (port 80) "));
}

} /* namespace json_server */
} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "json-server",
  "0.1",
  "Stewart Smith",
  "JSON HTTP interface",
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::json_server::json_server_init,             /* Plugin Init */
  NULL, /* depends */
  drizzle_plugin::json_server::init_options    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
