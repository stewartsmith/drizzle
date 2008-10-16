/*
  Definitions required for Configuration Variables plugin 
*/

#ifndef DRIZZLED_PLUGIN_CONFIGVAR_H
#define DRIZZLED_PLUGIN_CONFIGVAR_H

typedef struct configvar_st
{
  /* todo, define this api */
  /* this is the API that a configvar plugin must implement.
     it should implement each of these function pointers.
     if a function returns bool true, that means it failed.
     if a function pointer is NULL, that's ok.
  */

  bool (*configvar_func1)(THD *thd, void *parm1, void *parm2);
  bool (*configvar_func2)(THD *thd, void *parm3, void *parm4);
} configvar_t;

#endif /* DRIZZLED_PLUGIN_CONFIGVAR_H */
