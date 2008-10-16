#include <drizzled/server_includes.h>
#include <drizzled/qcache.h>

int qcache_initializer(st_plugin_int *plugin)
{
  qcache_t *p;

  p= (qcache_t *) malloc(sizeof(qcache_t));
  if (p == NULL) return 1;
  memset(p, 0, sizeof(qcache_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      sql_print_error("Qcache plugin '%s' init() failed",
		      plugin->name.str);
      goto err;
    }
  }
  return 0;

err:
  free(p);
  return 1;
}

int qcache_finalizer(st_plugin_int *plugin)
{ 
  qcache_t *p= (qcache_t *) plugin->data;

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      sql_print_error("Qcache plugin '%s' deinit() failed",
		      plugin->name.str);
    }
  }

  if (p) free(p);

  return 0;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the thd.
   So we will take all the additional paramters of qcache_do1,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
 */
typedef struct qcache_do1_parms_st
{
  void *parm1;
  void *parm2;
} qcache_do1_parms_t;

/* This gets called by plugin_foreach once for each loaded qcache plugin */
static bool qcache_do1_iterate (THD *thd, plugin_ref plugin, void *p)
{
  qcache_t *l= plugin_data(plugin, qcache_t *);
  qcache_do1_parms_t *parms= (qcache_do1_parms_t *) p;

  /* call this loaded qcache plugin's qcache_func1 function pointer */
  if (l && l->qcache_func1)
  {
    if (l->qcache_func1(thd, parms->parm1, parms->parm2))
    {
      sql_print_error("Qcache plugin '%s' do1() failed",
		      plugin->name.str);

      return true;
  }
  return false;
}

/* This is the qcache_do1 entry point.
   This gets called by the rest of the Drizzle server code */
bool qcache_do1 (THD *thd, void *parm1, void *parm2)
{
  qcache_do1_parms_t parms;
  bool foreach_rv;

  /* marshall the parameters so they will fit into the foreach */
  parms.parm1= parm1;
  parms.parm2= parm2;

  /* call qcache_do1_iterate
     once for each loaded qcache plugin */
  foreach_rv= plugin_foreach(thd,
			     qcache_do1_iterate,
			     DRIZZLE_QCACHE_PLUGIN,
			     (void *) &parms));
  return foreach_rv;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the thd.
   So we will take all the additional paramters of qcache_do2,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
 */
typedef struct qcache_do2_parms_st
{
  void *parm3;
  void *parm4;
} qcache_do2_parms_t;

/* This gets called by plugin_foreach once for each loaded qcache plugin */
static bool qcache_do2_iterate (THD *thd, plugin_ref plugin, void *p)
{
  qcache_t *l= plugin_data(plugin, qcache_t *);
  qcache_do2_parms_t *parms= (qcache_do2_parms_t *) p;

  /* call this loaded qcache plugin's qcache_func1 function pointer */
  if (l && l->qcache_func1)
  {
    if (l->qcache_func1(thd, parms->parm3, parms->parm4))
    {
      sql_print_error("Qcache plugin '%s' do2() failed",
		      plugin->name.str);

      return true;
  }
  return false;
}

/* This is the qcache_do2 entry point.
   This gets called by the rest of the Drizzle server code */
bool qcache_do2 (THD *thd, void *parm3, void *parm4)
{
  qcache_do2_parms_t parms;
  bool foreach_rv;

  /* marshall the parameters so they will fit into the foreach */
  parms.parm3= parm3;
  parms.parm4= parm4;

  /* call qcache_do2_iterate
     once for each loaded qcache plugin */
  foreach_rv= plugin_foreach(thd,
			     qcache_do2_iterate,
			     DRIZZLE_QCACHE_PLUGIN,
			     (void *) &parms));
  return foreach_rv;
}
