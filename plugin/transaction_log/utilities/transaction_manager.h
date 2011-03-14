/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 David Shrewsbury <shrewsbury.dave@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

/**
 * @file
 * Declaration of a class used to manage Transaction messages so that they
 * can be retrieved as a collection.
 */

#pragma once

#include <drizzled/message/transaction.pb.h>
#include <string>
#include <vector>
#include <boost/unordered_map.hpp>

typedef std::vector<std::string> MsgBufferType;


/**
 * Simple (example) Transaction message buffer and content manager.
 *
 * @details
 * This class groups Transaction messages together by transaction ID and
 * buffers them together in memory. Obviously, this could eat up a lot of
 * memory if you have very large transactions. A more robust implementation
 * would buffer larger transactions to disk rather than memory.
 *
 * @note
 * Once you have a complete transaction and have processed it, you should
 * call remove() to remove the cache contents for that transaction from memory.
 */
class TransactionManager
{
public:
  /**
   * Store the given Transaction message in a buffer.
   *
   * @param[in] transaction Pointer to the Transaction message to store.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool store(const drizzled::message::Transaction &transaction);

  /**
   * Clear the buffer contents for a given transaction ID.
   *
   * @param[in] trx_id The transaction ID for the transaction to remove
   *
   * @retval true Success
   * @retval false Failure
   */
  bool remove(uint64_t trx_id);

  /**
   * Check to see if any Transaction messages exist for a given transaction.
   *
   * @param[in] trx_id The transaction ID to check for.
   *
   * @retval true Transaction messages exist
   * @retval false No Transaction messages found
   */
  bool contains(uint64_t trx_id);

  /**
   * Return number of cached elements for the given transaction ID.
   *
   * @param[in] trx_id Transaction ID
   *
   * @returns The number of cached elements associated with trx_id.
   */
  uint32_t getTransactionBufferSize(uint64_t trx_id);

  /**
   * Retrieve a Transaction message from the managed cache.
   *
   * Caller must supply a Transaction message to populate. The Transaction
   * message to retrieve is indexed by a combination of transaction ID and
   * position.
   *
   * @param[out] transaction Transaction message to populate
   * @param[in] trx_id Transaction ID
   * @param[in] position Index into the buffer associated with trx_id
   *
   * @retval true Success
   * @retval false Failure
   */
  bool getTransactionMessage(drizzled::message::Transaction &transaction,
                             uint64_t trx_id,
                             uint32_t position);

private:
  /**
   * Our message buffer cache, mapped by the transaction ID.
   *
   * We organize Transactions messages by grouping them by transaction ID,
   * then storing the messages in std::vectors in std::string format. The
   * string format is convenient because it can be easily copied around
   * (GPB messages do not provide deep copying).
   */
  boost::unordered_map<uint64_t,MsgBufferType> cache;
};

