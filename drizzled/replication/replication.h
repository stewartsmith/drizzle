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

#ifndef DRIZZLED_REPLICATION_REPLICATION_H
#define DRIZZLED_REPLICATION_REPLICATION_H

#include <libdrizzle/libdrizzle.h>
#include <mysys/hash.h>


#include "slave.h"

typedef struct st_slave_info
{
  uint32_t server_id;
  uint32_t rpl_recovery_rank, master_id;
  char host[HOSTNAME_LENGTH+1];
  char user[USERNAME_LENGTH+1];
  char password[MAX_PASSWORD_LENGTH+1];
  uint16_t port;
  Session* session;
} SLAVE_INFO;

extern bool opt_show_slave_auth_info;
extern char *master_host, *master_info_file;
extern bool server_id_supplied;

extern int max_binlog_dump_events;

int start_slave(Session* session, Master_info* mi, bool net_report);
int stop_slave(Session* session, Master_info* mi, bool net_report);
bool change_master(Session* session, Master_info* mi);
bool mysql_show_binlog_events(Session* session);
int cmp_master_pos(const char* log_file_name1, uint64_t log_pos1,
		   const char* log_file_name2, uint64_t log_pos2);
int reset_slave(Session *session, Master_info* mi);
int reset_master(Session* session);
bool purge_master_logs(Session* session, const char* to_log);
bool purge_master_logs_before_date(Session* session, time_t purge_time);
bool log_in_use(const char* log_name);
void adjust_linfo_offsets(my_off_t purge_offset);
bool show_binlogs(Session* session);
void kill_zombie_dump_threads(uint32_t slave_server_id);
int check_binlog_magic(IO_CACHE* log, const char** errmsg);

typedef struct st_load_file_info
{
  Session* session;
  my_off_t last_pos_in_file;
  bool wrote_create_file, log_delayed;
} LOAD_FILE_INFO;

int log_loaded_block(IO_CACHE* file);
int init_replication_sys_vars();

void mysql_binlog_send(Session* session, char* log_ident, my_off_t pos, uint16_t flags);

#endif /* DRIZZLED_REPLICATION_REPLICATION_H */
