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
#include <drizzled/item.h>
#include <stdint.h>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <zmq.h>

namespace po= boost::program_options;

using namespace std;
using namespace drizzled;
using namespace google;

namespace drizzle_plugin {
namespace zeromq {

bool updateEndpoint(Session *, set_var* var);


ZeroMQLog::ZeroMQLog(const string &name, const string &endpoint) :
  plugin::TransactionApplier(name),
  sysvar_endpoint(endpoint)
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

std::string& ZeroMQLog::getEndpoint()
{
  return sysvar_endpoint;
}

bool ZeroMQLog::setEndpoint(std::string new_endpoint)
{
  void *tmp_context= zmq_init(1);
  void *tmp_socket= zmq_socket(tmp_context, ZMQ_PUB);
  if(!tmp_socket)
    return false;
  int tmp_rc= zmq_bind(tmp_socket, new_endpoint.c_str());
  if(tmp_rc!=0)
    return false;
  // need a mutex around this since other threads can try to write to _socket while we are changing the endpoint
  pthread_mutex_lock(&publishLock);

  zmq_close(_socket);
  _socket= tmp_socket;
  sysvar_endpoint= new_endpoint;
  
  //Releasing the mutex lock
  pthread_mutex_unlock(&publishLock);
  return true;
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

  string schema= getSchemaName(to_apply);
  zmq_msg_t schemamsg;
  int rc= zmq_msg_init_size(&schemamsg, schema.length());
  memcpy(zmq_msg_data(&schemamsg), schema.c_str(), schema.length());

  to_apply.SerializeWithCachedSizesToArray(buffer);
  zmq_msg_t msg;
  rc= zmq_msg_init_size(&msg, message_byte_length);
  assert (rc == 0);
  memcpy(zmq_msg_data(&msg), buffer, message_byte_length);

  // need a mutex around this since several threads can call this method at the same time
  pthread_mutex_lock(&publishLock);
  rc= zmq_send(_socket, &schemamsg, ZMQ_SNDMORE);
  rc= zmq_send(_socket, &msg, 0);
  pthread_mutex_unlock(&publishLock);

  zmq_msg_close(&msg);
  zmq_msg_close(&schemamsg);
  delete[] buffer;
  return plugin::SUCCESS;
}

string ZeroMQLog::getSchemaName(const message::Transaction &txn) {
  if(txn.statement_size() == 0) return "";

  const message::Statement &statement= txn.statement(0);

  switch(statement.type())
  {
	case message::Statement::INSERT:
	  return statement.insert_header().table_metadata().schema_name();
	case message::Statement::UPDATE:
	  return statement.update_header().table_metadata().schema_name();
	case message::Statement::DELETE:
	  return statement.delete_header().table_metadata().schema_name();
	case message::Statement::CREATE_TABLE:
	  return statement.create_table_statement().table().schema();
	case message::Statement::TRUNCATE_TABLE:
	  return statement.truncate_table_statement().table_metadata().schema_name();
	case message::Statement::DROP_TABLE:
	  return statement.drop_table_statement().table_metadata().schema_name();
	case message::Statement::CREATE_SCHEMA:
	  return statement.create_schema_statement().schema().name();
	case message::Statement::DROP_SCHEMA:
	  return statement.drop_schema_statement().schema_name();
    default:
	  return "";
  }
}

static ZeroMQLog *zeromqLogger; ///< the actual plugin

/**
 * This function is called when the value of zeromq_endpoint is updated in the system
 *
 * @return False on success, True on error.
 */
bool updateEndpoint(Session *, set_var* var)
{
  if (not var->value->str_value.empty())
  {
    std::string new_endpoint(var->value->str_value.data());
    if (zeromqLogger->setEndpoint(new_endpoint))
      return false; //success
    else
      return true; // error
  }
  errmsg_printf(error::ERROR, _("zeromq_endpoint cannot be NULL"));
  return true; // error
}

/**
 * Initialize the zeromq logger
 */
static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();
  zeromqLogger= new ZeroMQLog("zeromq_applier", vm["endpoint"].as<string>());
  context.add(zeromqLogger);
  ReplicationServices::attachApplier(zeromqLogger, vm["use-replicator"].as<string>());
  context.registerVariable(new sys_var_std_string("endpoint", zeromqLogger->getEndpoint(), NULL, &updateEndpoint));
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

} /* namespace zeromq */
} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "zeromq",
  "0.1",
  "Marcus Eriksson",
  N_("Publishes transactions to ZeroMQ"),
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::zeromq::init,
  NULL,
  drizzle_plugin::zeromq::init_options,
}
DRIZZLE_DECLARE_PLUGIN_END;
