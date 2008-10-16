#ifndef DRIZZLED_ERRMSG_H
#define DRIZZLED_ERRMSG_H

#include <drizzled/plugin_errmsg.h>

// need stdarg for va_list
#include <stdarg.h>

int errmsg_initializer(st_plugin_int *plugin);
int errmsg_finalizer(st_plugin_int *plugin);

void errmsg_vprintf (THD *thd, int priority, const char *format, va_list ap);
void errmsg_printf (THD *thd, int priority, const char *format, ...);

#endif /* DRIZZLED_ERRMSG_H */
