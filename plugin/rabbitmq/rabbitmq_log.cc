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
#include <drizzled/item.h>

namespace po= boost::program_options;

using namespace std;
using namespace drizzled;
using namespace google;

namespace drizzle_plugin {
namespace rabbitmq {

/**
 * rabbitmq port
 */
static port_constraint sysvar_rabbitmq_port;
bool sysvar_logging_enable= true;
string sysvar_rabbitmq_host;
string sysvar_rabbitmq_username;
string sysvar_rabbitmq_password;
string sysvar_rabbitmq_virtualhost;
string sysvar_rabbitmq_exchange;
string sysvar_rabbitmq_routingkey;
void updateSysvarLoggingEnable(Session *, sql_var_t);
bool updateSysvarRabbitMQHost(Session *, set_var *var);
int updateSysvarRabbitMQPort(Session *, set_var *var);
bool updateSysvarRabbitMQUserName(Session *, set_var *var);
bool updateSysvarRabbitMQPassword(Session *, set_var *var);
bool updateSysvarRabbitMQVirtualHost(Session *, set_var *var);
bool updateSysvarRabbitMQExchange(Session *, set_var *var);
bool updateSysvarRabbitMQRoutingKey(Session *, set_var *var);

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
  if(not sysvar_logging_enable)
    return plugin::SUCCESS;

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
      errmsg_printf(error::ERROR, "%s", e.what());
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

void RabbitMQLog::setRabbitMQHandler(RabbitMQHandler* new_rabbitMQHandler)
{
  _rabbitMQHandler= new_rabbitMQHandler;
}

static RabbitMQLog *rabbitmqLogger; ///< the actual plugin
static RabbitMQHandler* rabbitmqHandler; ///< the rabbitmq handler


void updateSysvarLoggingEnable(Session *, sql_var_t)
{
  if(not sysvar_logging_enable)
  {
    sysvar_logging_enable = false;
    delete rabbitmqHandler;
  }
  else
  {
    rabbitmqHandler= new RabbitMQHandler(sysvar_rabbitmq_host,
                                         sysvar_rabbitmq_port,
                                         sysvar_rabbitmq_username,
                                         sysvar_rabbitmq_password,
                                         sysvar_rabbitmq_virtualhost,
                                         sysvar_rabbitmq_exchange,
                                         sysvar_rabbitmq_routingkey);
    if(rabbitmqHandler->rabbitmq_connection_established)
    {
      rabbitmqLogger->setRabbitMQHandler(rabbitmqHandler);
      sysvar_logging_enable= true;
    }
    else
    {
      errmsg_printf(error::ERROR, _("Could not open socket, is rabbitmq running?"));
      sysvar_logging_enable= false;
    }
  }
}

bool updateSysvarRabbitMQHost(Session *, set_var *var)
{
  if(sysvar_logging_enable)
  {
    errmsg_printf(error::ERROR, _("Value of rabbitmq_host cannot be changed as rabbitmq plugin is enabled. You need to disable the plugin first."));
    return true; // error
  }
  if (not var->value->str_value.empty())
  {
    sysvar_rabbitmq_host = var->value->str_value.data();
    return false;
  }
  else
  {
    errmsg_printf(error::ERROR, _("rabbitmq_host cannot be NULL"));
    return true; // error
  }
  return true; // error
}

int updateSysvarRabbitMQPort(Session *, set_var *var)
{
  if(sysvar_logging_enable)
  {
    errmsg_printf(error::ERROR, _("Value of rabbitmq_port cannot be changed as rabbitmq plugin is enabled. You need to disable the plugin first."));
    return 1; // error
  }
  if (var->value->val_int())
  {
    sysvar_rabbitmq_port = var->value->val_int();
    return 0;
  }
  else
  {
    errmsg_printf(error::ERROR, _("rabbitmq_port cannot be NULL"));
    return 1; // error
  }
  return 1; // error
}

bool updateSysvarRabbitMQUserName(Session *, set_var *var)
{
  if(sysvar_logging_enable)
  {
    errmsg_printf(error::ERROR, _("Value of rabbitmq_username cannot be changed as rabbitmq plugin is enabled. You need to disable the plugin first."));
    return true; // error
  }
  if (not var->value->str_value.empty())
  {
    sysvar_rabbitmq_username = var->value->str_value.data();
    return false;
  }
  else
  {
    errmsg_printf(error::ERROR, _("rabbitmq_username cannot be NULL"));
    return true; // error
  }
  return true; // error
}

bool updateSysvarRabbitMQPassword(Session *, set_var *var)
{
  if(sysvar_logging_enable)
  {
    errmsg_printf(error::ERROR, _("Value of rabbitmq_password cannot be changed as rabbitmq plugin is enabled. You need to disable the plugin first."));
    return true; // error
  }
  if (not var->value->str_value.empty())
  {
    sysvar_rabbitmq_password = var->value->str_value.data();
    return false;
  }
  else
  {
    errmsg_printf(error::ERROR, _("rabbitmq_password cannot be NULL"));
    return true; // error
  }
  return true; // error
}

bool updateSysvarRabbitMQVirtualHost(Session *, set_var *var)
{
  if(sysvar_logging_enable)
  {
    errmsg_printf(error::ERROR, _("Value of rabbitmq_virtualhost cannot be changed as rabbitmq plugin is enabled. You need to disable the plugin first."));
    return true; // error
  }
  if (not var->value->str_value.empty())
  {
    sysvar_rabbitmq_virtualhost = var->value->str_value.data();
    return false;
  }
  else
  {
    errmsg_printf(error::ERROR, _("rabbitmq_virtualhost cannot be NULL"));
    return true; // error
  }
  return true; // error
}

bool updateSysvarRabbitMQExchange(Session *, set_var *var)
{
  if(sysvar_logging_enable)
  {
    errmsg_printf(error::ERROR, _("Value of rabbitmq_exchange cannot be changed as rabbitmq plugin is enabled. You need to disable the plugin first."));
    return true; // error
  }
  if (not var->value->str_value.empty())
  {
    sysvar_rabbitmq_exchange = var->value->str_value.data();
    return false;
  }
  else
  {
    errmsg_printf(error::ERROR, _("rabbitmq_exchange cannot be NULL"));
    return true; // error
  }
  return true; // error
}

bool updateSysvarRabbitMQRoutingKey(Session *, set_var *var)
{
  if(sysvar_logging_enable)
  {
    errmsg_printf(error::ERROR, _("Value of rabbitmq_routingkey cannot be changed as rabbitmq plugin is enabled. You need to disable the plugin first."));
    return true; // error
  }
  if (not var->value->str_value.empty())
  {
    sysvar_rabbitmq_routingkey = var->value->str_value.data();
    return false;
  }
  else
  {
    errmsg_printf(error::ERROR, _("rabbitmq_routingkey cannot be NULL"));
    return true; // error
  }
  return true; // error
}

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
    if(not rabbitmqHandler->rabbitmq_connection_established)
    {
      throw rabbitmq_handler_exception(_("Could not open socket, is rabbitmq running?"));
    }
  } 
  catch (exception& e) 
  {
    errmsg_printf(error::ERROR, _("Failed to allocate the RabbitMQHandler.  Got error: %s\n"),
                  e.what());
    return 1;
  }
  try 
  {
    rabbitmqLogger= new RabbitMQLog("rabbitmq_applier", rabbitmqHandler);
  } 
  catch (exception& e) 
  {
    errmsg_printf(error::ERROR, _("Failed to allocate the RabbitMQLog instance.  Got error: %s\n"), 
                  e.what());
    return 1;
  }

  context.add(rabbitmqLogger);
  ReplicationServices::attachApplier(rabbitmqLogger, vm["use-replicator"].as<string>());

  context.registerVariable(new sys_var_bool_ptr("logging_enable", &sysvar_logging_enable, &updateSysvarLoggingEnable));
  context.registerVariable(new sys_var_std_string("host", sysvar_rabbitmq_host, NULL, &updateSysvarRabbitMQHost));
  context.registerVariable(new sys_var_constrained_value<in_port_t>("port", sysvar_rabbitmq_port, &updateSysvarRabbitMQPort));
  context.registerVariable(new sys_var_std_string("username", sysvar_rabbitmq_username, NULL, &updateSysvarRabbitMQUserName));
  context.registerVariable(new sys_var_std_string("password", sysvar_rabbitmq_password, NULL, &updateSysvarRabbitMQPassword));
  context.registerVariable(new sys_var_std_string("virtualhost", sysvar_rabbitmq_virtualhost, NULL, &updateSysvarRabbitMQVirtualHost));
  context.registerVariable(new sys_var_std_string("exchange", sysvar_rabbitmq_exchange, NULL, &updateSysvarRabbitMQExchange));
  context.registerVariable(new sys_var_std_string("routingkey", sysvar_rabbitmq_routingkey, NULL, &updateSysvarRabbitMQRoutingKey));

  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("logging-enable",
          po::value<bool>(&sysvar_logging_enable)->default_value(true)->zero_tokens(),
          _("Enable logging to rabbitmq server"));
  context("host", 
          po::value<string>(&sysvar_rabbitmq_host)->default_value("localhost"),
          _("Host name to connect to"));
  context("port",
          po::value<port_constraint>(&sysvar_rabbitmq_port)->default_value(5672),
          _("Port to connect to"));
  context("virtualhost",
          po::value<string>(&sysvar_rabbitmq_virtualhost)->default_value("/"),
          _("RabbitMQ virtualhost"));
  context("username",
          po::value<string>(&sysvar_rabbitmq_username)->default_value("guest"),
          _("RabbitMQ username"));
  context("password",
          po::value<string>(&sysvar_rabbitmq_password)->default_value("guest"),
          _("RabbitMQ password"));
  context("use-replicator",
          po::value<string>()->default_value("default_replicator"),
          _("Name of the replicator plugin to use (default='default_replicator')"));
  context("exchange",
          po::value<string>(&sysvar_rabbitmq_exchange)->default_value("ReplicationExchange"),
          _("Name of RabbitMQ exchange to publish to"));
  context("routingkey",
          po::value<string>(&sysvar_rabbitmq_routingkey)->default_value("ReplicationRoutingKey"),
          _("Name of RabbitMQ routing key to use"));
}

} /* namespace rabbitmq */
} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "rabbitmq",
  "0.1",
  "Marcus Eriksson",
  N_("Publishes transactions to RabbitMQ"),
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::rabbitmq::init,
  NULL,
  drizzle_plugin::rabbitmq::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
