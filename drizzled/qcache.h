#ifndef DRIZZLED_QCACHE_H
#define DRIZZLED_QCACHE_H

#include <drizzled/plugin_qcache.h>

int qcache_initializer (st_plugin_int *plugin);
int qcache_finalizer (st_plugin_int *plugin);

/* todo, fill in this API */
/* these are the functions called by the rest of the drizzle server
   to do whatever this plugin does. */
bool qcache_do1 (THD *thd, void *parm1, void *parm2)
bool qcache_do2 (THD *thd, void *parm3, void *parm4)

#endif /* DRIZZLED_QCACHE_H */
