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

#include "config.h"
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

/**
 * The hostname to connect to
 */
static char* sysvar_rabbitmq_host= NULL;

/**
 * rabbitmq port
 */
static int sysvar_rabbitmq_port= 0;

/**
 * rabbitmq username
 */
static char* sysvar_rabbitmq_username= NULL;

/**
 * rabbitmq password
 */
static char* sysvar_rabbitmq_password= NULL;

/**
 * rabbitmq virtualhost
 */
static char* sysvar_rabbitmq_virtualhost= NULL;

/**
 * rabbitmq exchangename
 */
static char* sysvar_rabbitmq_exchange= NULL;

/**
 * rabbitmq routing key
 */
static char* sysvar_rabbitmq_routingkey= NULL;

/**
 * Is the rabbitmq log enabled?
 */
static bool sysvar_rabbitmq_log_enabled= false;

/**
 * The name of the replicator plugin
 * to pair the rabbitmq log's applier with.
 * Defaults to "default"
 */
static char *sysvar_rabbitmq_use_replicator= NULL;
static const char DEFAULT_USE_REPLICATOR[]= "default";


RabbitMQLog::RabbitMQLog(const string name_arg, 
			 RabbitMQHandler* mqHandler)
  :plugin::TransactionApplier(name_arg)
{
  rabbitMQHandler= mqHandler;
}

RabbitMQLog::~RabbitMQLog() 
{
}

plugin::ReplicationReturnCode
RabbitMQLog::apply(Session &, const message::Transaction &to_apply)
{
  size_t message_byte_length= to_apply.ByteSize();
  uint8_t* buffer= static_cast<uint8_t *>(malloc(message_byte_length));
  if(buffer == NULL)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate enough memory to transaction message\n"));
    deactivate();
    return plugin::UNKNOWN_ERROR;
  }

  to_apply.SerializeWithCachedSizesToArray(buffer);
  try
  {
    rabbitMQHandler->publish(buffer, 
			     int(message_byte_length), 
			     sysvar_rabbitmq_exchange, 
			     sysvar_rabbitmq_routingkey);
  }
  catch(exception& e)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _(e.what()));
    deactivate();
    return plugin::UNKNOWN_ERROR;
  }
  free(buffer);
  return plugin::SUCCESS;
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
  
  if (vm.count("username"))
  {
    sysvar_rabbitmq_username= const_cast<char *>(vm["username"].as<string>().c_str());
  }

  if (vm.count("password"))
  {
    sysvar_rabbitmq_password= const_cast<char *>(vm["password"].as<string>().c_str());
  }

  if (vm.count("host"))
  {
    sysvar_rabbitmq_host= const_cast<char *>(vm["host"].as<string>().c_str());
  }

  if(sysvar_rabbitmq_log_enabled)
  {
    try 
    {
      rabbitmqHandler= new RabbitMQHandler(sysvar_rabbitmq_host, 
					   sysvar_rabbitmq_port, 
					   sysvar_rabbitmq_username, 
					   sysvar_rabbitmq_password, 
					   sysvar_rabbitmq_virtualhost);
    } 
    catch (exception& e) 
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate the RabbitMQHandler.  Got error: %s\n"),
		    e.what());
      return 1;
    }
    try 
    {
      rabbitmqLogger= new RabbitMQLog("rabbit_log_applier", rabbitmqHandler);
    } 
    catch (exception& e) 
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to allocate the RabbitMQLog instance.  Got error: %s\n"), 
		    e.what());
      return 1;
    }

    context.add(rabbitmqLogger);
    ReplicationServices &replication_services= ReplicationServices::singleton();
    string replicator_name(sysvar_rabbitmq_use_replicator);
    replication_services.attachApplier(rabbitmqLogger, replicator_name);
    return 0;
  }
  return 0;
}


static DRIZZLE_SYSVAR_BOOL(enable,
                           sysvar_rabbitmq_log_enabled,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Enable rabbitmq log"),
                           NULL, /* check func */
                           NULL, /* update func */
                           false /* default */);


static DRIZZLE_SYSVAR_STR(hostname,
                          sysvar_rabbitmq_host,
                          PLUGIN_VAR_READONLY,
                          N_("Host name to connect to"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "localhost" /* default */);


static DRIZZLE_SYSVAR_INT(port,
			  sysvar_rabbitmq_port,
			  PLUGIN_VAR_READONLY,
			  N_("RabbitMQ Port"),
			  NULL, /* check func */
			  NULL, /* update func */
			  5672, /* default */
			  0,
			  65535,
			  0);

static DRIZZLE_SYSVAR_STR(username,
                          sysvar_rabbitmq_username,
                          PLUGIN_VAR_READONLY,
                          N_("RabbitMQ username"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "guest" /* default */);

static DRIZZLE_SYSVAR_STR(password,
                          sysvar_rabbitmq_password,
                          PLUGIN_VAR_READONLY,
                          N_("RabbitMQ password"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "guest" /* default */);

static DRIZZLE_SYSVAR_STR(virtualhost,
                          sysvar_rabbitmq_virtualhost,
                          PLUGIN_VAR_READONLY,
                          N_("RabbitMQ virtualhost"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "/" /* default */);

static DRIZZLE_SYSVAR_STR(exchange,
                          sysvar_rabbitmq_exchange,
                          PLUGIN_VAR_READONLY,
                          N_("Name of RabbitMQ exchange to publish to"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "ReplicationExchange" /* default */);

static DRIZZLE_SYSVAR_STR(routingkey,
                          sysvar_rabbitmq_routingkey,
                          PLUGIN_VAR_READONLY,
                          N_("Name of RabbitMQ routing key to use"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "ReplicationRoutingKey" /* default */);

static DRIZZLE_SYSVAR_STR(use_replicator,
                          sysvar_rabbitmq_use_replicator,
                          PLUGIN_VAR_READONLY,
                          N_("Name of the replicator plugin to use (default='default_replicator')"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          DEFAULT_USE_REPLICATOR /* default */);

static void init_options(drizzled::module::option_context &context)
{
  context ("enable",
           po::value<bool>(&sysvar_rabbitmq_log_enabled)->default_value(false)->zero_tokens(),
           N_("Enable rabbitmq log"));
  context("host", 
          po::value<string>()->default_value("localhost"),
          N_("Host name to connect to"));
  context("username",
          po::value<string>()->default_value("guest"),
          N_("RabbitMQ username"));
  context("password",
          po::value<string>()->default_value("guest"),
          N_("RabbitMQ password"));
}

static drizzle_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(hostname),
  DRIZZLE_SYSVAR(port),
  DRIZZLE_SYSVAR(username),
  DRIZZLE_SYSVAR(password),
  DRIZZLE_SYSVAR(virtualhost),
  DRIZZLE_SYSVAR(exchange),
  DRIZZLE_SYSVAR(routingkey),
  DRIZZLE_SYSVAR(use_replicator),
  NULL
};

DRIZZLE_PLUGIN(init, system_variables, init_options);

