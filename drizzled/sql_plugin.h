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

/*
  different values of st_plugin_int::state
  though they look like a bitmap, plugin may only
  be in one of those eigenstates, not in a superposition of them :)
  It's a bitmap, because it makes it easier to test
  "whether the state is one of those..."
*/
#define PLUGIN_IS_FREED         1
#define PLUGIN_IS_DELETED       2
#define PLUGIN_IS_UNINITIALIZED 4
#define PLUGIN_IS_READY         8
#define PLUGIN_IS_DYING         16

/* A handle for the dynamic library containing a plugin or plugins. */

struct st_plugin_dl
{
  LEX_STRING dl;
  void *handle;
  struct st_mysql_plugin *plugins;
  uint32_t ref_count;            /* number of plugins loaded from the library */
};

/* A handle of a plugin */

struct st_plugin_int
{
  LEX_STRING name;
  struct st_mysql_plugin *plugin;
  struct st_plugin_dl *plugin_dl;
  uint32_t state;
  uint32_t ref_count;               /* number of threads using the plugin */
  void *data;                   /* plugin type specific, e.g. handlerton */
  MEM_ROOT mem_root;            /* memory for dynamic plugin structures */
  sys_var *system_vars;         /* server variables for this plugin */
};


/*
  See intern_plugin_lock() for the explanation for the
  conditionally defined plugin_ref type
*/
typedef struct st_plugin_int **plugin_ref;
#define plugin_decl(pi) ((pi)[0]->plugin)
#define plugin_dlib(pi) ((pi)[0]->plugin_dl)
#define plugin_data(pi,cast) (static_cast<cast>((pi)[0]->data))
#define plugin_name(pi) (&((pi)[0]->name))
#define plugin_state(pi) ((pi)[0]->state)
#define plugin_equals(p1,p2) ((p1) && (p2) && (p1)[0] == (p2)[0])

typedef int (*plugin_type_init)(struct st_plugin_int *);

extern char *opt_plugin_load;
extern char *opt_plugin_dir_ptr;
extern char opt_plugin_dir[FN_REFLEN];
extern const LEX_STRING plugin_type_names[];

extern int plugin_init(int *argc, char **argv, int init_flags);
extern void plugin_shutdown(void);
extern void my_print_help_inc_plugins(struct my_option *options, uint32_t size);
extern bool plugin_is_ready(const LEX_STRING *name, int type);
#define my_plugin_lock_by_name(A,B,C) plugin_lock_by_name(A,B,C)
#define my_plugin_lock_by_name_ci(A,B,C) plugin_lock_by_name(A,B,C)
#define my_plugin_lock(A,B) plugin_lock(A,B)
#define my_plugin_lock_ci(A,B) plugin_lock(A,B)
extern plugin_ref plugin_lock(Session *session, plugin_ref *ptr);
extern plugin_ref plugin_lock_by_name(Session *session, const LEX_STRING *name,
                                      int type);
extern void plugin_unlock(Session *session, plugin_ref plugin);
extern void plugin_unlock_list(Session *session, plugin_ref *list,
                               uint32_t count);
extern bool mysql_install_plugin(Session *session, const LEX_STRING *name,
                                 const LEX_STRING *dl);
extern bool mysql_uninstall_plugin(Session *session, const LEX_STRING *name);
extern bool plugin_register_builtin(struct st_mysql_plugin *plugin);
extern void plugin_sessionvar_init(Session *session);
extern void plugin_sessionvar_cleanup(Session *session);

typedef bool (plugin_foreach_func)(Session *session,
                                   plugin_ref plugin,
                                   void *arg);
bool plugin_foreach(Session *session, plugin_foreach_func *func,
                    int type, void *arg,
                    uint32_t state_mask= PLUGIN_IS_READY);

#endif /* DRIZZLE_SERVER_PLUGIN_H */
