/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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
#include <drizzled/util/tokenize.h>
#include "drizzle_protocol.h"

namespace po= boost::program_options;
using namespace drizzled;
using namespace std;

namespace drizzle_plugin {
namespace drizzle_protocol {

static port_constraint port;
static timeout_constraint connect_timeout;
static timeout_constraint read_timeout;
static timeout_constraint write_timeout;
static retry_constraint retry_count;
static buffer_constraint buffer_length;

static const uint32_t DRIZZLE_TCP_PORT= 4427;

ProtocolCounters ListenDrizzleProtocol::drizzle_counters;

in_port_t ListenDrizzleProtocol::getPort() const
{
  return port;
}

plugin::Client *ListenDrizzleProtocol::getClient(int fd)
{
  int new_fd= acceptTcp(fd);
  return new_fd == -1 ? NULL : new ClientMySQLProtocol(new_fd, getCounters());
}

static int init(drizzled::module::Context &context)
{  
  const module::option_map &vm= context.getOptions();

  ListenDrizzleProtocol *protocol=new ListenDrizzleProtocol("drizzle_protocol", vm["bind-address"].as<std::string>());
  protocol->addCountersToTable();
  context.add(protocol);
  context.registerVariable(new sys_var_constrained_value_readonly<in_port_t>("port", port));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("connect_timeout", connect_timeout));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("read_timeout", read_timeout));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("write_timeout", write_timeout));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("retry_count", retry_count));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("buffer_length", buffer_length));
  context.registerVariable(new sys_var_const_string_val("bind_address", vm["bind-address"].as<std::string>()));
  context.registerVariable(new sys_var_uint32_t_ptr("max-connections", &ListenDrizzleProtocol::drizzle_counters.max_connections));

  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("port",
          po::value<port_constraint>(&port)->default_value(DRIZZLE_TCP_PORT),
          N_("Port number to use for connection or 0 for default to with Drizzle/MySQL protocol."));
  context("connect-timeout",
          po::value<timeout_constraint>(&connect_timeout)->default_value(10),
          N_("Connect Timeout."));
  context("read-timeout",
          po::value<timeout_constraint>(&read_timeout)->default_value(30),
          N_("Read Timeout."));
  context("write-timeout",
          po::value<timeout_constraint>(&write_timeout)->default_value(60),
          N_("Write Timeout."));
  context("retry-count",
          po::value<retry_constraint>(&retry_count)->default_value(10),
          N_("Retry Count."));
  context("buffer-length",
          po::value<buffer_constraint>(&buffer_length)->default_value(16384),
          N_("Buffer length."));
  context("bind-address",
          po::value<std::string>()->default_value("localhost"),
          N_("Address to bind to."));
  context("max-connections",
          po::value<uint32_t>(&ListenDrizzleProtocol::drizzle_counters.max_connections)->default_value(1000),
          N_("Maximum simultaneous connections."));
}

} /* namespace drizzle_protocol */
} /* namespace drizzle_plugin */

DRIZZLE_PLUGIN(drizzle_plugin::drizzle_protocol::init, NULL, drizzle_plugin::drizzle_protocol::init_options);
