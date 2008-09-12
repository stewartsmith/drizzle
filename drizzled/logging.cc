#include <drizzled/server_includes.h>
#include <drizzled/logging.h>

int logging_initializer(st_plugin_int *plugin)
{
  logging_t *p;

  fprintf(stderr, "MRA %s plugin:%s dl:%s\n",
	  __func__, plugin->name.str, plugin->plugin_dl->dl.str);

  p= (logging_t *) malloc(sizeof(logging_t));
  if (p == NULL) return 1;
  memset(p, 0, sizeof(logging_st));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      sql_print_error("Logging plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }
  return 0;

err:
  free(p);
  return 1;
}

int logging_finalizer(st_plugin_int *plugin)
{ 
  logging_st *p = (logging_st *) plugin->data;

  fprintf(stderr, "MRA %s plugin:%s dl:%s\n",
	  __func__, plugin->name.str, plugin->plugin_dl->dl.str);

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      sql_print_error("Logging plugin '%s' deinit function returned error.",
                      plugin->name.str);
    }
  }

  if (p) free(p);

  return 0;
}

bool logging_pre_do_i (THD *thd,
		       plugin_ref plugin,
		       void *stuff)
{
  logging_t *l= plugin_data(plugin, logging_t *);

  if (l && l->logging_pre)
  {
    if (l->logging_pre(thd, stuff))
      return true;
  }
  return false;
}

void logging_pre_do (THD *thd, void *stuff)
{
  if (plugin_foreach(thd, logging_pre_do_i, DRIZZLE_LOGGER_PLUGIN, stuff))
  {
    sql_print_error("Logging plugin pre had an error.");
  }
  return;
}

bool logging_post_do_i (THD *thd,
		       plugin_ref plugin,
		       void *stuff)
{
  logging_t *l= plugin_data(plugin, logging_t *);

  if (l && l->logging_post)
  {
    if (l->logging_post(thd, stuff))
      return true;
  }
  return false;
}

void logging_post_do (THD *thd, void *stuff)
{
  if (plugin_foreach(thd, logging_post_do_i, DRIZZLE_LOGGER_PLUGIN, stuff))
  {
    sql_print_error("Logging plugin post had an error.");
  }
  return;
}
