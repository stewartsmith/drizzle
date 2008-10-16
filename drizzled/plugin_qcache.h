/*
  Definitions required for Query Cache plugin 
*/

#ifndef DRIZZLED_PLUGIN_QCACHE_H
#define DRIZZLED_PLUGIN_QCACHE_H

typedef struct qcache_st
{
  /* todo, define this api */
  /* this is the API that a qcache plugin must implement.
     it should implement each of these function pointers.
     if a function returns bool true, that means it failed.
     if a function pointer is NULL, that's ok.
  */

  bool (*qcache_func1)(THD *thd, void *parm1, void *parm2);
  bool (*qcache_func2)(THD *thd, void *parm3, void *parm4);
} qcache_t;

#endif /* DRIZZLED_PLUGIN_QCACHE_H */
