/*
  Definitions required for Error Message plugin 
*/

#ifndef DRIZZLED_PLUGIN_ERRMSG_H
#define DRIZZLED_PLUGIN_ERRMSG_H

#include <stdarg.h>

typedef struct errmsg_st
{
  void (*errmsg_func)(THD *thd, int priority, const char *format, va_list ap);
} errmsg_t;

#endif /* DRIZZLED_PLUGIN_ERRMSG_H */
