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

/**
 * @TODO There is plugin.h and also sql_plugin.h.  Ostensibly,
 * it seems that the two files exist so that plugin.h can provide an
 * external API for plugin developers and sql_plugin.h will provide
 * and internal server API for dealing with those plugins.
 *
 * However, there are parts of plugin.h marked "INTERNAL USE ONLY" which
 * seems to contradict the above...
 *
 * Let's figure out a better way of dividing the public and internal API
 * and name the files more appropriately.
 *
 * Also, less #defines, more enums and bitmaps...
 *
 */

#ifndef DRIZZLE_SERVER_PLUGIN_H
#define DRIZZLE_SERVER_PLUGIN_H

#include <drizzled/lex_string.h>
#include <mysys/my_alloc.h>

class sys_var;
class Session;

/*
  the following flags are valid for plugin_init()
*/
#define PLUGIN_INIT_SKIP_DYNAMIC_LOADING 1
#define PLUGIN_INIT_SKIP_PLUGIN_TABLE    2
#define PLUGIN_INIT_SKIP_INITIALIZATION  4

#define INITIAL_LEX_PLUGIN_LIST_SIZE    16

/*
  the following #define adds server-only members to enum_mysql_show_type,
  that is defined in plugin.h
*/
#define SHOW_FUNC    SHOW_FUNC, SHOW_KEY_CACHE_LONG, SHOW_KEY_CACHE_LONGLONG, \
                     SHOW_LONG_STATUS, SHOW_DOUBLE_STATUS, SHOW_HAVE,   \
                     SHOW_MY_BOOL, SHOW_HA_ROWS, SHOW_SYS, SHOW_INT_NOFLUSH, \
                     SHOW_LONGLONG_STATUS
#include <drizzled/plugin.h>
#undef SHOW_FUNC
typedef enum enum_mysql_show_type SHOW_TYPE;
typedef struct st_mysql_show_var SHOW_VAR;

#define DRIZZLE_ANY_PLUGIN         -1

/* A handle for the dynamic library containing a plugin or plugins. */
struct st_mysql_plugin;

struct st_plugin_dl
{
  LEX_STRING dl;
  void *handle;
  struct st_mysql_plugin *plugins;
};

/* A handle of a plugin */

struct st_plugin_int
{
  LEX_STRING name;
  struct st_mysql_plugin *plugin;
  struct st_plugin_dl *plugin_dl;
  bool isInited;
  void *data;                   /* plugin type specific, e.g. StorageEngine */
  MEM_ROOT mem_root;            /* memory for dynamic plugin structures */
  sys_var *system_vars;         /* server variables for this plugin */
};


#define plugin_decl(pi) ((pi)->plugin)
#define plugin_dlib(pi) ((pi)->plugin_dl)
#define plugin_data(pi,cast) (static_cast<cast>((pi)->data))
#define plugin_name(pi) (&((pi)->name))
#define plugin_equals(p1,p2) ((p1) && (p2) && (p1) == (p2))

#include <drizzled/plugin_registry.h>

typedef int (*plugin_type_init)(PluginRegistry &);

/*
  Plugin description structure.
*/

struct st_mysql_plugin
{
  uint32_t type;             /* plugin type (a DRIZZLE_XXX_PLUGIN value)     */
  const char *name;          /* plugin name (for SHOW PLUGINS)               */
  const char *version;       /* plugin version (for SHOW PLUGINS)            */
  const char *author;        /* plugin author (for SHOW PLUGINS)             */
  const char *descr;         /* general descriptive text (for SHOW PLUGINS ) */
  int license;               /* plugin license (PLUGIN_LICENSE_XXX)          */
  plugin_type_init init;     /* function to invoke when plugin is loaded     */
  plugin_type_init deinit;   /* function to invoke when plugin is unloaded   */
  struct st_mysql_show_var *status_vars;
  struct st_mysql_sys_var **system_vars;
  void *reserved1;           /* reserved for dependency checking             */
};

extern char *opt_plugin_load;
extern char *opt_plugin_dir_ptr;
extern char opt_plugin_dir[FN_REFLEN];
extern const LEX_STRING plugin_type_names[];

extern int plugin_init(int *argc, char **argv, int init_flags);
extern void plugin_shutdown(void);
extern void my_print_help_inc_plugins(struct my_option *options, uint32_t size);
extern bool plugin_is_ready(const LEX_STRING *name, int type);
extern st_plugin_int *plugin_lock_by_name(const LEX_STRING *name, int type);
extern bool mysql_install_plugin(Session *session, const LEX_STRING *name,
                                 const LEX_STRING *dl);
extern bool mysql_uninstall_plugin(Session *session, const LEX_STRING *name);
extern bool plugin_register_builtin(struct st_mysql_plugin *plugin);
extern void plugin_sessionvar_init(Session *session);
extern void plugin_sessionvar_cleanup(Session *session);

typedef bool (plugin_foreach_func)(Session *session, st_plugin_int *plugin, void *arg);
bool plugin_foreach(Session *session, plugin_foreach_func *func,
                    int type, void *arg, bool all= false);

#endif /* DRIZZLE_SERVER_PLUGIN_H */
