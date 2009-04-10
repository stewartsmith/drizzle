/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef DRIZZLED_PLUGIN_H
#define DRIZZLED_PLUGIN_H

#include <drizzled/lex_string.h>
#include <drizzled/xid.h>

class Session;
class Item;

/*************************************************************************
  Plugin API. Common for all plugin types.
*/

/*
  The allowable types of plugins
*/
enum drizzle_plugin_type {
  DRIZZLE_DAEMON_PLUGIN,                /* Daemon / Raw */
  DRIZZLE_STORAGE_ENGINE_PLUGIN,        /* Storage Engine */
  DRIZZLE_INFORMATION_SCHEMA_PLUGIN,    /* Information Schema */
  DRIZZLE_UDF_PLUGIN,                   /* User-Defined Function */
  DRIZZLE_UDA_PLUGIN,                   /* User-Defined Aggregate Function */
  DRIZZLE_AUDIT_PLUGIN,                 /* Audit */
  DRIZZLE_LOGGER_PLUGIN,                /* Query Logging */
  DRIZZLE_ERRMSG_PLUGIN,                /* Error Messages */
  DRIZZLE_AUTH_PLUGIN,                  /* Authorization */
  DRIZZLE_QCACHE_PLUGIN,                /* Query Cache */
  DRIZZLE_SCHEDULING_PLUGIN,            /* Thread and Session Scheduling */
  DRIZZLE_REPLICATOR_PLUGIN,            /* Database Replication */
  DRIZZLE_PROTOCOL_PLUGIN,              /* Protocol Handlers */
  DRIZZLE_PLUGIN_MAX=DRIZZLE_PROTOCOL_PLUGIN
};

/* The number of plugin types */
const uint32_t DRIZZLE_MAX_PLUGIN_TYPE_NUM=DRIZZLE_PLUGIN_MAX+1;

/* We use the following strings to define licenses for plugins */
enum plugin_license_type {
  PLUGIN_LICENSE_PROPRIETARY,
  PLUGIN_LICENSE_GPL,
  PLUGIN_LICENSE_BSD,
  PLUGIN_LICENSE_LGPL,
  PLUGIN_LICENSE_MAX=PLUGIN_LICENSE_LGPL
};

const char * const PLUGIN_LICENSE_PROPRIETARY_STRING="PROPRIETARY";
const char * const PLUGIN_LICENSE_GPL_STRING="GPL";
const char * const PLUGIN_LICENSE_BSD_STRING="BSD";
const char * const PLUGIN_LICENSE_LGPL_STRING="LGPL";

/*
  Macros for beginning and ending plugin declarations. Between
  drizzle_declare_plugin and drizzle_declare_plugin_end there should
  be a st_mysql_plugin struct for each plugin to be declared.
*/


#ifndef DRIZZLE_DYNAMIC_PLUGIN
#define __DRIZZLE_DECLARE_PLUGIN(NAME, DECLS) \
struct st_mysql_plugin DECLS[]= {
#else
#define __DRIZZLE_DECLARE_PLUGIN(NAME, DECLS) \
struct st_mysql_plugin _mysql_plugin_declarations_[]= {
#endif

#define drizzle_declare_plugin(NAME) \
__DRIZZLE_DECLARE_PLUGIN(NAME, \
                 builtin_ ## NAME ## _plugin)

#define drizzle_declare_plugin_end ,{0,0,0,0,0,0,0,0,0,0,0}}

/*
  declarations for SHOW STATUS support in plugins
*/
enum enum_mysql_show_type
{
  SHOW_UNDEF, SHOW_BOOL, SHOW_INT, SHOW_LONG,
  SHOW_LONGLONG, SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_ARRAY, SHOW_FUNC, SHOW_DOUBLE, SHOW_SIZE
};

struct st_mysql_show_var {
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
};


#define SHOW_VAR_FUNC_BUFF_SIZE 1024
typedef int (*mysql_show_var_func)(Session *, struct st_mysql_show_var *, char *);

struct st_show_var_func_container {
  mysql_show_var_func func;
};
/*
  declarations for server variables and command line options
*/


#define PLUGIN_VAR_BOOL         0x0001
#define PLUGIN_VAR_INT          0x0002
#define PLUGIN_VAR_LONG         0x0003
#define PLUGIN_VAR_LONGLONG     0x0004
#define PLUGIN_VAR_STR          0x0005
#define PLUGIN_VAR_ENUM         0x0006
#define PLUGIN_VAR_SET          0x0007
#define PLUGIN_VAR_UNSIGNED     0x0080
#define PLUGIN_VAR_SessionLOCAL     0x0100 /* Variable is per-connection */
#define PLUGIN_VAR_READONLY     0x0200 /* Server variable is read only */
#define PLUGIN_VAR_NOSYSVAR     0x0400 /* Not a server variable */
#define PLUGIN_VAR_NOCMDOPT     0x0800 /* Not a command line option */
#define PLUGIN_VAR_NOCMDARG     0x1000 /* No argument for cmd line */
#define PLUGIN_VAR_RQCMDARG     0x0000 /* Argument required for cmd line */
#define PLUGIN_VAR_OPCMDARG     0x2000 /* Argument optional for cmd line */
#define PLUGIN_VAR_MEMALLOC     0x8000 /* String needs memory allocated */

struct st_mysql_sys_var;
struct st_mysql_value;

/*
  SYNOPSIS
    (*mysql_var_check_func)()
      session               thread handle
      var               dynamic variable being altered
      save              pointer to temporary storage
      value             user provided value
  RETURN
    0   user provided value is OK and the update func may be called.
    any other value indicates error.

  This function should parse the user provided value and store in the
  provided temporary storage any data as required by the update func.
  There is sufficient space in the temporary storage to store a double.
  Note that the update func may not be called if any other error occurs
  so any memory allocated should be thread-local so that it may be freed
  automatically at the end of the statement.
*/

typedef int (*mysql_var_check_func)(Session *session,
                                    struct st_mysql_sys_var *var,
                                    void *save, struct st_mysql_value *value);

/*
  SYNOPSIS
    (*mysql_var_update_func)()
      session               thread handle
      var               dynamic variable being altered
      var_ptr           pointer to dynamic variable
      save              pointer to temporary storage
   RETURN
     NONE

   This function should use the validated value stored in the temporary store
   and persist it in the provided pointer to the dynamic variable.
   For example, strings may require memory to be allocated.
*/
typedef void (*mysql_var_update_func)(Session *session,
                                      struct st_mysql_sys_var *var,
                                      void *var_ptr, const void *save);


/* the following declarations are for internal use only */


#define PLUGIN_VAR_MASK \
        (PLUGIN_VAR_READONLY | PLUGIN_VAR_NOSYSVAR | \
         PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_NOCMDARG | \
         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC)

#define DRIZZLE_PLUGIN_VAR_HEADER \
  int flags;                    \
  const char *name;             \
  const char *comment;          \
  mysql_var_check_func check;   \
  mysql_var_update_func update

#define DRIZZLE_SYSVAR_NAME(name) mysql_sysvar_ ## name
#define DRIZZLE_SYSVAR(name) \
  ((struct st_mysql_sys_var *)&(DRIZZLE_SYSVAR_NAME(name)))

/*
  for global variables, the value pointer is the first
  element after the header, the default value is the second.
  for thread variables, the value offset is the first
  element after the header, the default value is the second.
*/


#define DECLARE_DRIZZLE_SYSVAR_BASIC(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  type *value;                  \
  const type def_val;           \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_DRIZZLE_SYSVAR_SIMPLE(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  type *value; type def_val;    \
  type min_val; type max_val;   \
  type blk_sz;                  \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_DRIZZLE_SYSVAR_TYPELIB(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  type *value; type def_val;    \
  TYPELIB *typelib;             \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_SessionVAR_FUNC(type) \
  type *(*resolve)(Session *session, int offset)

#define DECLARE_DRIZZLE_SessionVAR_BASIC(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  const type def_val;           \
  DECLARE_SessionVAR_FUNC(type);    \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_DRIZZLE_SessionVAR_SIMPLE(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  type def_val; type min_val;   \
  type max_val; type blk_sz;    \
  DECLARE_SessionVAR_FUNC(type);    \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_DRIZZLE_SessionVAR_TYPELIB(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  type def_val;                 \
  DECLARE_SessionVAR_FUNC(type);    \
  TYPELIB *typelib;             \
} DRIZZLE_SYSVAR_NAME(name)


/*
  the following declarations are for use by plugin implementors
*/

#define DRIZZLE_SYSVAR_BOOL(name, varname, opt, comment, check, update, def) \
DECLARE_DRIZZLE_SYSVAR_BASIC(name, bool) = { \
  PLUGIN_VAR_BOOL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def}

#define DRIZZLE_SYSVAR_STR(name, varname, opt, comment, check, update, def) \
DECLARE_DRIZZLE_SYSVAR_BASIC(name, char *) = { \
  PLUGIN_VAR_STR | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def}

#define DRIZZLE_SYSVAR_INT(name, varname, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SYSVAR_SIMPLE(name, int) = { \
  PLUGIN_VAR_INT | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, min, max, blk }

#define DRIZZLE_SYSVAR_UINT(name, varname, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SYSVAR_SIMPLE(name, unsigned int) = { \
  PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, min, max, blk }

#define DRIZZLE_SYSVAR_LONG(name, varname, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SYSVAR_SIMPLE(name, long) = { \
  PLUGIN_VAR_LONG | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, min, max, blk }

#define DRIZZLE_SYSVAR_ULONG(name, varname, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SYSVAR_SIMPLE(name, unsigned long) = { \
  PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, min, max, blk }

#define DRIZZLE_SYSVAR_LONGLONG(name, varname, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SYSVAR_SIMPLE(name, int64_t) = { \
  PLUGIN_VAR_LONGLONG | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, min, max, blk }

#define DRIZZLE_SYSVAR_ULONGLONG(name, varname, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SYSVAR_SIMPLE(name, uint64_t) = { \
  PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, min, max, blk }

#define DRIZZLE_SYSVAR_ENUM(name, varname, opt, comment, check, update, def, typelib) \
DECLARE_DRIZZLE_SYSVAR_TYPELIB(name, unsigned long) = { \
  PLUGIN_VAR_ENUM | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, typelib }

#define DRIZZLE_SYSVAR_SET(name, varname, opt, comment, check, update, def, typelib) \
DECLARE_DRIZZLE_SYSVAR_TYPELIB(name, uint64_t) = { \
  PLUGIN_VAR_SET | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def, typelib }

#define DRIZZLE_SessionVAR_BOOL(name, opt, comment, check, update, def) \
DECLARE_DRIZZLE_SessionVAR_BASIC(name, char) = { \
  PLUGIN_VAR_BOOL | PLUGIN_VAR_SessionLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL}

#define DRIZZLE_SessionVAR_STR(name, opt, comment, check, update, def) \
DECLARE_DRIZZLE_SessionVAR_BASIC(name, char *) = { \
  PLUGIN_VAR_STR | PLUGIN_VAR_SessionLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL}

#define DRIZZLE_SessionVAR_INT(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SessionVAR_SIMPLE(name, int) = { \
  PLUGIN_VAR_INT | PLUGIN_VAR_SessionLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_SessionVAR_UINT(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SessionVAR_SIMPLE(name, unsigned int) = { \
  PLUGIN_VAR_INT | PLUGIN_VAR_SessionLOCAL | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_SessionVAR_LONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SessionVAR_SIMPLE(name, long) = { \
  PLUGIN_VAR_LONG | PLUGIN_VAR_SessionLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_SessionVAR_ULONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SessionVAR_SIMPLE(name, unsigned long) = { \
  PLUGIN_VAR_LONG | PLUGIN_VAR_SessionLOCAL | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_SessionVAR_LONGLONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SessionVAR_SIMPLE(name, int64_t) = { \
  PLUGIN_VAR_LONGLONG | PLUGIN_VAR_SessionLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_SessionVAR_ULONGLONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_SessionVAR_SIMPLE(name, uint64_t) = { \
  PLUGIN_VAR_LONGLONG | PLUGIN_VAR_SessionLOCAL | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_SessionVAR_ENUM(name, opt, comment, check, update, def, typelib) \
DECLARE_DRIZZLE_SessionVAR_TYPELIB(name, unsigned long) = { \
  PLUGIN_VAR_ENUM | PLUGIN_VAR_SessionLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL, typelib }

#define DRIZZLE_SessionVAR_SET(name, opt, comment, check, update, def, typelib) \
DECLARE_DRIZZLE_SessionVAR_TYPELIB(name, uint64_t) = { \
  PLUGIN_VAR_SET | PLUGIN_VAR_SessionLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL, typelib }

/* accessor macros */

#define SYSVAR(name) \
  (*(DRIZZLE_SYSVAR_NAME(name).value))

/* when session == null, result points to global value */
#define SessionVAR(session, name) \
  (*(DRIZZLE_SYSVAR_NAME(name).resolve(session, DRIZZLE_SYSVAR_NAME(name).offset)))


struct StorageEngine;


class Plugin
{
private:
  std::string name;
  std::string version;
  std::string author;
  std::string description;

public:
  Plugin(std::string in_name, std::string in_version,
         std::string in_author, std::string in_description)
    : name(in_name), version(in_version),
    author(in_author), description(in_description)
  {}

  virtual ~Plugin() {}

  virtual void add_functions() {}

};

/*************************************************************************
  st_mysql_value struct for reading values from mysqld.
  Used by server variables framework to parse user-provided values.
  Will be used for arguments when implementing UDFs.

  Note that val_str() returns a string in temporary memory
  that will be freed at the end of statement. Copy the string
  if you need it to persist.
*/

#define DRIZZLE_VALUE_TYPE_STRING 0
#define DRIZZLE_VALUE_TYPE_REAL   1
#define DRIZZLE_VALUE_TYPE_INT    2

struct st_mysql_value
{
  int (*value_type)(struct st_mysql_value *);
  const char *(*val_str)(struct st_mysql_value *, char *buffer, int *length);
  int (*val_real)(struct st_mysql_value *, double *realbuf);
  int (*val_int)(struct st_mysql_value *, int64_t *intbuf);
};


/*************************************************************************
  Miscellaneous functions for plugin implementors
*/

#ifdef __cplusplus
extern "C" {
#endif

int session_in_lock_tables(const Session *session);
int session_tablespace_op(const Session *session);
void set_session_proc_info(Session *session, const char *info);
const char *get_session_proc_info(Session *session);
int64_t session_test_options(const Session *session, int64_t test_options);
int session_sql_command(const Session *session);
void **session_ha_data(const Session *session, const struct StorageEngine *engine);
int session_tx_isolation(const Session *session);
/* Increments the row counter, see Session::row_count */
void session_inc_row_count(Session *session);

LEX_STRING *session_make_lex_string(Session *session, LEX_STRING *lex_str,
                                    const char *str, unsigned int size,
                                    int allocate_lex_string);



/**
  Create a temporary file.

  @details
  The temporary file is created in a location specified by the mysql
  server configuration (--tmpdir option).  The caller does not need to
  delete the file, it will be deleted automatically.

  @param prefix  prefix for temporary file name
  @retval -1    error
  @retval >= 0  a file handle that can be passed to dup or my_close
*/
int mysql_tmpfile(const char *prefix);

/**
  Check the killed state of a connection

  @details
  In MySQL support for the KILL statement is cooperative. The KILL
  statement only sets a "killed" flag. This function returns the value
  of that flag.  A thread should check it often, especially inside
  time-consuming loops, and gracefully abort the operation if it is
  non-zero.

  @param session  user thread connection handle
  @retval 0  the connection is active
  @retval 1  the connection has been killed
*/
int session_killed(const Session *session);


/**
  Return the thread id of a user thread

  @param session  user thread connection handle
  @return  thread id
*/
unsigned long session_get_thread_id(const Session *session);


/**
  Allocate memory in the connection's local memory pool

  @details
  When properly used in place of @c malloc(), this can significantly
  improve concurrency. Don't use this or related functions to allocate
  large chunks of memory. Use for temporary storage only. The memory
  will be freed automatically at the end of the statement; no explicit
  code is required to prevent memory leaks.

  @see alloc_root()
*/
void *session_alloc(Session *session, unsigned int size);
/**
  @see session_alloc()
*/
void *session_calloc(Session *session, unsigned int size);
/**
  @see session_alloc()
*/
char *session_strdup(Session *session, const char *str);
/**
  @see session_alloc()
*/
char *session_strmake(Session *session, const char *str, unsigned int size);
/**
  @see session_alloc()
*/
void *session_memdup(Session *session, const void* str, unsigned int size);

/**
  Get the XID for this connection's transaction

  @param session  user thread connection handle
  @param xid  location where identifier is stored
*/
void session_get_xid(const Session *session, DRIZZLE_XID *xid);

/**
  Invalidate the query cache for a given table.

  @param session         user thread connection handle
  @param key         databasename\\0tablename\\0
  @param key_length  length of key in bytes, including the NUL bytes
  @param using_trx   flag: TRUE if using transactions, FALSE otherwise
*/
void mysql_query_cache_invalidate4(Session *session,
                                   const char *key, unsigned int key_length,
                                   int using_trx);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/**
  Provide a handler data getter to simplify coding
*/
inline
void *
session_get_ha_data(const Session *session, const struct StorageEngine *engine)
{
  return *session_ha_data(session, engine);
}

/**
  Provide a handler data setter to simplify coding
*/
inline
void
session_set_ha_data(const Session *session, const struct StorageEngine *engine,
                const void *ha_data)
{
  *session_ha_data(session, engine)= (void*) ha_data;
}
#endif

#endif /* _my_plugin_h */

