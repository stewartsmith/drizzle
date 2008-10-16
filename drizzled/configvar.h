#ifndef DRIZZLED_CONFIGVAR_H
#define DRIZZLED_CONFIGVAR_H

#include <drizzled/plugin_configvar.h>

int configvar_initializer (st_plugin_int *plugin);
int configvar_finalizer (st_plugin_int *plugin);

/* todo, fill in this API */
/* these are the functions called by the rest of the drizzle server
   to do whatever this plugin does. */
bool configvar_do1 (THD *thd, void *parm1, void *parm2)
bool configvar_do2 (THD *thd, void *parm3, void *parm4)

#endif /* DRIZZLED_CONFIGVAR_H */
