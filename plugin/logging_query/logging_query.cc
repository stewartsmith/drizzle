/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Mark Atwood
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

/* todo, make this dynamic as needed */
#define MAX_MSG_LEN (32*1024)

static char* logging_query_filename= NULL;
static bool logging_query_enable= true;
static ulong logging_query_enable_time= 0;
static ulong logging_query_threshold_big_resultset= 0;

static int fd= -1;

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
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0) {}
  newtime= (uint64_t)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif  /* defined(HAVE_GETHRTIME) */
}

/* we could just not have a pre entrypoint at all,
   and have logging_pre == NULL
   but we have this here for the sake of being an example */
bool logging_query_func_pre (Session *)
{
  return false;
}

bool logging_query_func_post (Session *session)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len= 0;
  int wrv;

  if (fd < 0) return false;

  assert(session != NULL);

  /*
    here is some time stuff from class Session
      uint64_t connect_utime;
        todo, looks like this isnt being set
	we could store the time this plugin was loaded
	but that would just be a dumb workaround
      uint64_t start_utime;
      uint64_t utime_after_lock;
      uint64_t current_utime();
        todo, cant get to because of namemangling
  */

  /* todo, the Session should have a "utime command completed" inside
     itself, so be more accurate, and so plugins dont have to keep
     calling current_utime, which can be slow */
  uint64_t t_mark= get_microtime();

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
             "thread_id=%ld query_id=%ld"
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
  /* a single write has a OS level thread lock
     so there is no need to have mutexes guarding this write,
  */
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);

  return false;
}

static int logging_query_plugin_init(void *p)
{
  logging_t *l= (logging_t *) p;

  if (logging_query_filename == NULL)
  {
    /* no destination filename was specified via system variables
       return now, dont set the callback pointers
    */
    return 0;
  }

  fd= open(logging_query_filename, O_WRONLY | O_APPEND | O_CREAT,
           S_IRUSR|S_IWUSR);
  if (fd < 0)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("fail open() fn=%s er=%s\n"),
                  logging_query_filename,
                  strerror(errno));

    /* todo
       we should return an error here, so the plugin doesnt load
       but this causes Drizzle to crash
       so until that is fixed,
       just return a success,
       but leave the function pointers as NULL and the fd as -1
    */
    return 0;
  }

  l->logging_pre= logging_query_func_pre;
  l->logging_post= logging_query_func_post;

  return 0;
}

static int logging_query_plugin_deinit(void *p)
{
  logging_st *l= (logging_st *) p;

  if (fd >= 0)
  {
    close(fd);
    fd= -1;
  }

  l->logging_pre= NULL;
  l->logging_post= NULL;

  return 0;
}

static DRIZZLE_SYSVAR_STR(
  filename,
  logging_query_filename,
  PLUGIN_VAR_READONLY,
  N_("File to log to"),
  NULL, /* check func */
  NULL, /* update func*/
  NULL /* default */);

static DRIZZLE_SYSVAR_BOOL(
  enable,
  logging_query_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable logging"),
  NULL, /* check func */
  NULL, /* update func */
  true /* default */);

static DRIZZLE_SYSVAR_ULONG(
  enable_time,
  logging_query_enable_time,
  PLUGIN_VAR_OPCMDARG,
  N_("Disable after this many seconds. Zero for forever"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

#ifdef NOT_YET
static DRIZZLE_SYSVAR_ULONG(
  threshhold_slow,
  logging_query_threshold_slow,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging slow queries, in microseconds"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);
#endif

static DRIZZLE_SYSVAR_ULONG(
  threshold_big_resultset,
  logging_query_threshold_big_resultset,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows returned"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

#ifdef NOT_YET
static DRIZZLE_SYSVAR_ULONG(
  threshhold_big_examined,
  logging_query_threshold_big_examined,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows examined"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);
#endif

static struct st_mysql_sys_var* logging_query_system_variables[]= {
  DRIZZLE_SYSVAR(filename),
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(enable_time),
  DRIZZLE_SYSVAR(threshold_big_resultset),
  NULL
};

mysql_declare_plugin(logging_query)
{
  DRIZZLE_LOGGER_PLUGIN,
  "logging_query",
  "0.1",
  "Mark Atwood <mark@fallenpegasus.com>",
  N_("Log queries to a file"),
  PLUGIN_LICENSE_GPL,
  logging_query_plugin_init,
  logging_query_plugin_deinit,
  NULL,   /* status variables */
  logging_query_system_variables,
  NULL
}
mysql_declare_plugin_end;
