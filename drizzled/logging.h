#ifndef DRIZZLED_LOGGING_H
#define DRIZZLED_LOGGING_H

#include <drizzled/plugin_logging.h>

int logging_initializer(st_plugin_int *plugin);
int logging_finalizer(st_plugin_int *plugin);

void logging_pre_do (THD *thd);
void logging_post_do (THD *thd);

#endif /* DRIZZLED_LOGGING_H */
