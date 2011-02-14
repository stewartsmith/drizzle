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

#ifndef PLUGIN_SLAVE_REPLICATION_SLAVE_H
#define PLUGIN_SLAVE_REPLICATION_SLAVE_H

#include "plugin/slave/queue_consumer.h"
#include "plugin/slave/queue_producer.h"
#include "drizzled/plugin/daemon.h"
#include "drizzled/program_options/config_file.h"
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <fstream>

namespace po= boost::program_options;

namespace slave
{

class ReplicationSlave : public drizzled::plugin::Daemon
{
public:

  ReplicationSlave() : drizzled::plugin::Daemon("Replication Slave")
  {}
  
  ~ReplicationSlave()
  {
    _consumer_thread.interrupt();
    _producer_thread.interrupt();
  }

  /**
   * Initialize slave services with the given configuration file.
   *
   * In case of an error during initialization, you can call the getError()
   * method to get a string describing what went wrong.
   *
   * @param[in] config_file Full path to the configuration file.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool initWithConfig(const std::string &config_file)
  {
    po::variables_map vm;
    po::options_description slave_options("Options for the slave plugin");

    slave_options.add_options()
      ("master-host", po::value<std::string>()->default_value(""))
      ("master-port", po::value<uint16_t>()->default_value(3306))
      ("master-user", po::value<std::string>()->default_value(""))
      ("master-pass", po::value<std::string>()->default_value(""));

    std::ifstream cf_stream(config_file.c_str());
    po::store(drizzled::program_options::parse_config_file(cf_stream, slave_options), vm);

    po::notify(vm);

    if (vm.count("master-host"))
      _producer.setMasterHost(vm["master-host"].as<std::string>());

    if (vm.count("master-port"))
      _producer.setMasterPort(vm["master-port"].as<uint16_t>());

    if (vm.count("master-user"))
      _producer.setMasterUser(vm["master-user"].as<std::string>());

    if (vm.count("master-pass"))
      _producer.setMasterPassword(vm["master-pass"].as<std::string>());

    _consumer_thread= boost::thread(&QueueConsumer::run, &_consumer);
    _producer_thread= boost::thread(&QueueProducer::run, &_producer);

    return true;
  }

  /**
   * Get the error message describing what went wrong during setup.
   */
  const std::string &getError() const
  {
    return _error;
  }

private:
  std::string _error;

  QueueConsumer _consumer;
  QueueProducer _producer;

  /** Applier thread that will drain the work queue */
  boost::thread _consumer_thread;

  /** I/O thread that will populate the work queue */
  boost::thread _producer_thread;
};
  
} /* namespace slave */

#endif /* PLUGIN_SLAVE_REPLICATION_SLAVE_H */
