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
#include <drizzled/program_options/config_file.h>
#include <drizzled/errmsg_print.h>
#include <boost/program_options.hpp>
#include <fstream>
#include <drizzled/plugin.h>

using namespace std;
using namespace drizzled;

namespace po= boost::program_options;

namespace slave
{

/* Gets called after all plugins are initialized. */
void ReplicationSlave::startup(Session &session)
{
  (void)session;
  if (not initWithConfig())
  {
    errmsg_printf(error::ERROR,
                  _("Could not start slave services: %s\n"),
                  getError().c_str());
  }
  else
  {
    _consumer_thread= boost::thread(&QueueConsumer::run, &_consumer);
    _producer_thread= boost::thread(&QueueProducer::run, &_producer);
  }
}

bool ReplicationSlave::initWithConfig()
{
  po::variables_map vm;
  po::options_description slave_options("Options for the slave plugin");

  slave_options.add_options()
    ("master-host", po::value<string>()->default_value(""))
    ("master-port", po::value<uint16_t>()->default_value(3306))
    ("master-user", po::value<string>()->default_value(""))
    ("master-pass", po::value<string>()->default_value(""))
    ("max-reconnects", po::value<uint32_t>()->default_value(10))
    ("seconds-between-reconnects", po::value<uint32_t>()->default_value(30))
    ("io-thread-sleep", po::value<uint32_t>()->default_value(5))
    ("applier-thread-sleep", po::value<uint32_t>()->default_value(5));

  ifstream cf_stream(_config_file.c_str());

  if (not cf_stream.is_open())
  {
    _error= "Unable to open file ";
    _error.append(_config_file);
    return false;
  }

  po::store(drizzled::program_options::parse_config_file(cf_stream, slave_options), vm);

  po::notify(vm);

  if (vm.count("master-host"))
    _producer.setMasterHost(vm["master-host"].as<string>());

  if (vm.count("master-port"))
    _producer.setMasterPort(vm["master-port"].as<uint16_t>());

  if (vm.count("master-user"))
    _producer.setMasterUser(vm["master-user"].as<string>());

  if (vm.count("master-pass"))
    _producer.setMasterPassword(vm["master-pass"].as<string>());

  if (vm.count("max-reconnects"))
    _producer.setMaxReconnectAttempts(vm["max-reconnects"].as<uint32_t>());

  if (vm.count("seconds-between-reconnects"))
    _producer.setSecondsBetweenReconnects(vm["seconds-between-reconnects"].as<uint32_t>());

  if (vm.count("io-thread-sleep"))
    _producer.setSleepInterval(vm["io-thread-sleep"].as<uint32_t>());

  if (vm.count("applier-thread-sleep"))
    _consumer.setSleepInterval(vm["applier-thread-sleep"].as<uint32_t>());

  /* setup schema and tables */
  ReplicationSchema rs;
  if (not rs.create())
  {
    _error= rs.getErrorMessage();
    return false;
  }

  if (_initial_max_commit_id)
  {
    if (not rs.setInitialMaxCommitId(_initial_max_commit_id))
    {
      _error= rs.getErrorMessage();
      return false;
    }
  }

  return true;
}

} /* namespace slave */
