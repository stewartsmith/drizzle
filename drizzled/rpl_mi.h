/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_RPL_MI_H
#define DRIZZLED_RPL_MI_H

#include "rpl_rli.h"
#include "rpl_reporting.h"
#include <drizzled/serialize/serialize.h>
#include <string>


/*****************************************************************************

  Replication IO Thread

  Master_info contains:
    - information about how to connect to a master
    - current master log name
    - current master log offset
    - misc control variables

  Master_info is initialized once from the master.info file if such
  exists. Otherwise, data members corresponding to master.info fields
  are initialized with defaults specified by master-* options. The
  initialization is done through init_master_info() call.

  The format of master.info file:

  log_name
  log_pos
  master_host
  master_user
  master_pass
  master_port
  master_connect_retry

  To write out the contents of master.info file to disk ( needed every
  time we read and queue data from the master ), a call to
  flush_master_info() is required.

  To clean up, call end_master_info()

*****************************************************************************/

class Master_info : public Slave_reporting_capability
{
private:
  drizzle::MasterList list;

public:

  /* the variables below are needed because we can change masters on the fly */
  string master_log_name;
  string host;
  string user;
  string password;
  uint16_t port;
  uint32_t connect_retry;
  off_t master_log_pos;
  THD *io_thd;
  Master_info();
  ~Master_info();

  File fd; // we keep the file open, so we need to remember the file pointer
  IO_CACHE file;

  pthread_mutex_t data_lock,run_lock;
  pthread_cond_t data_cond,start_cond,stop_cond;
  DRIZZLE *drizzle;
  uint32_t file_id;				/* for 3.23 load data infile */
  Relay_log_info rli;
  float heartbeat_period;         // interface with CHANGE MASTER or master.info
  uint64_t received_heartbeats;  // counter of received heartbeat events
  int events_till_disconnect;
  bool inited;
  volatile bool abort_slave;
  volatile uint32_t slave_running;
  volatile uint32_t slave_run_id;
  /*
     The difference in seconds between the clock of the master and the clock of
     the slave (second - first). It must be signed as it may be <0 or >0.
     clock_diff_with_master is computed when the I/O thread starts; for this the
     I/O thread does a SELECT UNIX_TIMESTAMP() on the master.
     "how late the slave is compared to the master" is computed like this:
     clock_of_slave - last_timestamp_executed_by_SQL_thread - clock_diff_with_master

  */
  long clock_diff_with_master;
  int flush_master_info(bool flush_relay_log_cache);
  void end_master_info();
  void init_master_log_pos();

  int init_master_info(const char* master_info_fname,
                       const char* slave_info_fname,
                       bool abort_if_no_master_info_file,
                       int thread_mask);

  bool setUsername(const char *username);
  const char *getUsername();

  bool setPassword(const char *pword);
  const char *getPassword();

  bool setHost(const char *host, uint16_t new_port);
  const char *getHostname();
  uint16_t getPort();

  off_t getLogPosition();
  bool setLogPosition(off_t position);

  const char *getLogName();
  bool setLogName(const char *name);

  uint32_t getConnectionRetry();
  bool setConnectionRetry(uint32_t log_position);

};

#endif /* DRIZZLED_RPL_MI_H */
