#include <drizzled/server_includes.h>
#include <drizzled/authentication.h>

static bool are_plugins_loaded= false;

static bool authenticate_by(THD *thd, plugin_ref plugin, void* p_data)
{
  const char *password= (const char *)p_data;
  authentication_st *auth= plugin_data(plugin, authentication_st *);

  (void)p_data;

  if (auth && auth->authenticate)
  {
    if (auth->authenticate(thd, password))
      return true;
  }

  return false;
}

bool authenticate_user(THD *thd, const char *password)
{
  /* If we never loaded any auth plugins, just return true */
  if (are_plugins_loaded != true)
    return true;

  return plugin_foreach(thd, authenticate_by, DRIZZLE_AUTH_PLUGIN, (void *)password);
}


int authentication_initializer(st_plugin_int *plugin)
{
  authentication_st *authen;

  if ((authen= (authentication_st *)malloc(sizeof(authentication_st))) == 0)
      return(1);

  memset(authen, 0, sizeof(authentication_st));

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init(authen))
    {
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }

  plugin->data= (void *)authen;
  are_plugins_loaded= true;

  return(0);
err:
  free(authen);
  return(1);
}

int authentication_finalizer(st_plugin_int *plugin)
{
  authentication_st *authen= (authentication_st *)plugin->data;

  assert(authen);
  if (authen && plugin->plugin->deinit)
    plugin->plugin->deinit(authen);

  free(authen);

  return(0);
}
