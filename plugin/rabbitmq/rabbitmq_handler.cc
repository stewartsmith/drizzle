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

#include <drizzled/gettext.h>

#include "rabbitmq_handler.h"

using namespace std;

namespace drizzle_plugin
{

RabbitMQHandler::RabbitMQHandler(const std::string &rabbitMQHost, 
                                 const in_port_t rabbitMQPort, 
                                 const std::string &rabbitMQUsername, 
                                 const std::string &rabbitMQPassword, 
                                 const std::string &rabbitMQVirtualhost, 
				 const std::string &rabbitMQExchange, 
				 const std::string &rabbitMQRoutingKey) 
  throw(rabbitmq_handler_exception) :
    rabbitmqConnection(amqp_new_connection()),
    hostname(rabbitMQHost),
    port(rabbitMQPort),
    username(rabbitMQUsername),
    password(rabbitMQPassword),
    virtualhost(rabbitMQVirtualhost),
    exchange(rabbitMQExchange), 
    routingKey(rabbitMQRoutingKey)
{
  pthread_mutex_init(&publishLock, NULL);
  connect();
}

RabbitMQHandler::~RabbitMQHandler()
{
  pthread_mutex_destroy(&publishLock);
  disconnect();
}

void RabbitMQHandler::publish(void *message, 
                              const int length)
throw(rabbitmq_handler_exception)
{
  pthread_mutex_lock(&publishLock);
  amqp_bytes_t b;
  b.bytes= message;
  b.len= length;
  
  if (amqp_basic_publish(rabbitmqConnection,
                         1,
                         amqp_cstring_bytes(exchange.c_str()),
                         amqp_cstring_bytes(routingKey.c_str()),
                         0,
                         0,
                         NULL,
                         b) < 0)
  {
    pthread_mutex_unlock(&publishLock);
    throw rabbitmq_handler_exception("Could not publish message");
  }
  pthread_mutex_unlock(&publishLock);

}

void RabbitMQHandler::reconnect() throw(rabbitmq_handler_exception)
{
  disconnect();
  connect();
}

void RabbitMQHandler::disconnect() throw(rabbitmq_handler_exception) 
{
  try
  {
    handleAMQPError(amqp_channel_close(rabbitmqConnection, 
				       1, 
				       AMQP_REPLY_SUCCESS),
		    "close channel");
    handleAMQPError(amqp_connection_close(rabbitmqConnection, 
					  AMQP_REPLY_SUCCESS),
		    "close connection");
    amqp_destroy_connection(rabbitmqConnection);
  }
  catch(exception& e) {} // do not throw in destructorn 
  close(sockfd);
}

void RabbitMQHandler::connect() throw(rabbitmq_handler_exception) {
  sockfd = amqp_open_socket(hostname.c_str(), port);
  if(sockfd < 0) 
  {
    throw rabbitmq_handler_exception(_("Could not open socket, is rabbitmq running?"));
  }
  amqp_set_sockfd(rabbitmqConnection, sockfd);
  /* login to rabbitmq, handleAMQPError throws exception if there is a problem */
  handleAMQPError(amqp_login(rabbitmqConnection, 
                             virtualhost.c_str(), 
                             0, 
                             131072, 
                             0, 
                             AMQP_SASL_METHOD_PLAIN, 
                             username.c_str(), 
                             password.c_str()), 
                  "rabbitmq login");
  /* open the channel */
  amqp_channel_open(rabbitmqConnection, 1);
  handleAMQPError(amqp_get_rpc_reply(rabbitmqConnection), "RPC Reply");
  amqp_table_t empty_table = { 0, NULL }; // for users of old librabbitmq users - amqp_empty_table did not exist
  amqp_exchange_declare(rabbitmqConnection, 1, amqp_cstring_bytes(exchange.c_str()), amqp_cstring_bytes("fanout"), 0, 0, empty_table);
  handleAMQPError(amqp_get_rpc_reply(rabbitmqConnection), "RPC Reply");
}

void RabbitMQHandler::handleAMQPError(amqp_rpc_reply_t x, string context) throw(rabbitmq_handler_exception)
{
  string errorMessage("");
  switch (x.reply_type) {
  case AMQP_RESPONSE_NORMAL:
    break;
  case AMQP_RESPONSE_NONE:
    errorMessage.assign("No response in ");
    errorMessage.append(context);
    throw rabbitmq_handler_exception(errorMessage);
  case AMQP_RESPONSE_LIBRARY_EXCEPTION:
  case AMQP_RESPONSE_SERVER_EXCEPTION:
    switch (x.reply.id) {      
    case AMQP_CONNECTION_CLOSE_METHOD:
      errorMessage.assign("Connection closed in ");
      errorMessage.append(context);
      throw rabbitmq_handler_exception(errorMessage);
    case AMQP_CHANNEL_CLOSE_METHOD:
      errorMessage.assign("Channel closed in ");
      errorMessage.append(context);
      throw rabbitmq_handler_exception(errorMessage);
    default:
      errorMessage.assign("Unknown error in ");
      errorMessage.append(context);
      throw rabbitmq_handler_exception(errorMessage);
    }
  }
}

} /* namespace drizzle_plugin */
