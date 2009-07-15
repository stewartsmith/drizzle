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
#include <drizzled/plugin_registry.h>
#include <drizzled/plugin.h>

class sys_var;
class Session;


namespace drizzled {
namespace plugin {

typedef int (*plugin_type_init)(PluginRegistry &);

/*
  Plugin description structure.
*/

struct Manifest
{
  /* Hide these - we will not use them */
/*  Manifest(const Manifest&);
  Manifest operator=(const Manifest&);
public:*/
  const char *name;          /* plugin name (for SHOW PLUGINS)               */
  const char *version;       /* plugin version (for SHOW PLUGINS)            */
  const char *author;        /* plugin author (for SHOW PLUGINS)             */
  const char *descr;         /* general descriptive text (for SHOW PLUGINS ) */
  plugin_license_type license; /* plugin license (PLUGIN_LICENSE_XXX)          */
  plugin_type_init init;     /* function to invoke when plugin is loaded     */
  plugin_type_init deinit;   /* function to invoke when plugin is unloaded   */
  st_mysql_show_var *status_vars;
  st_mysql_sys_var **system_vars;
  void* reserved1;
/*  Manifest(name)
    : name(NULL), version(NULL), author(NULL), descr(NULL),
      license(PLUGIN_LICENSE_GPL), init(NULL), deinit(NULL),
      status_vars(NULL), system_vars(NULL) {} */
};

/* A handle for the dynamic library containing a plugin or plugins. */
class Library
{
public:
  LEX_STRING dl;
  void *handle;
  Manifest *plugins;
  Library() : dl(), handle(NULL), plugins(NULL) {}
};

/* A handle of a plugin */
class Handle
{
public:
  LEX_STRING name;
  Manifest *plugin;
  Library *plugin_dl;
  bool isInited;
  MEM_ROOT mem_root;            /* memory for dynamic plugin structures */
  sys_var *system_vars;         /* server variables for this plugin */
  Handle()
    : name(), plugin(NULL), plugin_dl(NULL), isInited(false), 
      mem_root(), system_vars(NULL) {}
};

} /* namespace plugin */
} /* namespace drizzled */

#define plugin_decl(pi) ((pi)->plugin)
#define plugin_data(pi,cast) (static_cast<cast>((pi)->data))
#define plugin_name(pi) (&((pi)->name))
#define plugin_equals(p1,p2) ((p1) && (p2) && (p1) == (p2))


extern char *opt_plugin_load;
extern char *opt_plugin_dir_ptr;
extern char opt_plugin_dir[FN_REFLEN];

extern int plugin_init(int *argc, char **argv, int init_flags);
extern void plugin_shutdown(void);
extern void my_print_help_inc_plugins(struct my_option *options, uint32_t size);
extern bool plugin_is_ready(const LEX_STRING *name, int type);
extern bool mysql_install_plugin(Session *session, const LEX_STRING *name,
                                 const LEX_STRING *dl);
extern bool mysql_uninstall_plugin(Session *session, const LEX_STRING *name);
extern bool plugin_register_builtin(drizzled::plugin::Manifest *plugin);
extern void plugin_sessionvar_init(Session *session);
extern void plugin_sessionvar_cleanup(Session *session);
extern sys_var *intern_find_sys_var(const char *str, uint32_t, bool no_error);

#endif /* DRIZZLE_SERVER_PLUGIN_H */
