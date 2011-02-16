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

#ifndef PLUGIN_SLAVE_QUEUE_PRODUCER_H
#define PLUGIN_SLAVE_QUEUE_PRODUCER_H

#include "plugin/slave/queue_thread.h"
#include "plugin/slave/sql_executor.h"
#include "client/client_priv.h"
#include <string>
#include <vector>

namespace slave
{
  
class QueueProducer : public QueueThread, public SQLExecutor
{
public:
  QueueProducer() :
    SQLExecutor("slave", "replication"),
    _check_interval(5),
    _master_port(3306),
    _last_return(DRIZZLE_RETURN_OK),
    _is_connected(false),
    _saved_max_commit_id(0),
    _max_reconnects(10),
    _seconds_between_reconnects(30)
  {}

  virtual ~QueueProducer();

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

  void setMasterHost(const std::string &host)
  {
    _master_host= host;
  }

  void setMasterPort(uint16_t port)
  {
    _master_port= port;
  }

  void setMasterUser(const std::string &user)
  {
    _master_user= user;
  }

  void setMasterPassword(const std::string &password)
  {
    _master_pass= password;
  }

  void setMaxReconnectAttempts(uint32_t max)
  {
    _max_reconnects= max;
  }

  void setSecondsBetweenReconnects(uint32_t seconds)
  {
    _seconds_between_reconnects= seconds;
  }

private:
  /** Number of seconds to sleep between checking queue for messages */
  uint32_t _check_interval;

  /* Master server connection parameters */
  std::string _master_host;
  uint16_t    _master_port;
  std::string _master_user;
  std::string _master_pass;

  drizzle_st _drizzle;
  drizzle_con_st _connection;
  drizzle_return_t _last_return;

  bool _is_connected;
  uint32_t _saved_max_commit_id;
  uint32_t _max_reconnects;
  uint32_t _seconds_between_reconnects;

  std::string _last_error_message;

  /**
   * Open connection to the master server.
   */
  bool openConnection();

  /**
   * Close connection to the master server.
   */
  bool closeConnection();

  /**
   * Attempt to reconnect to the master server.
   *
   * This method does not return until reconnect succeeds, or we exceed our
   * maximum number of retries defined by _max_reconnects.
   *
   * @retval true Reconnect succeeded
   * @retval false Reconnect failed
   */
  bool reconnect();

  bool queryForMaxCommitId(uint32_t *max_commit_id);
  bool queryForReplicationEvents(uint32_t max_commit_id);
  bool queryForTrxIdList(uint32_t max_commit_id, std::vector<uint64_t> &list);
  bool queueInsert(const char *trx_id,
                   const char *seg_id,
                   const char *commit_id,
                   const char *msg,
                   const char *msg_length);

  /**
   * Update IO thread status in state table.
   *
   * @param err_msg Error message string
   * @param status false = STOPPED, true = RUNNING
   */
  void setIOState(const std::string &err_msg, bool status);

};

} /* namespace slave */

#endif /* PLUGIN_SLAVE_QUEUE_PRODUCER_H */
