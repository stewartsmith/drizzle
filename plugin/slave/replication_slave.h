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

#include <plugin/slave/queue_consumer.h>
#include <plugin/slave/queue_producer.h>
#include <plugin/slave/replication_schema.h>
#include <drizzled/plugin/daemon.h>
#include <boost/thread.hpp>

namespace drizzled
{
  class Session;
}

namespace slave
{

class ReplicationSlave : public drizzled::plugin::Daemon
{
public:

  ReplicationSlave(const std::string &config)
    : drizzled::plugin::Daemon("Replication Slave"),
      _config_file(config)
  {}
  
  ~ReplicationSlave()
  {
    _consumer_thread.interrupt();
    _producer_thread.interrupt();
  }

  void startup(drizzled::Session &session);

  /**
   * Get the error message describing what went wrong during setup.
   */
  const std::string &getError() const
  {
    return _error;
  }

private:
  std::string _config_file;
  std::string _error;

  QueueConsumer _consumer;
  QueueProducer _producer;

  /** Applier thread that will drain the work queue */
  boost::thread _consumer_thread;

  /** I/O thread that will populate the work queue */
  boost::thread _producer_thread;

  /**
   * Initialize slave services with the given configuration file.
   *
   * In case of an error during initialization, you can call the getError()
   * method to get a string describing what went wrong.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool initWithConfig();
};
  
} /* namespace slave */

#endif /* PLUGIN_SLAVE_REPLICATION_SLAVE_H */
