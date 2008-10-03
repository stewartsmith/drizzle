/* drizzle/plugin/logging_query/logging_query.cc */

/* need to define DRIZZLE_SERVER to get inside the THD */
#define DRIZZLE_SERVER 1
#include <drizzled/server_includes.h>
#include <drizzled/plugin_logging.h>

#define MAX_MSG_LEN (32*1024)

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


bool logging_query_func_pre (THD *thd)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len = 0;
  int wrv;

  assert(thd != NULL);
  assert(fd > 0);

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
	     "log bgn thread_id=%ld query_id=%ld command=%.*s"
	     " db=\"%.*s\" query=\"%.*s\"\n",
	     (unsigned long) thd->thread_id,
	     (unsigned long) thd->query_id,
	     command_name[thd->command].length, command_name[thd->command].str,
	     thd->db_length, thd->db,
	     thd->query_length, thd->query);
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);

  return false;
}

bool logging_query_func_post (THD *thd)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len = 0;
  int wrv;

  assert(thd != NULL);
  assert(fd > 0);

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
	     "log end thread_id=%ld query_id=%ld command=%.*s"
	     " utime=%lld rows.sent=%ld rows.exam=%ld\n",
	     (unsigned long) thd->thread_id, 
	     (unsigned long) thd->query_id,
	     command_name[thd->command].length, command_name[thd->command].str,
	     (thd->current_utime() - thd->start_utime),
	     (unsigned long) thd->sent_row_count,
	     (unsigned long) thd->examined_row_count);
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);

  // some other interesting things in the THD
  // thd->enable_slow_log

  return false;
}

static int logging_query_plugin_init(void *p)
{
  logging_t *l= (logging_t *) p;

  l->logging_pre= logging_query_func_pre;
  l->logging_post= logging_query_func_post;

  fd= open("/tmp/drizzle.log", O_WRONLY | O_APPEND);
  if (fd < 0) {
    fprintf(stderr,
	    "MRA fail open /tmp/drizzle.log fd=%d er=%s\n",
	    fd, strerror(errno));
    return fd;
  }

  /* need to do something better with the fd */

  return 0;
}

static int logging_query_plugin_deinit(void *p)
{
  logging_st *l= (logging_st *) p;

  close(fd);

  l->logging_pre= NULL;
  l->logging_post= NULL;

  return 0;
}

mysql_declare_plugin(logging_query)
{
  DRIZZLE_LOGGER_PLUGIN,
  "logging_query",
  "0.1",
  "Mark Atwood <mark@fallenpegasus.com>",
  "Log queries",
  PLUGIN_LICENSE_GPL,
  logging_query_plugin_init,
  logging_query_plugin_deinit,
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
