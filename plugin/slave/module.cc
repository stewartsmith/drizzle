/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
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
#include <plugin/slave/replication_slave.h>
#include <drizzled/plugin.h>
#include <drizzled/configmake.h>   // for SYSCONFDIR
#include <drizzled/module/option_map.h>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <string>

using namespace drizzled;
using namespace std;

namespace po= boost::program_options;
namespace fs= boost::filesystem;

namespace slave
{

static const fs::path DEFAULT_SLAVE_CFG_FILE= SYSCONFDIR "/slave.cfg";

static string slave_config;

static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  ReplicationSlave *slave= new ReplicationSlave(vm["config-file"].as<string>());
  context.add(slave);
  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("config-file",
          po::value<string>()->default_value(DEFAULT_SLAVE_CFG_FILE.string()),
          N_("Path to the slave configuration file"));
}

} /* namespace slave */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "slave",
  "1.1",
  "David Shrewsbury",
  N_("Drizzle replication slave"),
  PLUGIN_LICENSE_GPL,
  slave::init,
  NULL,
  slave::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
