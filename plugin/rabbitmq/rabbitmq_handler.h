/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 */

#ifndef PLUGIN_RABBITMQ_HANDLER_RABBITMQ_HANDLER_H
#define PLUGIN_RABBITMQ_HANDLER_RABBITMQ_HANDLER_H
#include <string>
#include <amqp.h>
#include <amqp_framing.h>

/**
 * exception thrown by the rabbitmq handler
 *
 */
class rabbitmq_handler_exception: public std::exception
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
  const char* hostname;
  const int port;
  const char* username;
  const char* password;
  const char* virtualhost;
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
  RabbitMQHandler(const char* hostname, 
		  const int port, 
		  const char* username, 
		  const char* password, 
		  const char* virtualhost) throw(rabbitmq_handler_exception);
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
   * @param[in] exchangeName name of the exchange to publish to
   * @param[in] routingKey the routing key to use
   * @throw exception if there is a problem publishing
   */
  void publish(const uint8_t *message, 
	       const int length, 
	       const char* exchangeName, 
	       const char* routingKey) throw(rabbitmq_handler_exception);


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
};

#endif
