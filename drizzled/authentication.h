#ifndef DRIZZLED_AUTHENTICATION_H
#define DRIZZLED_AUTHENTICATION_H

#include <drizzled/plugin_authentication.h>

bool authenticate_user(THD *thd, const char *password);
int authentication_initializer(st_plugin_int *plugin);
int authentication_finalizer(st_plugin_int *plugin);

#endif /* DRIZZLED_AUTHENTICATION_H */
