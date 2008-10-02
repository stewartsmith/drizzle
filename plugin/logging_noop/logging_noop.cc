/* drizzle/plugin/logging_noop/logging_noop.cc */

/* need to define DRIZZLE_SERVER to get inside the THD */
#define DRIZZLE_SERVER 1
#include <drizzled/server_includes.h>
#include <drizzled/plugin_logging.h>

#define MAX_MSG_LEN (32*1024)

static int fd = -1;

bool logging_noop_func_pre (THD *thd)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len = 0;
  int wrv;

  assert(thd != NULL);
  assert(fd > 0);

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
	     "log bgn thread_id=%ld stmt_id=%ld query_id=%ld command=%d"
	     " db=\"%.*s\" query=\"%.*s\"\n",
	     (unsigned long) thd->thread_id,
	     thd->id,
	     (unsigned long) thd->query_id,
	     thd->command,
	     thd->db_length, thd->db,
	     thd->query_length, thd->query);
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);

  return false;
}

bool logging_noop_func_post (THD *thd)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len = 0;
  int wrv;

  assert(thd != NULL);
  assert(fd > 0);

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
	     "log end thread_id=%ld stmt_id=%ld query_id=%ld command=%d"
	     " utime=%lld rows.sent=%ld rows.exam=%ld\n",
	     (unsigned long) thd->thread_id, 
	     thd->id,
	     (unsigned long) thd->query_id,
	     thd->command,
	     (thd->current_utime() - thd->start_utime),
	     (unsigned long) thd->sent_row_count,
	     (unsigned long) thd->examined_row_count);
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);

  // some other interesting things in the THD
  // thd->enable_slow_log

  return false;
}

static int logging_noop_plugin_init(void *p)
{
  logging_t *l= (logging_t *) p;

  l->logging_pre= logging_noop_func_pre;
  l->logging_post= logging_noop_func_post;

  fd = open("/tmp/drizzle.log", O_WRONLY | O_APPEND);
  if (fd < 0) return fd;

  /* need to do something better with the fd */

  return 0;
}

static int logging_noop_plugin_deinit(void *p)
{
  logging_st *l= (logging_st *) p;

  close(fd);

  l->logging_pre= NULL;
  l->logging_post= NULL;

  return 0;
}

mysql_declare_plugin(logging_noop)
{
  DRIZZLE_LOGGER_PLUGIN,
  "logging_noop",
  "0.1",
  "Mark Atwood <mark@fallenpegasus.com>",
  "Logging Plugin Example",
  PLUGIN_LICENSE_GPL,
  logging_noop_plugin_init,
  logging_noop_plugin_deinit,
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
