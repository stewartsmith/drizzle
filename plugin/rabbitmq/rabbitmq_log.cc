/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Marcus Eriksson
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
#include "rabbitmq_log.h"
#include <drizzled/message/transaction.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <stdio.h>
#include <drizzled/module/registry.h>
#include <drizzled/plugin.h>
#include <stdint.h>
#include "rabbitmq_handler.h"
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>

namespace po= boost::program_options;

using namespace std;
using namespace drizzled;
using namespace google;

namespace drizzle_plugin
{

/**
 * rabbitmq port
 */
static port_constraint sysvar_rabbitmq_port;


RabbitMQLog::RabbitMQLog(const string &name, 
                         RabbitMQHandler* mqHandler) :
  plugin::TransactionApplier(name),
  _rabbitMQHandler(mqHandler)
{ }

RabbitMQLog::~RabbitMQLog() 
{ 
  _rabbitMQHandler->disconnect();
  delete _rabbitMQHandler;
}

plugin::ReplicationReturnCode
RabbitMQLog::apply(Session &, const message::Transaction &to_apply)
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
  short tries = 3;
  bool sent = false;
  while (!sent && tries > 0) {
    tries--;
    try
    {
      _rabbitMQHandler->publish(buffer, int(message_byte_length));
      sent = true;
    } 
    catch(exception& e)
    {
      errmsg_printf(error::ERROR, _(e.what()));
      try {
  	_rabbitMQHandler->reconnect();
      } catch(exception &e) {
  	errmsg_printf(error::ERROR, _("Could not reconnect, trying again.. - waiting 10 seconds for server to come back"));
  	sleep(10);
      } // 
    }
  }

  delete[] buffer;
  if(sent) return plugin::SUCCESS;
  errmsg_printf(error::ERROR, _("RabbitMQ server has disappeared, failing transaction."));
  deactivate();
  return plugin::UNKNOWN_ERROR;
}

static RabbitMQLog *rabbitmqLogger; ///< the actual plugin
static RabbitMQHandler* rabbitmqHandler; ///< the rabbitmq handler


/**
 * Initialize the rabbitmq logger - instanciates the dependencies (the handler)
 * and creates the log handler with the dependency - makes it easier to swap out
 * handler implementation
 */
static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();
  
  try 
  {
    rabbitmqHandler= new RabbitMQHandler(vm["host"].as<string>(),
                                         sysvar_rabbitmq_port, 
                                         vm["username"].as<string>(), 
                                         vm["password"].as<string>(), 
                                         vm["virtualhost"].as<string>(),
					 vm["exchange"].as<string>(),
					 vm["routingkey"].as<string>());
  } 
  catch (exception& e) 
  {
    errmsg_printf(error::ERROR, _("Failed to allocate the RabbitMQHandler.  Got error: %s\n"),
                  e.what());
    return 1;
  }
  try 
  {
    rabbitmqLogger= new RabbitMQLog("rabbit_log_applier",
                                    rabbitmqHandler);
  } 
  catch (exception& e) 
  {
    errmsg_printf(error::ERROR, _("Failed to allocate the RabbitMQLog instance.  Got error: %s\n"), 
                  e.what());
    return 1;
  }

  context.add(rabbitmqLogger);
  ReplicationServices::attachApplier(rabbitmqLogger, vm["use-replicator"].as<string>());

  context.registerVariable(new sys_var_const_string_val("host", vm["host"].as<string>()));
  context.registerVariable(new sys_var_constrained_value_readonly<in_port_t>("port", sysvar_rabbitmq_port));
  context.registerVariable(new sys_var_const_string_val("username", vm["username"].as<string>()));
  context.registerVariable(new sys_var_const_string_val("password", vm["password"].as<string>()));
  context.registerVariable(new sys_var_const_string_val("virtualhost", vm["virtualhost"].as<string>()));
  context.registerVariable(new sys_var_const_string_val("exchange", vm["exchange"].as<string>()));
  context.registerVariable(new sys_var_const_string_val("routingkey", vm["routingkey"].as<string>()));

  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("host", 
          po::value<string>()->default_value("localhost"),
          _("Host name to connect to"));
  context("port",
          po::value<port_constraint>(&sysvar_rabbitmq_port)->default_value(5672),
          _("Port to connect to"));
  context("virtualhost",
          po::value<string>()->default_value("/"),
          _("RabbitMQ virtualhost"));
  context("username",
          po::value<string>()->default_value("guest"),
          _("RabbitMQ username"));
  context("password",
          po::value<string>()->default_value("guest"),
          _("RabbitMQ password"));
  context("use-replicator",
          po::value<string>()->default_value("default_replicator"),
          _("Name of the replicator plugin to use (default='default_replicator')"));
  context("exchange",
          po::value<string>()->default_value("ReplicationExchange"),
          _("Name of RabbitMQ exchange to publish to"));
  context("routingkey",
          po::value<string>()->default_value("ReplicationRoutingKey"),
          _("Name of RabbitMQ routing key to use"));
}

} /* namespace drizzle_plugin */

DRIZZLE_PLUGIN(drizzle_plugin::init, NULL, drizzle_plugin::init_options);

