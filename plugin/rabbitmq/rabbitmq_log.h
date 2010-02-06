#ifndef PLUGIN_RABBITMQ_LOG_RABBITMQ_LOG_H
#define PLUGIN_RABBITMQ_LOG_RABBITMQ_LOG_H
#include <drizzled/replication_services.h>
#include <drizzled/plugin/transaction_applier.h>
#include <string>
#include "rabbitmq_handler.h"


/**
 * @brief
 *   A TransactionApplier that sends the transactions to rabbitmq
 *   (or any AMQP 0-8 compliant message queue)
 *
 * @details
 *   Connects to rabbitmq server in constructor, publishes messages
 *   in apply(...). If error occurs, the plugin disables itself.
 */
class RabbitMQLog: public drizzled::plugin::TransactionApplier 
{
private:
  RabbitMQHandler* rabbitMQHandler;
public:

  /**
   * @brief
   *   Constructs a new RabbitMQLog.
   *
   * @details
   *  Takes an instance of RabbitMHandler and uses that for rabbitmq communication
   *
   * @param[in] mqHandler name of the plugin, typically rabbitmq_log.
   */
  RabbitMQLog(const std::string name_arg, 
	      RabbitMQHandler* mqHandler);
  ~RabbitMQLog();

  /**
   * @brief
   *   Serializes the transaction and uses a RabbiMQHandler to publish the message.
   *
   * @details
   *   Serializes the protobuf transaction and drops it on rabbitmq
   *
   * @param[in] to_apply the transaction to send
   */
  void apply(const drizzled::message::Transaction &to_apply);

};

#endif
