/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <drizzled/module/manifest.h>
#include <drizzled/module/module.h>
#include <drizzled/plugin/version.h>
#include <drizzled/module/context.h>
#include <drizzled/definitions.h>

#include <drizzled/lex_string.h>
#include <drizzled/sys_var.h>

#include <drizzled/visibility.h>

namespace drizzled {

/*************************************************************************
  Plugin API. Common for all plugin types.
*/

extern boost::filesystem::path plugin_dir;

/*
  Macros for beginning and ending plugin declarations. Between
  DRIZZLE_DECLARE_PLUGIN and DRIZZLE_DECLARE_PLUGIN_END there should
  be a module::Manifest for each plugin to be declared.
*/


#define PANDORA_CPP_NAME(x) _drizzled_ ## x ## _plugin_
#define PANDORA_PLUGIN_NAME(x) PANDORA_CPP_NAME(x)
#define DRIZZLE_DECLARE_PLUGIN \
  DRIZZLED_API ::drizzled::module::Manifest PANDORA_PLUGIN_NAME(PANDORA_MODULE_NAME)= 


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
    init, \
    STRINGIFY_ARG(PANDORA_MODULE_DEPENDENCIES), \
    options \
  } 


/*
  declarations for server variables and command line options
*/


#define PLUGIN_VAR_READONLY     0x0200 /* Server variable is read only */
#define PLUGIN_VAR_OPCMDARG     0x2000 /* Argument optional for cmd line */
#define PLUGIN_VAR_MEMALLOC     0x8000 /* String needs memory allocated */

struct drizzle_sys_var;
struct drizzle_value;

/*
  SYNOPSIS
    (*var_check_func)()
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

typedef int (*var_check_func)(Session *session,
                                    drizzle_sys_var *var,
                                    void *save, drizzle_value *value);

/*
  SYNOPSIS
    (*var_update_func)()
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
typedef void (*var_update_func)(Session *session,
                                      drizzle_sys_var *var,
                                      void *var_ptr, const void *save);



/*
  skeleton of a plugin variable - portion of structure common to all.
*/
struct drizzle_sys_var
{
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
extern void plugin_startup_window(module::Registry &registry, drizzled::Session &session);
extern void my_print_help_inc_plugins(option *options);
extern bool plugin_is_ready(const LEX_STRING *name, int type);
extern void plugin_sessionvar_init(Session *session);
extern void plugin_sessionvar_cleanup(Session *session);

int session_in_lock_tables(const Session *session);
DRIZZLED_API int64_t session_test_options(const Session *session, int64_t test_options);
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
DRIZZLED_API int tmpfile(const char *prefix);

} /* namespace drizzled */


