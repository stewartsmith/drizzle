/* drizzle/plugin/logging_noop/logging_noop.cc */

#define DRIZZLE_SERVER 1
#include <drizzled/server_includes.h>
#include <drizzled/plugin_logging.h>

bool logging_noop_func_pre (THD *thd, void *stuff)
{
  /* something fake so unused var warning doesnt cause compile fail */
  return ((void *)thd == stuff);

  return 0;
}

bool logging_noop_func_post (THD *thd, void *stuff)
{
  /* something fake so unused var warning doesnt cause compile fail */
  return ((void *)thd == stuff);

  return 0;
}

static int logging_noop_plugin_init(void *p)
{
  logging_t *l= (logging_t *) p;

  l->logging_pre= logging_noop_func_pre;
  l->logging_post= logging_noop_func_post;

  return 0;
}

static int logging_noop_plugin_deinit(void *p)
{
  logging_st *l= (logging_st *) p;

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
  "Logging Plugin that does nothing",
  PLUGIN_LICENSE_GPL,
  logging_noop_plugin_init,
  logging_noop_plugin_deinit,
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
