/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
<<<<<<< TREE
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
=======
 *  Copyright (C) 2008 Sun Microsystems
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
>>>>>>> MERGE-SOURCE
 */


#include "config.h"
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/query_id.h>
#include <drizzled/sql_state.h>
#include <drizzled/session.h>
#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/m_string.h"
#include <algorithm>
#include <iostream>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include "drizzle_protocol.h"
#include "plugin/drizzle_protocol/status_table.h"

namespace po= boost::program_options;
using namespace drizzled;
using namespace std;

namespace drizzle_protocol
{

static uint32_t port;
static uint32_t connect_timeout;
static uint32_t read_timeout;
static uint32_t write_timeout;
static uint32_t retry_count;
static uint32_t buffer_length;
static char* bind_address;

static const uint32_t DRIZZLE_TCP_PORT= 4427;

ListenDrizzleProtocol::~ListenDrizzleProtocol()
{
  /* This is strdup'd from the options */
  free(bind_address);
}

const char* ListenDrizzleProtocol::getHost(void) const
{
  return bind_address;
}

in_port_t ListenDrizzleProtocol::getPort(void) const
{
  return (in_port_t) port;
}

static int init(drizzled::module::Context &context)
{  
  const module::option_map &vm= context.getOptions();
  if (vm.count("port"))
  { 
    if (port > 65535)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of port\n"));
      exit(-1);
    }
  }

  if (vm.count("connect-timeout"))
  {
    if (connect_timeout < 1 || connect_timeout > 300)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for connect_timeout\n"));
      exit(-1);
    }
  }

  if (vm.count("read-timeout"))
  {
    if (read_timeout < 1 || read_timeout > 300)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for read_timeout\n"));
      exit(-1);
    }
  }

  if (vm.count("write-timeout"))
  {
    if (write_timeout < 1 || write_timeout > 300)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for write_timeout\n"));
      exit(-1);
    }
  }

  if (vm.count("retry-count"))
  {
    if (retry_count < 1 || retry_count > 100)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for retry_count"));
      exit(-1);
    }
  }

  if (vm.count("buffer-length"))
  {
    if (buffer_length < 1024 || buffer_length > 1024*1024)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value for buffer_length\n"));
      exit(-1);
    }
  }

  if (vm.count("bind-address"))
  {
    bind_address= strdup(vm["bind-address"].as<string>().c_str());
  }

  else
  {
    bind_address= NULL;
  }

  context.add(new StatusTable);
  context.add(new ListenDrizzleProtocol("drizzle_protocol", true));

  return 0;
}

static DRIZZLE_SYSVAR_UINT(port, port, PLUGIN_VAR_RQCMDARG,
                           N_("Port number to use for connection or 0 for default to with Drizzle/MySQL protocol."),
                           NULL, NULL, DRIZZLE_TCP_PORT, 0, 65535, 0);
static DRIZZLE_SYSVAR_UINT(connect_timeout, connect_timeout,
                           PLUGIN_VAR_RQCMDARG, N_("Connect Timeout."),
                           NULL, NULL, 10, 1, 300, 0);
static DRIZZLE_SYSVAR_UINT(read_timeout, read_timeout, PLUGIN_VAR_RQCMDARG,
                           N_("Read Timeout."), NULL, NULL, 30, 1, 300, 0);
static DRIZZLE_SYSVAR_UINT(write_timeout, write_timeout, PLUGIN_VAR_RQCMDARG,
                           N_("Write Timeout."), NULL, NULL, 60, 1, 300, 0);
static DRIZZLE_SYSVAR_UINT(retry_count, retry_count, PLUGIN_VAR_RQCMDARG,
                           N_("Retry Count."), NULL, NULL, 10, 1, 100, 0);
static DRIZZLE_SYSVAR_UINT(buffer_length, buffer_length, PLUGIN_VAR_RQCMDARG,
                           N_("Buffer length."), NULL, NULL, 16384, 1024,
                           1024*1024, 0);
static DRIZZLE_SYSVAR_STR(bind_address, bind_address, PLUGIN_VAR_READONLY,
                          N_("Address to bind to."), NULL, NULL, NULL);

static void init_options(drizzled::module::option_context &context)
{
  context("port",
          po::value<uint32_t>(&port)->default_value(DRIZZLE_TCP_PORT),
          N_("Port number to use for connection or 0 for default to with Drizzle/MySQL protocol."));
  context("connect-timeout",
          po::value<uint32_t>(&connect_timeout)->default_value(10),
          N_("Connect Timeout."));
  context("read-timeout",
          po::value<uint32_t>(&read_timeout)->default_value(30),
          N_("Read Timeout."));
  context("write-timeout",
          po::value<uint32_t>(&write_timeout)->default_value(60),
          N_("Write Timeout."));
  context("retry-count",
          po::value<uint32_t>(&retry_count)->default_value(10),
          N_("Retry Count."));
  context("buffer-length",
          po::value<uint32_t>(&buffer_length)->default_value(16384),
          N_("Buffer length."));
  context("bind-address",
          po::value<string>(),
          N_("Address to bind to."));
}

static drizzle_sys_var* sys_variables[]= {
  DRIZZLE_SYSVAR(port),
  DRIZZLE_SYSVAR(connect_timeout),
  DRIZZLE_SYSVAR(read_timeout),
  DRIZZLE_SYSVAR(write_timeout),
  DRIZZLE_SYSVAR(retry_count),
  DRIZZLE_SYSVAR(buffer_length),
  DRIZZLE_SYSVAR(bind_address),
  NULL
};

} /* namespace drizzle_protocol */

DRIZZLE_PLUGIN(drizzle_protocol::init, drizzle_protocol::sys_variables, drizzle_protocol::init_options);
