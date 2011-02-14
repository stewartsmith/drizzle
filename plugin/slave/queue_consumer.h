/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
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

#ifndef PLUGIN_SLAVE_QUEUE_CONSUMER_H
#define PLUGIN_SLAVE_QUEUE_CONSUMER_H

#include "plugin/slave/queue_thread.h"
#include "drizzled/session.h"

namespace drizzled
{
  namespace message
  {
    class Transaction;
  }
}

namespace slave
{

class QueueConsumer : public QueueThread
{
public:
  QueueConsumer() :
    _in_error_state(false),
    _check_interval(5)
  { }

  bool init();
  bool process();
  void shutdown();

  void setSleepInterval(uint32_t seconds)
  {
    _check_interval= seconds;
  }

  uint32_t getSleepInterval()
  {
    return _check_interval;
  }
  
  /**
   * Update applier status in state table.
   *
   * @param err_msg Error message string
   * @param status false = STOPPED, true = RUNNING
   */
  void setApplierState(const std::string &err_msg, bool status);

private:
  typedef std::vector<uint64_t> TrxIdList;

  drizzled::Session::shared_ptr _session;

  bool _in_error_state;
  std::string _error_message;

  /** Number of seconds to sleep between checking queue for messages */
  uint32_t _check_interval;

  /**
   * Create the schema and tables (if necessary) that will store
   * information about replication state of the applier thread.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool createApplierSchemaAndTables();

  bool getListOfCompletedTransactions(TrxIdList &list);

  bool getMessage(drizzled::message::Transaction &transaction,
                  std::string &commit_id,
                  uint64_t trx_id,
                  uint32_t segment_id);

  /**
   * Convert the given Transaction message into equivalent SQL.
   *
   * @param[in] transaction Transaction protobuf message to convert.
   * @param[in,out] aggregate_sql Buffer for total SQL for this transaction.
   * @param[in,out] segmented_sql Buffer for carried over segmented statements.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool convertToSQL(const drizzled::message::Transaction &transaction,
                    std::vector<std::string> &aggregate_sql,
                    std::vector<std::string> &segmented_sql);

  /**
   * Execute a batch of SQL statements.
   *
   * @param sql Batch of SQL statements to execute.
   * @param commit_id Commit ID value to store in state table.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool executeSQL(std::vector<std::string> &sql,
                  const std::string &commit_id);

  bool executeSQL(std::vector<std::string> &sql);
  
  /**
   * Remove messages for a given transaction from the queue.
   *
   * @param trx_id Transaction ID for the messages to remove.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool deleteFromQueue(uint64_t trx_id);

  /**
   * Determine if a Statement message is an end message.
   *
   * @retval true Is an end Statement message
   * @retval false Is NOT an end Statement message
   */
  bool isEndStatement(const drizzled::message::Statement &statement);
};

} /* namespace slave */

#endif /* PLUGIN_SLAVE_QUEUE_CONSUMER_H */
