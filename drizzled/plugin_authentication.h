/*
  Definitions required for Authentication plugin 
*/

#ifndef DRIZZLED_PLUGIN_AUTHENTICATION_H
#define DRIZZLED_PLUGIN_AUTHENTICATION_H

typedef struct authentication_st
{
  bool (*authenticate)(THD *thd, const char *password);
} authentication_st;

#endif /* DRIZZLED_PLUGIN_AUTHENTICATION_H */
