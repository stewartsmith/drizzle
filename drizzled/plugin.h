/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _my_plugin_h
#define _my_plugin_h

#include <drizzled/global.h>

#ifdef __cplusplus
class THD;
class Item;
#define DRIZZLE_THD THD*
#else
#define DRIZZLE_THD void*
#endif


#ifndef _m_string_h
/* This definition must match the one given in m_string.h */
struct st_mysql_lex_string
{
  char *str;
  unsigned int length;
};
#endif /* _m_string_h */
typedef struct st_mysql_lex_string DRIZZLE_LEX_STRING;

#define DRIZZLE_XIDDATASIZE 128
/**
  struct st_mysql_xid is binary compatible with the XID structure as
  in the X/Open CAE Specification, Distributed Transaction Processing:
  The XA Specification, X/Open Company Ltd., 1991.
  http://www.opengroup.org/bookstore/catalog/c193.htm

  @see XID in sql/handler.h
*/
struct st_mysql_xid {
  long formatID;
  long gtrid_length;
  long bqual_length;
  char data[DRIZZLE_XIDDATASIZE];  /* Not \0-terminated */
};
typedef struct st_mysql_xid DRIZZLE_XID;

/*************************************************************************
  Plugin API. Common for all plugin types.
*/

/*
  The allowable types of plugins
*/
#define DRIZZLE_DAEMON_PLUGIN          0  /* Daemon / Raw */
#define DRIZZLE_STORAGE_ENGINE_PLUGIN  1  /* Storage Engine */
#define DRIZZLE_INFORMATION_SCHEMA_PLUGIN  2  /* Information Schema */
#define DRIZZLE_UDF_PLUGIN             3  /* User-Defined Function */
#define DRIZZLE_UDA_PLUGIN             4  /* User-Defined Aggregate function */
#define DRIZZLE_AUDIT_PLUGIN           5  /* Audit */
#define DRIZZLE_LOGGER_PLUGIN          6  /* Logging */
#define DRIZZLE_AUTH_PLUGIN            7  /* Authorization */

#define DRIZZLE_MAX_PLUGIN_TYPE_NUM    8  /* The number of plugin types */

/* We use the following strings to define licenses for plugins */
#define PLUGIN_LICENSE_PROPRIETARY 0
#define PLUGIN_LICENSE_GPL 1
#define PLUGIN_LICENSE_BSD 2

#define PLUGIN_LICENSE_PROPRIETARY_STRING "PROPRIETARY"
#define PLUGIN_LICENSE_GPL_STRING "GPL"
#define PLUGIN_LICENSE_BSD_STRING "BSD"

/*
  Macros for beginning and ending plugin declarations.  Between
  mysql_declare_plugin and mysql_declare_plugin_end there should
  be a st_mysql_plugin struct for each plugin to be declared.
*/


#ifndef DRIZZLE_DYNAMIC_PLUGIN
#define __DRIZZLE_DECLARE_PLUGIN(NAME, DECLS) \
struct st_mysql_plugin DECLS[]= {
#else
#define __DRIZZLE_DECLARE_PLUGIN(NAME, DECLS) \
struct st_mysql_plugin _mysql_plugin_declarations_[]= {
#endif

#define mysql_declare_plugin(NAME) \
__DRIZZLE_DECLARE_PLUGIN(NAME, \
                 builtin_ ## NAME ## _plugin)

#define mysql_declare_plugin_end ,{0,0,0,0,0,0,0,0,0,0,0}}

/*
  declarations for SHOW STATUS support in plugins
*/
enum enum_mysql_show_type
{
  SHOW_UNDEF, SHOW_BOOL, SHOW_INT, SHOW_LONG,
  SHOW_LONGLONG, SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_ARRAY, SHOW_FUNC, SHOW_DOUBLE
};

struct st_mysql_show_var {
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
};


#define SHOW_VAR_FUNC_BUFF_SIZE 1024
typedef int (*mysql_show_var_func)(DRIZZLE_THD, struct st_mysql_show_var*, char *);

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
#define PLUGIN_VAR_THDLOCAL     0x0100 /* Variable is per-connection */
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
      thd               thread handle
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

typedef int (*mysql_var_check_func)(DRIZZLE_THD thd,
                                    struct st_mysql_sys_var *var,
                                    void *save, struct st_mysql_value *value);

/*
  SYNOPSIS
    (*mysql_var_update_func)()
      thd               thread handle
      var               dynamic variable being altered
      var_ptr           pointer to dynamic variable
      save              pointer to temporary storage
   RETURN
     NONE
   
   This function should use the validated value stored in the temporary store
   and persist it in the provided pointer to the dynamic variable.
   For example, strings may require memory to be allocated.
*/
typedef void (*mysql_var_update_func)(DRIZZLE_THD thd,
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

#define DECLARE_THDVAR_FUNC(type) \
  type *(*resolve)(DRIZZLE_THD thd, int offset)

#define DECLARE_DRIZZLE_THDVAR_BASIC(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  const type def_val;           \
  DECLARE_THDVAR_FUNC(type);    \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_DRIZZLE_THDVAR_SIMPLE(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  type def_val; type min_val;   \
  type max_val; type blk_sz;    \
  DECLARE_THDVAR_FUNC(type);    \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_DRIZZLE_THDVAR_TYPELIB(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  type def_val;                 \
  DECLARE_THDVAR_FUNC(type);    \
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

#define DRIZZLE_THDVAR_BOOL(name, opt, comment, check, update, def) \
DECLARE_DRIZZLE_THDVAR_BASIC(name, char) = { \
  PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL}

#define DRIZZLE_THDVAR_STR(name, opt, comment, check, update, def) \
DECLARE_DRIZZLE_THDVAR_BASIC(name, char *) = { \
  PLUGIN_VAR_STR | PLUGIN_VAR_THDLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL}

#define DRIZZLE_THDVAR_INT(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_THDVAR_SIMPLE(name, int) = { \
  PLUGIN_VAR_INT | PLUGIN_VAR_THDLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_THDVAR_UINT(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_THDVAR_SIMPLE(name, unsigned int) = { \
  PLUGIN_VAR_INT | PLUGIN_VAR_THDLOCAL | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_THDVAR_LONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_THDVAR_SIMPLE(name, long) = { \
  PLUGIN_VAR_LONG | PLUGIN_VAR_THDLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_THDVAR_ULONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_THDVAR_SIMPLE(name, unsigned long) = { \
  PLUGIN_VAR_LONG | PLUGIN_VAR_THDLOCAL | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_THDVAR_LONGLONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_THDVAR_SIMPLE(name, int64_t) = { \
  PLUGIN_VAR_LONGLONG | PLUGIN_VAR_THDLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_THDVAR_ULONGLONG(name, opt, comment, check, update, def, min, max, blk) \
DECLARE_DRIZZLE_THDVAR_SIMPLE(name, uint64_t) = { \
  PLUGIN_VAR_LONGLONG | PLUGIN_VAR_THDLOCAL | PLUGIN_VAR_UNSIGNED | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, min, max, blk, NULL }

#define DRIZZLE_THDVAR_ENUM(name, opt, comment, check, update, def, typelib) \
DECLARE_DRIZZLE_THDVAR_TYPELIB(name, unsigned long) = { \
  PLUGIN_VAR_ENUM | PLUGIN_VAR_THDLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL, typelib }

#define DRIZZLE_THDVAR_SET(name, opt, comment, check, update, def, typelib) \
DECLARE_DRIZZLE_THDVAR_TYPELIB(name, uint64_t) = { \
  PLUGIN_VAR_SET | PLUGIN_VAR_THDLOCAL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, -1, def, NULL, typelib }

/* accessor macros */

#define SYSVAR(name) \
  (*(DRIZZLE_SYSVAR_NAME(name).value))

/* when thd == null, result points to global value */
#define THDVAR(thd, name) \
  (*(DRIZZLE_SYSVAR_NAME(name).resolve(thd, DRIZZLE_SYSVAR_NAME(name).offset)))


/*
  Plugin description structure.
*/

struct st_mysql_plugin
{
  int type;             /* the plugin type (a DRIZZLE_XXX_PLUGIN value)   */
  const char *name;     /* plugin name (for SHOW PLUGINS)               */
  const char *version;  /* plugin version (for SHOW PLUGINS)            */
  const char *author;   /* plugin author (for SHOW PLUGINS)             */
  const char *descr;    /* general descriptive text (for SHOW PLUGINS ) */
  int license;          /* the plugin license (PLUGIN_LICENSE_XXX)      */
  int (*init)(void *);  /* the function to invoke when plugin is loaded */
  int (*deinit)(void *);/* the function to invoke when plugin is unloaded */
  struct st_mysql_show_var *status_vars;
  struct st_mysql_sys_var **system_vars;
  void * __reserved1;   /* reserved for dependency checking             */
};

struct handlerton;


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

int thd_in_lock_tables(const DRIZZLE_THD thd);
int thd_tablespace_op(const DRIZZLE_THD thd);
int64_t thd_test_options(const DRIZZLE_THD thd, int64_t test_options);
int thd_sql_command(const DRIZZLE_THD thd);
const char *thd_proc_info(DRIZZLE_THD thd, const char *info);
void **thd_ha_data(const DRIZZLE_THD thd, const struct handlerton *hton);
int thd_tx_isolation(const DRIZZLE_THD thd);
/* Increments the row counter, see THD::row_count */
void thd_inc_row_count(DRIZZLE_THD thd);

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

  @param thd  user thread connection handle
  @retval 0  the connection is active
  @retval 1  the connection has been killed
*/
int thd_killed(const DRIZZLE_THD thd);


/**
  Return the thread id of a user thread

  @param thd  user thread connection handle
  @return  thread id
*/
unsigned long thd_get_thread_id(const DRIZZLE_THD thd);


/**
  Allocate memory in the connection's local memory pool

  @details
  When properly used in place of @c my_malloc(), this can significantly
  improve concurrency. Don't use this or related functions to allocate
  large chunks of memory. Use for temporary storage only. The memory
  will be freed automatically at the end of the statement; no explicit
  code is required to prevent memory leaks.

  @see alloc_root()
*/
void *thd_alloc(DRIZZLE_THD thd, unsigned int size);
/**
  @see thd_alloc()
*/
void *thd_calloc(DRIZZLE_THD thd, unsigned int size);
/**
  @see thd_alloc()
*/
char *thd_strdup(DRIZZLE_THD thd, const char *str);
/**
  @see thd_alloc()
*/
char *thd_strmake(DRIZZLE_THD thd, const char *str, unsigned int size);
/**
  @see thd_alloc()
*/
void *thd_memdup(DRIZZLE_THD thd, const void* str, unsigned int size);

/**
  Create a LEX_STRING in this connection's local memory pool

  @param thd      user thread connection handle
  @param lex_str  pointer to LEX_STRING object to be initialized
  @param str      initializer to be copied into lex_str
  @param size     length of str, in bytes
  @param allocate_lex_string  flag: if TRUE, allocate new LEX_STRING object,
                              instead of using lex_str value
  @return  NULL on failure, or pointer to the LEX_STRING object

  @see thd_alloc()
*/
DRIZZLE_LEX_STRING *thd_make_lex_string(DRIZZLE_THD thd, DRIZZLE_LEX_STRING *lex_str,
                                      const char *str, unsigned int size,
                                      int allocate_lex_string);

/**
  Get the XID for this connection's transaction

  @param thd  user thread connection handle
  @param xid  location where identifier is stored
*/
void thd_get_xid(const DRIZZLE_THD thd, DRIZZLE_XID *xid);

/**
  Invalidate the query cache for a given table.

  @param thd         user thread connection handle
  @param key         databasename\\0tablename\\0
  @param key_length  length of key in bytes, including the NUL bytes
  @param using_trx   flag: TRUE if using transactions, FALSE otherwise
*/
void mysql_query_cache_invalidate4(DRIZZLE_THD thd,
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
thd_get_ha_data(const DRIZZLE_THD thd, const struct handlerton *hton)
{
  return *thd_ha_data(thd, hton);
}

/**
  Provide a handler data setter to simplify coding
*/
inline
void
thd_set_ha_data(const DRIZZLE_THD thd, const struct handlerton *hton,
                const void *ha_data)
{
  *thd_ha_data(thd, hton)= (void*) ha_data;
}
#endif

#endif

