/*
  Definitions required for Logging plugin 
*/

#ifndef DRIZZLED_PLUGIN_LOGGING_H
#define DRIZZLED_PLUGIN_LOGGING_H

typedef struct logging_st
{
  bool (*logging_pre)(THD *thd, void *stuff);
  bool (*logging_post)(THD *thd, void *stuff);
} logging_t;

#endif /* DRIZZLED_PLUGIN_LOGGING_H */
