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

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "drizzled/module/manifest.h"
#include "drizzled/module/module.h"
#include "drizzled/plugin/version.h"
#include "drizzled/module/context.h"
#include "drizzled/definitions.h"

#include "drizzled/lex_string.h"
#include "drizzled/xid.h"

namespace drizzled
{

class Session;
class Item;
struct charset_info_st;

/*************************************************************************
  Plugin API. Common for all plugin types.
*/


class sys_var;
typedef drizzle_lex_string LEX_STRING;
struct option;

extern boost::filesystem::path plugin_dir;

namespace plugin { class StorageEngine; }

/*
  Macros for beginning and ending plugin declarations. Between
  DRIZZLE_DECLARE_PLUGIN and DRIZZLE_DECLARE_PLUGIN_END there should
  be a module::Manifest for each plugin to be declared.
*/


#define PANDORA_CPP_NAME(x) _drizzled_ ## x ## _plugin_
#define PANDORA_PLUGIN_NAME(x) PANDORA_CPP_NAME(x)
#define DRIZZLE_DECLARE_PLUGIN \
  ::drizzled::module::Manifest PANDORA_PLUGIN_NAME(PANDORA_MODULE_NAME)= 


#define DRIZZLE_DECLARE_PLUGIN_END
#define DRIZZLE_PLUGIN(init,system,options) \
  DRIZZLE_DECLARE_PLUGIN \
  { \
    DRIZZLE_VERSION_ID, \
    STRINGIFY_ARG(PANDORA_MODULE_NAME), \
    STRINGIFY_ARG(PANDORA_MODULE_VERSION), \
    STRINGIFY_ARG(PANDORA_MODULE_AUTHOR), \
    STRINGIFY_ARG(PANDORA_MODULE_TITLE), \
    PANDORA_MODULE_LICENSE, \
    init, system, options \
  } 


/*
  declarations for SHOW STATUS support in plugins
*/
enum enum_mysql_show_type
{
  SHOW_UNDEF, SHOW_BOOL, SHOW_INT, SHOW_LONG,
  SHOW_LONGLONG, SHOW_CHAR, SHOW_CHAR_PTR,
  SHOW_FUNC,
  SHOW_LONG_STATUS, SHOW_DOUBLE_STATUS,
  SHOW_MY_BOOL, SHOW_HA_ROWS, SHOW_SYS, SHOW_INT_NOFLUSH,
  SHOW_LONGLONG_STATUS, SHOW_DOUBLE, SHOW_SIZE
};

struct drizzle_show_var {
  const char *name;
  char *value;
  enum enum_mysql_show_type type;
};

typedef enum enum_mysql_show_type SHOW_TYPE;


#define SHOW_VAR_FUNC_BUFF_SIZE 1024
typedef int (*mysql_show_var_func)(drizzle_show_var *, char *);

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
#define PLUGIN_VAR_UNSIGNED     0x0080
#define PLUGIN_VAR_SessionLOCAL     0x0100 /* Variable is per-connection */
#define PLUGIN_VAR_READONLY     0x0200 /* Server variable is read only */
#define PLUGIN_VAR_NOSYSVAR     0x0400 /* Not a server variable */
#define PLUGIN_VAR_NOCMDOPT     0x0800 /* Not a command line option */
#define PLUGIN_VAR_NOCMDARG     0x1000 /* No argument for cmd line */
#define PLUGIN_VAR_RQCMDARG     0x0000 /* Argument required for cmd line */
#define PLUGIN_VAR_OPCMDARG     0x2000 /* Argument optional for cmd line */
#define PLUGIN_VAR_MEMALLOC     0x8000 /* String needs memory allocated */

struct drizzle_sys_var;
struct drizzle_value;

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
                                    drizzle_sys_var *var,
                                    void *save, drizzle_value *value);

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
                                      drizzle_sys_var *var,
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

#define DRIZZLE_SYSVAR_NAME(name) drizzle_sysvar_ ## name
#define DRIZZLE_SYSVAR(name) \
  ((drizzle_sys_var *)(&(DRIZZLE_SYSVAR_NAME(name))))

/*
  for global variables, the value pointer is the first
  element after the header, the default value is the second.
  for thread variables, the value offset is the first
  element after the header, the default value is the second.
*/


#define DECLARE_DRIZZLE_SYSVAR_BOOL(name) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  bool *value;                  \
  bool def_val;           \
} DRIZZLE_SYSVAR_NAME(name)

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

#define DECLARE_SessionVAR_FUNC(type) \
  type *(*resolve)(Session *session, int offset)

#define DECLARE_DRIZZLE_SessionVAR_BASIC(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  const type def_val;           \
  DECLARE_SessionVAR_FUNC(type);    \
} DRIZZLE_SYSVAR_NAME(name)

#define DECLARE_DRIZZLE_SessionVAR_BOOL(name) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  int offset;                   \
  bool def_val;           \
  DECLARE_SessionVAR_FUNC(bool);    \
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

#define DECLARE_DRIZZLE_SYSVAR_BOOL(name) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  bool *value;                  \
  bool def_val;           \
} DRIZZLE_SYSVAR_NAME(name)


#define DRIZZLE_SYSVAR_BOOL(name, varname, opt, comment, check, update, def) \
  DECLARE_DRIZZLE_SYSVAR_BOOL(name) = { \
  PLUGIN_VAR_BOOL | ((opt) & PLUGIN_VAR_MASK), \
  #name, comment, check, update, &varname, def}

#define DECLARE_DRIZZLE_SYSVAR_BASIC(name, type) struct { \
  DRIZZLE_PLUGIN_VAR_HEADER;      \
  type *value;                  \
  const type def_val;           \
} DRIZZLE_SYSVAR_NAME(name)

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

#define DRIZZLE_SessionVAR_BOOL(name, opt, comment, check, update, def) \
DECLARE_DRIZZLE_SessionVAR_BOOL(name) = { \
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

/* accessor macros */

#define SYSVAR(name) \
  (*(DRIZZLE_SYSVAR_NAME(name).value))

/* when session == null, result points to global value */
#define SessionVAR(session, name) \
  (*(DRIZZLE_SYSVAR_NAME(name).resolve(session, DRIZZLE_SYSVAR_NAME(name).offset)))


/*************************************************************************
  drizzle_value struct for reading values from mysqld.
  Used by server variables framework to parse user-provided values.
  Will be used for arguments when implementing UDFs.

  Note that val_str() returns a string in temporary memory
  that will be freed at the end of statement. Copy the string
  if you need it to persist.
*/

#define DRIZZLE_VALUE_TYPE_STRING 0
#define DRIZZLE_VALUE_TYPE_REAL   1
#define DRIZZLE_VALUE_TYPE_INT    2

/*
  skeleton of a plugin variable - portion of structure common to all.
*/
struct drizzle_sys_var
{
  DRIZZLE_PLUGIN_VAR_HEADER;
};

void plugin_opt_set_limits(option *options, const drizzle_sys_var *opt);

struct drizzle_value
{
  int (*value_type)(drizzle_value *);
  const char *(*val_str)(drizzle_value *, char *buffer, int *length);
  int (*val_real)(drizzle_value *, double *realbuf);
  int (*val_int)(drizzle_value *, int64_t *intbuf);
};


/*************************************************************************
  Miscellaneous functions for plugin implementors
*/

extern bool plugin_init(module::Registry &registry,
                        boost::program_options::options_description &long_options);
extern bool plugin_finalize(module::Registry &registry);
extern void my_print_help_inc_plugins(option *options);
extern bool plugin_is_ready(const LEX_STRING *name, int type);
extern void plugin_sessionvar_init(Session *session);
extern void plugin_sessionvar_cleanup(Session *session);
extern sys_var *intern_find_sys_var(const char *str, uint32_t, bool no_error);

int session_in_lock_tables(const Session *session);
int session_tablespace_op(const Session *session);
void set_session_proc_info(Session *session, const char *info);
const char *get_session_proc_info(Session *session);
int64_t session_test_options(const Session *session, int64_t test_options);
int session_sql_command(const Session *session);
enum_tx_isolation session_tx_isolation(const Session *session);

void compose_plugin_add(std::vector<std::string> options);
void compose_plugin_remove(std::vector<std::string> options);
void notify_plugin_load(std::string in_plugin_load);


/**
  Create a temporary file.

  @details
  The temporary file is created in a location specified by the mysql
  server configuration (--tmpdir option).  The caller does not need to
  delete the file, it will be deleted automatically.

  @param prefix  prefix for temporary file name
  @retval -1    error
  @retval >= 0  a file handle that can be passed to dup or internal::my_close
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


const charset_info_st *session_charset(Session *session);

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

} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_H */

