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


#pragma once

#include <drizzled/replication_services.h>
#include <drizzled/plugin/transaction_applier.h>
#include <string>
#include <zmq.h>

namespace drizzle_plugin {
namespace zeromq {

/**
 * @brief
 *   A TransactionApplier that publishes transaction on a zeromq pub socket
 *
 * @details
 * sends messages to a pub socket
 */
class ZeroMQLog :
  public drizzled::plugin::TransactionApplier 
{
private:
  void *_socket;
  pthread_mutex_t publishLock;
  std::string sysvar_endpoint;
  std::string getSchemaName(const drizzled::message::Transaction &txn);
public:

  /**
   * @brief
   *   Constructs a new ZeroMQLog.
   *
   * @details
   *
   * @param[in] name of plugin
   * @param[in] the endpoint string (typically tcp:// *:9999) (no space actually, compiler whines about comment within comment..)
  */
  ZeroMQLog(const std::string &name, const std::string &endpoint);
  ~ZeroMQLog();
  
  /**
   * @brief
   *   Getter for endpoint
   * 
   * @details
   *   Returns value of sysvar_endpoint
   */
  std::string& getEndpoint();

  /**
   * @brief
   *   Setter for endpoint
   *
   * @details
   *   This function is called to change the value of sysvar_endpoint
   *
   * @param[in] new endpoint string
   */
  bool setEndpoint(std::string new_endpoint);

  /**
   * @brief
   *   Serializes the transaction and publishes the message to zmq
   *
   * @details
   *   Serializes the protobuf transaction and drops it on zmq
   *
   * @param[in] to_apply the transaction to send
   */
  drizzled::plugin::ReplicationReturnCode
  apply(drizzled::Session &session, const drizzled::message::Transaction &to_apply);

};

} /* namespace zeromq */
} /* namespace drizzle_plugin */

