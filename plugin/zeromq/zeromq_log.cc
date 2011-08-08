/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Marcus Eriksson
 *
 *  Authors:
 *
 *  Marcus Eriksson <krummas@gmail.com>
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
#include "zeromq_log.h"
#include <drizzled/message/transaction.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <stdio.h>
#include <drizzled/module/registry.h>
#include <drizzled/plugin.h>
#include <stdint.h>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <zmq.h>

namespace po= boost::program_options;

using namespace std;
using namespace drizzled;
using namespace google;

namespace drizzle_plugin
{

ZeroMQLog::ZeroMQLog(const string &name, const string &endpoint) :
  plugin::TransactionApplier(name)
{
  void *context= zmq_init(1);
  _socket= zmq_socket (context, ZMQ_PUB);
  assert (_socket);
  int rc= zmq_bind (_socket, endpoint.c_str());
  assert (rc == 0);
  pthread_mutex_init(&publishLock, NULL);
}

ZeroMQLog::~ZeroMQLog()
{
  zmq_close(_socket);
  pthread_mutex_destroy(&publishLock);
}

plugin::ReplicationReturnCode
ZeroMQLog::apply(Session &, const message::Transaction &to_apply)
{
  size_t message_byte_length= to_apply.ByteSize();
  uint8_t* buffer= new uint8_t[message_byte_length];
  if(buffer == NULL)
  {
    errmsg_printf(error::ERROR, _("Failed to allocate enough memory to transaction message\n"));
    deactivate();
    return plugin::UNKNOWN_ERROR;
  }

  to_apply.SerializeWithCachedSizesToArray(buffer);
  zmq_msg_t msg;
  int rc= zmq_msg_init_size (&msg, message_byte_length);
  assert (rc == 0);
  memcpy(zmq_msg_data(&msg), buffer, message_byte_length);

  pthread_mutex_lock(&publishLock);
  rc= zmq_send (_socket, &msg, 0);
  pthread_mutex_unlock(&publishLock);

  zmq_msg_close (&msg);
  delete[] buffer;
  return plugin::SUCCESS;
}

static ZeroMQLog *zeromqLogger; ///< the actual plugin

/**
 * Initialize the zeromq logger
 */
static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();
  zeromqLogger= new ZeroMQLog("zeromq_log_applier", vm["endpoint"].as<string>());
  context.add(zeromqLogger);
  ReplicationServices::attachApplier(zeromqLogger, vm["use-replicator"].as<string>());
  context.registerVariable(new sys_var_const_string_val("endpoint", vm["endpoint"].as<string>()));
  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("endpoint", 
          po::value<string>()->default_value("tcp://*:9999"),
          _("End point to bind to"));
  context("use-replicator",
          po::value<string>()->default_value("default_replicator"),
          _("Name of the replicator plugin to use (default='default_replicator')"));

}

} /* namespace drizzle_plugin */

DRIZZLE_PLUGIN(drizzle_plugin::init, NULL, drizzle_plugin::init_options);
