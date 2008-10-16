#include <drizzled/server_includes.h>
#include <drizzled/configvar.h>

int configvar_initializer(st_plugin_int *plugin)
{
  configvar_t *p;

  p= (configvar_t *) malloc(sizeof(configvar_t));
  if (p == NULL) return 1;
  memset(p, 0, sizeof(configvar_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      sql_print_error("Configvar plugin '%s' init() failed",
		      plugin->name.str);
      goto err;
    }
  }
  return 0;

err:
  free(p);
  return 1;
}

int configvar_finalizer(st_plugin_int *plugin)
{ 
  configvar_t *p= (configvar_t *) plugin->data;

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      sql_print_error("Configvar plugin '%s' deinit() failed",
		      plugin->name.str);
    }
  }

  if (p) free(p);

  return 0;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the thd.
   So we will take all the additional paramters of configvar_do1,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
*/
typedef struct configvar_do1_parms_st
{
  void *parm1;
  void *parm2;
} configvar_do1_parms_t;

/* This gets called by plugin_foreach once for each loaded configvar plugin */
static bool configvar_do1_iterate (THD *thd, plugin_ref plugin, void *p)
{
  configvar_t *l= plugin_data(plugin, configvar_t *);
  configvar_do1_parms_t *parms= (configvar_do1_parms_t *) p;

  /* call this loaded configvar plugin's configvar_func1 function pointer */
  if (l && l->configvar_func1)
  {
    if (l->configvar_func1(thd, parms->parm1, parms->parm2))
    {
      sql_print_error("Configvar plugin '%s' do1() failed",
		      (char *)plugin_name(plugin));

      return true;
    }
  }
  return false;
}

/* This is the configvar_do1 entry point.
   This gets called by the rest of the Drizzle server code */
bool configvar_do1 (THD *thd, void *parm1, void *parm2)
{
  configvar_do1_parms_t parms;
  bool foreach_rv;
  
  /* marshall the parameters so they will fit into the foreach */
  parms.parm1= parm1;
  parms.parm2= parm2;

  /* call configvar_do1_iterate
     once for each loaded configvar plugin */
  foreach_rv= plugin_foreach(thd,
			     configvar_do1_iterate,
			     DRIZZLE_CONFIGVAR_PLUGIN,
			     (void *) &parms);
return foreach_rv;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the thd.
   So we will take all the additional paramters of configvar_do2,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
*/
typedef struct configvar_do2_parms_st
{
  void *parm3;
  void *parm4;
} configvar_do2_parms_t;

/* This gets called by plugin_foreach once for each loaded configvar plugin */
static bool configvar_do2_iterate (THD *thd, plugin_ref plugin, void *p)
{
  configvar_t *l= plugin_data(plugin, configvar_t *);
  configvar_do2_parms_t *parms= (configvar_do2_parms_t *) p;

  /* call this loaded configvar plugin's configvar_func1 function pointer */
  if (l && l->configvar_func1)
  {
    if (l->configvar_func1(thd, parms->parm3, parms->parm4))
    {
      sql_print_error("Configvar plugin '%s' do2() failed",
		      (char *)plugin_name(plugin));

      return true;
    }
  }
  return false;
}

/* This is the configvar_do2 entry point.
   This gets called by the rest of the Drizzle server code */
  bool configvar_do2 (THD *thd, void *parm3, void *parm4)
  {
    configvar_do2_parms_t parms;
    bool foreach_rv;

    /* marshall the parameters so they will fit into the foreach */
    parms.parm3= parm3;
    parms.parm4= parm4;

    /* call configvar_do2_iterate
       once for each loaded configvar plugin */
    foreach_rv= plugin_foreach(thd,
			       configvar_do2_iterate,
			       DRIZZLE_CONFIGVAR_PLUGIN,
			       (void *) &parms);
  return foreach_rv;
}
