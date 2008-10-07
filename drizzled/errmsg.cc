#include <drizzled/server_includes.h>
#include <drizzled/errmsg.h>

int errmsg_initializer(st_plugin_int *plugin)
{
  errmsg_t *p;

  fprintf(stderr, "MRA %s plugin:%s dl:%s\n",
	  __func__, plugin->name.str, plugin->plugin_dl->dl.str);

  p= (errmsg_t *) malloc(sizeof(errmsg_t));
  if (p == NULL) return 1;
  memset(p, 0, sizeof(errmsg_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      sql_print_error("Errmsg plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }
  return 0;

err:
  free(p);
  return 1;
}

int errmsg_finalizer(st_plugin_int *plugin)
{ 
  errmsg_t *p = (errmsg_t *) plugin->data;

  fprintf(stderr, "MRA %s plugin:%s dl:%s\n",
	  __func__, plugin->name.str, plugin->plugin_dl->dl.str);

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      sql_print_error("Errmsg plugin '%s' deinit function returned error.",
                      plugin->name.str);
    }
  }

  if (p) free(p);

  return 0;
}

static bool errmsg_iterate (THD *thd, plugin_ref plugin,
			    void *stuff __attribute__ ((__unused__)))
{
  errmsg_t *l= plugin_data(plugin, errmsg_t *);

  if (l && l->errmsg_pre)
  {
    if (l->errmsg_pre(thd))
      return true;
  }
  return false;
}

void errmsg_vprintf (THD *thd, int priority, const char *format, va_list ap)
{
}

void errmsg_pre_do (THD *thd)
{
  if (plugin_foreach(thd, errmsg_pre_iterate, DRIZZLE_LOGGER_PLUGIN, NULL))
  {
    sql_print_error("Errmsg plugin pre had an error.");
  }
  return;
}
