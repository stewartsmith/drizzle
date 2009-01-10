/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Mark Atwood
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

#include <drizzled/server_includes.h>
#include <drizzled/plugin_logging.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>

#include <syslog.h>
#include <stdarg.h>

// copied from drizzled/sql_parse.cc
const LEX_STRING command_name[]={
  { C_STRING_WITH_LEN("Sleep") },
  { C_STRING_WITH_LEN("Quit") },
  { C_STRING_WITH_LEN("InitDB") },
  { C_STRING_WITH_LEN("Query") },
  { C_STRING_WITH_LEN("FieldList") },
  { C_STRING_WITH_LEN("CreateDB") },
  { C_STRING_WITH_LEN("DropDB") },
  { C_STRING_WITH_LEN("Refresh") },
  { C_STRING_WITH_LEN("Shutdown") },
  { C_STRING_WITH_LEN("Processlist") },
  { C_STRING_WITH_LEN("Connect") },
  { C_STRING_WITH_LEN("Kill") },
  { C_STRING_WITH_LEN("Ping") },
  { C_STRING_WITH_LEN("Time") },
  { C_STRING_WITH_LEN("ChangeUser") },
  { C_STRING_WITH_LEN("BinlogDump") },
  { C_STRING_WITH_LEN("ConnectOut") },
  { C_STRING_WITH_LEN("RegisterSlave") },
  { C_STRING_WITH_LEN("SetOption") },
  { C_STRING_WITH_LEN("Daemon") },
  { C_STRING_WITH_LEN("Error") }
};

/* stolen from mysys/my_getsystime
   until the Session has a good utime "now" we can use
   will have to use this instead */

#include <sys/time.h>
static uint64_t get_microtime()
{
#if defined(HAVE_GETHRTIME)
  return gethrtime()/1000;
#else
  uint64_t newtime;
  struct timeval t;
  /* loop is because gettimeofday may fail on some systems */
  while (gettimeofday(&t, NULL) != 0) {}
  newtime= (uint64_t)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif
}

bool logging_syslog_func_post (Session *session)
{
  assert(session != NULL);

  /* skip returning field list, too verbose */
  if (session->command == COM_FIELD_LIST) return false;

  uint64_t t_mark= get_microtime();

  syslog(LOG_INFO, "thread_id=%ld query_id=%ld"
         " t_connect=%lld t_start=%lld t_lock=%lld"
         " command=%.*s"
         " rows_sent=%ld rows_examined=%u\n"
         " db=\"%.*s\" query=\"%.*s\"\n",
         (unsigned long) session->thread_id,
         (unsigned long) session->query_id,
         (unsigned long long)(t_mark - session->connect_utime),
         (unsigned long long)(t_mark - session->start_utime),
         (unsigned long long)(t_mark - session->utime_after_lock),
         (uint32_t)command_name[session->command].length,
         command_name[session->command].str,
         (unsigned long) session->sent_row_count,
         (uint32_t) session->examined_row_count,
         session->db_length, session->db,
         session->query_length, session->query);

  return false;
}

static int logging_syslog_plugin_init(void *p)
{
  logging_t *l= (logging_t *) p;

  openlog("drizzled", LOG_PID, LOG_LOCAL3);

  l->logging_pre= NULL;
  l->logging_post= logging_syslog_func_post;

  return 0;
}

static int logging_syslog_plugin_deinit(void *p)
{
  logging_st *l= (logging_st *) p;

  l->logging_pre= NULL;
  l->logging_post= NULL;

  return 0;
}

static struct st_mysql_sys_var* logging_syslog_system_variables[]= {
  NULL
};

mysql_declare_plugin(logging_syslog)
{
  DRIZZLE_LOGGER_PLUGIN,
  "logging_syslog",
  "0.1",
  "Mark Atwood <mark@fallenpegasus.com>",
  N_("Log to syslog"),
  PLUGIN_LICENSE_GPL,
  logging_syslog_plugin_init,
  logging_syslog_plugin_deinit,
  NULL,   /* status variables */
  logging_syslog_system_variables,
  NULL
}
mysql_declare_plugin_end;
