#include "rabbitmq_handler.h"

using namespace std;

RabbitMQHandler::RabbitMQHandler(const char* rabbitMQHost, 
				 const int rabbitMQPort, 
				 const char* rabbitMQUsername, 
				 const char* rabbitMQPassword, 
				 const char* rabbitMQVirtualhost) 
  throw(rabbitmq_handler_exception):
  hostname(rabbitMQHost),
  port(rabbitMQPort),
  username(rabbitMQUsername),
  password(rabbitMQPassword),
  virtualhost(rabbitMQVirtualhost)
{
  rabbitmqConnection = amqp_new_connection();
  /* open the socket to the rabbitmq server */
  sockfd = amqp_open_socket(hostname, port);
  if(sockfd < 0) 
  {
    throw rabbitmq_handler_exception("Could not open socket, is rabbitmq running?");
  }
  amqp_set_sockfd(rabbitmqConnection, sockfd);
  /* login to rabbitmq, handleAMQPError throws exception if there is a problem */
  handleAMQPError(amqp_login(rabbitmqConnection, 
			     virtualhost, 
			     0, 
			     131072, 
			     0, 
			     AMQP_SASL_METHOD_PLAIN, 
			     username, 
			     password), 
		  "rabbitmq login");
  /* open the channel */
  amqp_channel_open(rabbitmqConnection, 1);
}

RabbitMQHandler::~RabbitMQHandler()
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

void RabbitMQHandler::publish(const uint8_t *message, 
			      const int length, 
			      const char* exchangeName, 
			      const char* routingKey) throw(rabbitmq_handler_exception)
{
  amqp_bytes_t b;
  b.bytes= (void*)message;
  b.len= length;
  
  if(amqp_basic_publish(rabbitmqConnection,
			1,
			amqp_cstring_bytes(exchangeName),
			amqp_cstring_bytes(routingKey),
			0,
			0,
			NULL,
			b) < 0)
  {
    throw rabbitmq_handler_exception("Could not publish message");
  }

}

void RabbitMQHandler::handleAMQPError(amqp_rpc_reply_t x, string context) throw(rabbitmq_handler_exception)
{
  string errorMessage = "";
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
