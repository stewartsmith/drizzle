/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <drizzled/server_includes.h>
#include "rpl_mi.h"

#define DEFAULT_CONNECT_RETRY 60

// Defined in slave.cc
int init_intvar_from_file(int* var, IO_CACHE* f, int default_val);
int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
			  const char *default_val);
int init_floatvar_from_file(float* var, IO_CACHE* f, float default_val);

Master_info::Master_info()
  :Slave_reporting_capability("I/O"),
   connect_retry(DEFAULT_CONNECT_RETRY), heartbeat_period(0),
   received_heartbeats(0), inited(0),
   abort_slave(0), slave_running(0), slave_run_id(0)
{
  host[0] = 0; user[0] = 0; password[0] = 0;
  io_thd= NULL;
  port= DRIZZLE_PORT;
  fd= -1,

  memset(&file, 0, sizeof(file));
  pthread_mutex_init(&run_lock, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&data_lock, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&data_cond, NULL);
  pthread_cond_init(&start_cond, NULL);
  pthread_cond_init(&stop_cond, NULL);
}

Master_info::~Master_info()
{
  pthread_mutex_destroy(&run_lock);
  pthread_mutex_destroy(&data_lock);
  pthread_cond_destroy(&data_cond);
  pthread_cond_destroy(&start_cond);
  pthread_cond_destroy(&stop_cond);
}

bool Master_info::setPassword(const char *pword)
{
  password.assign(pword);

  return true;
}

const char *Master_info::getPassword()
{
  return password.c_str();
}

bool Master_info::setUsername(const char *username)
{
  user.assign(username);

  return true;
}

const char *Master_info::getUsername()
{
  return user.c_str();
}

bool Master_info::setHost(const char *hostname, uint16_t new_port)
{
  host.assign(hostname);
  port= new_port;

  return true;
}

const char *Master_info::getHostname()
{
  return host.c_str();
}

uint16_t Master_info::getPort()
{
  return port;
}

off_t Master_info::getLogPosition()
{
  return master_log_pos;
}

bool Master_info::setLogPosition(off_t position)
{
  master_log_pos= position;

  return true;
}

const char *Master_info::getLogName()
{
  return master_log_name.c_str();
}

bool Master_info::setLogName(const char *name)
{
  master_log_name.assign(name);

  return true;
}

uint32_t Master_info::getConnectionRetry()
{
  return connect_retry;
}

bool Master_info::setConnectionRetry(uint32_t retry)
{
  connect_retry= retry;

  return true;
}


void Master_info::init_master_log_pos()
{
  master_log_name.clear();
  master_log_pos = BIN_LOG_HEADER_SIZE;             // skip magic number
  /* 
    always request heartbeat unless master_heartbeat_period is set
    explicitly zero.  Here is the default value for heartbeat period
    if CHANGE MASTER did not specify it.  (no data loss in conversion
    as hb period has a max)
  */
  heartbeat_period= (float) cmin((double)SLAVE_MAX_HEARTBEAT_PERIOD,
                                 (slave_net_timeout/2.0));
  assert(heartbeat_period > (float) 0.001
         || heartbeat_period == 0);
  return;
}


int Master_info::init_master_info(const char* master_info_fname,
                                  const char* slave_info_fname,
                                  bool abort_if_no_master_info_file __attribute__((unused)),
                                  int thread_mask)
{
  int fd,error;
  char fname[FN_REFLEN+128];

  if (inited)
  {
    /*
      We have to reset read position of relay-log-bin as we may have
      already been reading from 'hotlog' when the slave was stopped
      last time. If this case pos_in_file would be set and we would
      get a crash when trying to read the signature for the binary
      relay log.

      We only rewind the read position if we are starting the SQL
      thread. The handle_slave_sql thread assumes that the read
      position is at the beginning of the file, and will read the
      "signature" and then fast-forward to the last position read.
    */
    if (thread_mask & SLAVE_SQL)
    {
      my_b_seek(rli.cur_log, (my_off_t) 0);
    }
    return(0);
  }

  drizzle= 0;
  file_id= 1;
  fn_format(fname, master_info_fname, mysql_data_home, "", 4+32);

  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */

  pthread_mutex_lock(&data_lock);
  fd= -1;

  /* does master.info exist ? */

  if (access(fname,F_OK))
  {
    /* Write new Master info file here (from fname) */
  }
  else // file exists
  {
    /* Read Master info file here (from fname) */
  }

  rli.mi = this;
  if (init_relay_log_info(&rli, slave_info_fname))
    goto err;

  inited = 1;
  // now change cache READ -> WRITE - must do this before flush_master_info
  reinit_io_cache(&file, WRITE_CACHE, 0L, 0, 1);
  if ((error= test(flush_master_info(1))))
    sql_print_error(_("Failed to flush master info file"));
  pthread_mutex_unlock(&data_lock);
  return(error);

err:
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&file);
  }
  fd= -1;
  pthread_mutex_unlock(&data_lock);
  return(1);
}


/*
  RETURN
     2 - flush relay log failed
     1 - flush master info failed
     0 - all ok
*/
int Master_info::flush_master_info(bool flush_relay_log_cache __attribute__((unused)))
{
  /*
    Flush the relay log to disk. If we don't do it, then the relay log while
    have some part (its last kilobytes) in memory only, so if the slave server
    dies now, with, say, from master's position 100 to 150 in memory only (not
    on disk), and with position 150 in master.info, then when the slave
    restarts, the I/O thread will fetch binlogs from 150, so in the relay log
    we will have "[0, 100] U [150, infinity[" and nobody will notice it, so the
    SQL thread will jump from 100 to 150, and replication will silently break.

    When we come to this place in code, relay log may or not be initialized;
    the caller is responsible for setting 'flush_relay_log_cache' accordingly.
  */

  return 0;
}


void Master_info::end_master_info()
{
  if (!inited)
    return;
  end_relay_log_info(&rli);
  if (fd >= 0)
  {
    end_io_cache(&file);
    (void)my_close(fd, MYF(MY_WME));
    fd = -1;
  }
  inited = 0;

  return;
}
