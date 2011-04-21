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
#include <pthread.h>

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
  evbuffer_add_printf(buf, "Handling root request for URI %s\n", evhttp_request_uri(req));
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

extern "C" void *libevent_thread(void*);

extern "C" void *libevent_thread(void*)
{
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
