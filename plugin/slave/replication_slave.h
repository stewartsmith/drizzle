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

#pragma once

#include <plugin/slave/queue_consumer.h>
#include <plugin/slave/queue_producer.h>
#include <plugin/slave/replication_schema.h>
#include <drizzled/plugin/daemon.h>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>

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
    : drizzled::plugin::Daemon("replication_slave"),
      _config_file(config)
  {}
  
  ~ReplicationSlave()
  {
    _consumer_thread.interrupt();

    boost::unordered_map<uint32_t, Master *>::const_iterator it;

    for (it= _masters.begin(); it != _masters.end(); ++it)
    {
      it->second->thread().interrupt();
    }
  }

  /** Gets called after all plugins are initialized */
  void startup(drizzled::Session &session);

private:

  /**
   * Class representing a master server.
   */
  class Master
  {
  public:
    Master(uint32_t id)
    {
      _producer.setMasterId(id);
    }

    QueueProducer &producer()
    {
      return _producer;
    }
  
    boost::thread &thread()
    {
      return _producer_thread;
    }

    void start()
    {
      _producer_thread= boost::thread(&QueueProducer::run, &_producer);
    }

  private:
    /** Manages a single master */
    QueueProducer _producer;

    /** I/O thread that will populate the work queue */
    boost::thread _producer_thread;
  };

  /** Configuration file containing master info */
  std::string _config_file;

  std::string _error;
  
  /** Object to use with the consumer thread */
  QueueConsumer _consumer;

  /**
   * Applier thread that will drain the work queue.
   * @todo Support more than one consumer thread.
   */
  boost::thread _consumer_thread;

  /** List of master objects, one per master */
  boost::unordered_map<uint32_t, Master *> _masters;

  /** Convenience method to get object reference */
  Master &master(size_t index)
  {
    return *(_masters[index]);
  }

  /**
   * Initialize slave services with the given configuration file.
   *
   * In case of an error during initialization, _error contains a
   * string describing what went wrong.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool initWithConfig();
};
  
} /* namespace slave */

