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

#pragma once

#include <exception>
#include <string>
#include <amqp.h>
#include <amqp_framing.h>
#include <netinet/in.h>

namespace drizzle_plugin
{

/**
 * exception thrown by the rabbitmq handler
 *
 */
class rabbitmq_handler_exception : public std::exception
{
private:
  const char* message;
public:
  rabbitmq_handler_exception(const char* m):message(m) {};
  rabbitmq_handler_exception(std::string m):message(m.c_str()) {};
  virtual const char* what() const throw()
  {
    return message;
  }
};


/**
 * @brief wrapper around librabbitmq, hides error handling and reconnections etc
 * TODO: add reconnection handling
 */
class RabbitMQHandler
{
private:
  amqp_connection_state_t rabbitmqConnection; 
  int sockfd; ///< the socket file desc to the rabbitmq server, 
              ///< need this to be able to close() it.
  const std::string &hostname;
  const in_port_t port;
  const std::string &username;
  const std::string &password;
  const std::string &virtualhost;
  const std::string &exchange;
  const std::string &routingKey;
  pthread_mutex_t publishLock;
public:
  /**
   * @brief
   *   Constructs a new RabbitMQHandler, purpose is to 
   *   hide away the error handling, reconnections etc.
   *
   * @details
   *   Connects to the given rabbitmq server on the virtualhost
   *   with the given username/password. 
   *
   * @param[in] hostname the host to connect to.
   * @param[in] port the port.
   * @param[in] username the username to use when logging in.
   * @param[in] password the password to use.
   * @param[in] virtualhost the rabbitmq virtual host.
   * @throw exception if we cannot connect to rabbitmq server
   */
  RabbitMQHandler(const std::string &hostname, 
                  const in_port_t port, 
                  const std::string &username, 
                  const std::string &password, 
                  const std::string &virtualhost,
		  const std::string &exchange,
		  const std::string &routingKey)
    throw(rabbitmq_handler_exception);

  ~RabbitMQHandler();

  /**
   * @brief
   *   Publishes the message to the server
   *
   * @details
   *   publishes the given message
   *
   * @param[in] message the message to send
   * @param[in] length the length of the message
   * @throw exception if there is a problem publishing
   */
  void publish(void *message, 
               const int length)
    throw(rabbitmq_handler_exception);

  void reconnect() throw(rabbitmq_handler_exception);
  void disconnect() throw(rabbitmq_handler_exception);

private:
  /**
   * @brief
   *   Handles errors produced by librabbitmq
   *
   * @details
   *   If an error occurs, an error string is thrown.
   *
   * @param[in] x the response from librabbitmq
   * @param[in] context the context the call occured, simply appended to the error message.
   *
   * @throw exception with the message unless the command was successful
   */
  void handleAMQPError(amqp_rpc_reply_t x, std::string context) throw(rabbitmq_handler_exception);

  void connect() throw(rabbitmq_handler_exception);

};

} /* namespace drizzle_plugin */

