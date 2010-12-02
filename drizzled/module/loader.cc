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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "config.h"

#include <dlfcn.h>

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>

#include <boost/program_options.hpp>

#include "drizzled/option.h"
#include "drizzled/internal/m_string.h"

#include "drizzled/plugin.h"
#include "drizzled/module/load_list.h"
#include "drizzled/module/library.h"
#include "drizzled/module/registry.h"
#include "drizzled/module/option_context.h"
#include "drizzled/sql_parse.h"
#include "drizzled/show.h"
#include "drizzled/cursor.h"
#include "drizzled/set_var.h"
#include "drizzled/session.h"
#include "drizzled/item/null.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"
#include "drizzled/errmsg_print.h"
#include "drizzled/strfunc.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/util/tokenize.h"

#include <boost/foreach.hpp>

/* FreeBSD 2.2.2 does not define RTLD_NOW) */
#ifndef RTLD_NOW
#define RTLD_NOW 1
#endif

namespace po=boost::program_options;

using namespace std;

/** These exist just to prevent symbols from being optimized out */
typedef drizzled::module::Manifest drizzled_builtin_list[];
extern drizzled_builtin_list PANDORA_BUILTIN_SYMBOLS_LIST;
extern drizzled_builtin_list PANDORA_BUILTIN_LOAD_SYMBOLS_LIST;
drizzled::module::Manifest *drizzled_builtins[]=
{
  PANDORA_BUILTIN_SYMBOLS_LIST, NULL
};
drizzled::module::Manifest *drizzled_load_builtins[]=
{
  PANDORA_BUILTIN_LOAD_SYMBOLS_LIST, NULL
};

namespace drizzled
{
 

class sys_var_pluginvar;
static vector<sys_var_pluginvar *> plugin_sysvar_vec;

typedef vector<string> PluginOptions;
static PluginOptions opt_plugin_load;
static PluginOptions opt_plugin_add;
static PluginOptions opt_plugin_remove;
const char *builtin_plugins= PANDORA_BUILTIN_LIST;
const char *builtin_load_plugins= PANDORA_BUILTIN_LOAD_LIST;

/* Note that 'int version' must be the first field of every plugin
   sub-structure (plugin->info).
*/

static bool initialized= false;


static bool reap_needed= false;

/*
  write-lock on LOCK_system_variables_hash is required before modifying
  the following variables/structures
*/
static memory::Root plugin_mem_root(4096);
static uint32_t global_variables_dynamic_size= 0;


/*
  hidden part of opaque value passed to variable check functions.
  Used to provide a object-like structure to non C++ consumers.
*/
struct st_item_value_holder : public drizzle_value
{
  Item *item;
};

class Bookmark
{
public:
  Bookmark() :
    type_code(0),
    offset(0),
    version(0),
    key("")
  {}
  uint8_t type_code;
  int offset;
  uint32_t version;
  string key;
};

typedef boost::unordered_map<string, Bookmark> bookmark_unordered_map;
static bookmark_unordered_map bookmark_hash;


/*
  sys_var class for access to all plugin variables visible to the user
*/
class sys_var_pluginvar: public sys_var
{
public:
  module::Module *plugin;
  drizzle_sys_var *plugin_var;

  sys_var_pluginvar(const std::string name_arg,
                    drizzle_sys_var *plugin_var_arg)
    :sys_var(name_arg), plugin_var(plugin_var_arg) {}
  sys_var_pluginvar *cast_pluginvar() { return this; }
  bool is_readonly() const { return plugin_var->flags & PLUGIN_VAR_READONLY; }
  bool check_type(sql_var_t type)
  { return !(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) && type != OPT_GLOBAL; }
  bool check_update_type(Item_result type);
  SHOW_TYPE show_type();
  unsigned char* real_value_ptr(Session *session, sql_var_t type);
  TYPELIB* plugin_var_typelib(void);
  unsigned char* value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
  bool check(Session *session, set_var *var);
  bool check_default(sql_var_t)
    { return is_readonly(); }
  void set_default(Session *session, sql_var_t);
  bool update(Session *session, set_var *var);
};


/* prototypes */
static void plugin_prune_list(vector<string> &plugin_list,
                              const vector<string> &plugins_to_remove);
static bool plugin_load_list(module::Registry &registry,
                             memory::Root *tmp_root,
                             const set<string> &plugin_list,
                             po::options_description &long_options,
                             bool builtin= false);
static int test_plugin_options(memory::Root *, module::Module *,
                               po::options_description &long_options);
static void unlock_variables(Session *session, drizzle_system_variables *vars);
static void cleanup_variables(drizzle_system_variables *vars);
static void plugin_vars_free_values(module::Module::Variables &vars);

/* declared in set_var.cc */
extern sys_var *intern_find_sys_var(const char *str, uint32_t length, bool no_error);
extern bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                                 const std::string &name, int64_t val);

static bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                                 const char *name, int64_t val)
{
  const std::string name_str(name);
  return throw_bounds_warning(session, fixed, unsignd, name_str, val);
}

/****************************************************************************
  Value type thunks, allows the C world to play in the C++ world
****************************************************************************/

static int item_value_type(drizzle_value *value)
{
  switch (((st_item_value_holder*)value)->item->result_type()) {
  case INT_RESULT:
    return DRIZZLE_VALUE_TYPE_INT;
  case REAL_RESULT:
    return DRIZZLE_VALUE_TYPE_REAL;
  default:
    return DRIZZLE_VALUE_TYPE_STRING;
  }
}

static const char *item_val_str(drizzle_value *value,
                                char *buffer, int *length)
{
  String str(buffer, *length, system_charset_info), *res;
  if (!(res= ((st_item_value_holder*)value)->item->val_str(&str)))
    return NULL;
  *length= res->length();
  if (res->c_ptr_quick() == buffer)
    return buffer;

  /*
    Lets be nice and create a temporary string since the
    buffer was too small
  */
  return current_session->strmake(res->c_ptr_quick(), res->length());
}


static int item_val_int(drizzle_value *value, int64_t *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_int();
  if (item->is_null())
    return 1;
  return 0;
}


static int item_val_real(drizzle_value *value, double *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_real();
  if (item->is_null())
    return 1;
  return 0;
}


/****************************************************************************
  Plugin support code
****************************************************************************/




/*
  NOTE
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static bool plugin_add(module::Registry &registry, memory::Root *tmp_root,
                       module::Library *library,
                       po::options_description &long_options)
{
  if (! initialized)
    return true;

  if (registry.find(library->getName()))
  {
    errmsg_printf(ERRMSG_LVL_WARN, ER(ER_PLUGIN_EXISTS),
                  library->getName().c_str());
    return false;
  }

  module::Module *tmp= NULL;
  /* Find plugin by name */
  const module::Manifest *manifest= library->getManifest();

  if (registry.find(manifest->name))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Plugin '%s' contains the name '%s' in its manifest, which "
                    "has already been registered.\n"),
                  library->getName().c_str(),
                  manifest->name);
    return true;
  }

  tmp= new (std::nothrow) module::Module(manifest, library);
  if (tmp == NULL)
    return true;

  if (!test_plugin_options(tmp_root, tmp, long_options))
  {
    registry.add(tmp);
    return false;
  }
  errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_CANT_FIND_DL_ENTRY),
                library->getName().c_str());
  return true;
}


static void delete_module(module::Module *module)
{
  /* Free allocated strings before deleting the plugin. */
  plugin_vars_free_values(module->getSysVars());
  module->isInited= false;
  delete module;
}


static void reap_plugins(module::Registry &registry)
{
  std::map<std::string, module::Module *>::const_iterator modules=
    registry.getModulesMap().begin();

  while (modules != registry.getModulesMap().end())
  {
    module::Module *module= (*modules).second;
    delete_module(module);
    ++modules;
  }

  drizzle_del_plugin_sysvar();
}


static void plugin_initialize_vars(module::Module *module)
{
  /*
    set the plugin attribute of plugin's sys vars so they are pointing
    to the active plugin
  */
  for (module::Module::Variables::iterator iter= module->getSysVars().begin();
       iter != module->getSysVars().end();
       ++iter)
  {
    sys_var *current_var= *iter;
    current_var->cast_pluginvar()->plugin= module;
  }
}


static bool plugin_initialize(module::Registry &registry,
                              module::Module *module)
{
  assert(module->isInited == false);

  module::Context loading_context(registry, module);
  if (module->getManifest().init)
  {
    if (module->getManifest().init(loading_context))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Plugin '%s' init function returned error.\n"),
                    module->getName().c_str());
      return true;
    }
  }
  module->isInited= true;


  return false;
}


inline static void dashes_to_underscores(std::string &name_in,
                                         char from= '-', char to= '_')
{
  for (string::iterator p= name_in.begin();
       p != name_in.end();
       ++p)
  {
    if (*p == from)
    {
      *p= to;
    }
  }
}

inline static void underscores_to_dashes(std::string &name_in)
{
  return dashes_to_underscores(name_in, '_', '-');
}

static void compose_plugin_options(vector<string> &target,
                                   vector<string> options)
{
  for (vector<string>::iterator it= options.begin();
       it != options.end();
       ++it)
  {
    tokenize(*it, target, ",", true);
  }
  for (vector<string>::iterator it= target.begin();
       it != target.end();
       ++it)
  {
    dashes_to_underscores(*it);
  }
}

void compose_plugin_add(vector<string> options)
{
  compose_plugin_options(opt_plugin_add, options);
}

void compose_plugin_remove(vector<string> options)
{
  compose_plugin_options(opt_plugin_remove, options);
}

void notify_plugin_load(string in_plugin_load)
{
  tokenize(in_plugin_load, opt_plugin_load, ",", true);
}

/*
  The logic is that we first load and initialize all compiled in plugins.
  From there we load up the dynamic types (assuming we have not been told to
  skip this part).

  Finally we initialize everything, aka the dynamic that have yet to initialize.
*/
bool plugin_init(module::Registry &registry,
                 po::options_description &long_options)
{
  memory::Root tmp_root(4096);

  if (initialized)
    return false;

  initialized= 1;

  PluginOptions builtin_load_list;
  tokenize(builtin_load_plugins, builtin_load_list, ",", true);

  PluginOptions builtin_list;
  tokenize(builtin_plugins, builtin_list, ",", true);

  bool load_failed= false;

  if (opt_plugin_add.size() > 0)
  {
    for (PluginOptions::iterator iter= opt_plugin_add.begin();
         iter != opt_plugin_add.end();
         ++iter)
    {
      if (find(builtin_list.begin(),
               builtin_list.end(), *iter) != builtin_list.end())
      {
        builtin_load_list.push_back(*iter);
      }
      else
      {
        opt_plugin_load.push_back(*iter);
      }
    }
  }

  if (opt_plugin_remove.size() > 0)
  {
    plugin_prune_list(opt_plugin_load, opt_plugin_remove);
    plugin_prune_list(builtin_load_list, opt_plugin_remove);
  }


  /*
    First we register builtin plugins
  */
  const set<string> builtin_list_set(builtin_load_list.begin(),
                                     builtin_load_list.end());
  load_failed= plugin_load_list(registry, &tmp_root,
                                builtin_list_set, long_options, true);
  if (load_failed)
  {
    tmp_root.free_root(MYF(0));
    return true;
  }

  /* Uniquify the list */
  const set<string> plugin_list_set(opt_plugin_load.begin(),
                                    opt_plugin_load.end());
  
  /* Register all dynamic plugins */
  load_failed= plugin_load_list(registry, &tmp_root,
                                plugin_list_set, long_options);
  if (load_failed)
  {
    tmp_root.free_root(MYF(0));
    return true;
  }

  tmp_root.free_root(MYF(0));

  return false;
}

bool plugin_finalize(module::Registry &registry)
{
  /*
    Now we initialize all remaining plugins
  */
  std::map<std::string, module::Module *>::const_iterator modules=
    registry.getModulesMap().begin();
    
  while (modules != registry.getModulesMap().end())
  {
    module::Module *module= (*modules).second;
    ++modules;
    if (module->isInited == false)
    {
      plugin_initialize_vars(module);

      if (plugin_initialize(registry, module))
      {
        registry.remove(module);
        delete_module(module);
        return true;
      }
    }
  }

  BOOST_FOREACH(plugin::Plugin::map::value_type value, registry.getPluginsMap())
  {
    value.second->prime();
  }

  return false;
}

class PrunePlugin :
  public unary_function<string, bool>
{
  const string to_match;
  PrunePlugin();
  PrunePlugin& operator=(const PrunePlugin&);
public:
  explicit PrunePlugin(const string &match_in) :
    to_match(match_in)
  { }

  result_type operator()(const string &match_against)
  {
    return match_against == to_match;
  }
};

static void plugin_prune_list(vector<string> &plugin_list,
                              const vector<string> &plugins_to_remove)
{
  for (vector<string>::const_iterator iter= plugins_to_remove.begin();
       iter != plugins_to_remove.end();
       ++iter)
  {
    plugin_list.erase(remove_if(plugin_list.begin(),
                                plugin_list.end(),
                                PrunePlugin(*iter)),
                      plugin_list.end());
  }
}

/*
  called only by plugin_init()
*/
static bool plugin_load_list(module::Registry &registry,
                             memory::Root *tmp_root,
                             const set<string> &plugin_list,
                             po::options_description &long_options,
                             bool builtin)
{
  module::Library *library= NULL;

  for (set<string>::const_iterator iter= plugin_list.begin();
       iter != plugin_list.end();
       ++iter)
  {
    const string plugin_name(*iter);

    library= registry.addLibrary(plugin_name, builtin);
    if (library == NULL)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Couldn't load plugin library named '%s'.\n"),
                    plugin_name.c_str());
      return true;
    }

    tmp_root->free_root(MYF(memory::MARK_BLOCKS_FREE));
    if (plugin_add(registry, tmp_root, library, long_options))
    {
      registry.removeLibrary(plugin_name);
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Couldn't load plugin named '%s'.\n"),
                    plugin_name.c_str());
      return true;

    }
  }
  return false;
}


void module_shutdown(module::Registry &registry)
{

  if (initialized)
  {
    reap_needed= true;

    reap_plugins(registry);
    unlock_variables(NULL, &global_system_variables);
    unlock_variables(NULL, &max_system_variables);

    cleanup_variables(&global_system_variables);
    cleanup_variables(&max_system_variables);

    initialized= 0;
  }

  /* Dispose of the memory */
  plugin_mem_root.free_root(MYF(0));

  global_variables_dynamic_size= 0;
}

/****************************************************************************
  Internal type declarations for variables support
****************************************************************************/

#undef DRIZZLE_SYSVAR_NAME
#define DRIZZLE_SYSVAR_NAME(name) name
#define PLUGIN_VAR_TYPEMASK 0x007f

static const uint32_t EXTRA_OPTIONS= 1; /* handle the NULL option */

typedef DECLARE_DRIZZLE_SYSVAR_BOOL(sysvar_bool_t);
typedef DECLARE_DRIZZLE_SessionVAR_BOOL(sessionvar_bool_t);
typedef DECLARE_DRIZZLE_SYSVAR_BASIC(sysvar_str_t, char *);
typedef DECLARE_DRIZZLE_SessionVAR_BASIC(sessionvar_str_t, char *);

typedef DECLARE_DRIZZLE_SessionVAR_TYPELIB(sessionvar_enum_t, unsigned long);
typedef DECLARE_DRIZZLE_SessionVAR_TYPELIB(sessionvar_set_t, uint64_t);

typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_int_t, int);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_long_t, long);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_int64_t_t, int64_t);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_uint_t, uint);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_ulong_t, ulong);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_uint64_t_t, uint64_t);

typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_int_t, int);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_long_t, long);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_int64_t_t, int64_t);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_uint_t, uint);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_ulong_t, ulong);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_uint64_t_t, uint64_t);

typedef bool *(*mysql_sys_var_ptr_p)(Session* a_session, int offset);


/****************************************************************************
  default variable data check and update functions
****************************************************************************/

static int check_func_bool(Session *, drizzle_sys_var *var,
                           void *save, drizzle_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *strvalue= "NULL", *str;
  int result, length;
  int64_t tmp;

  if (value->value_type(value) == DRIZZLE_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)) ||
        (result= find_type(&bool_typelib, str, length, 1)-1) < 0)
    {
      if (str)
        strvalue= str;
      goto err;
    }
  }
  else
  {
    if (value->val_int(value, &tmp) < 0)
      goto err;
    if (tmp > 1)
    {
      internal::llstr(tmp, buff);
      strvalue= buff;
      goto err;
    }
    result= (int) tmp;
  }
  *(int*)save= -result;
  return 0;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, strvalue);
  return 1;
}


static int check_func_int(Session *session, drizzle_sys_var *var,
                          void *save, drizzle_value *value)
{
  bool fixed;
  int64_t tmp;
  struct option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
    *(uint32_t *)save= (uint32_t) getopt_ull_limit_value((uint64_t) tmp, &options,
                                                   &fixed);
  else
    *(int *)save= (int) getopt_ll_limit_value(tmp, &options, &fixed);

  return throw_bounds_warning(session, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}


static int check_func_long(Session *session, drizzle_sys_var *var,
                          void *save, drizzle_value *value)
{
  bool fixed;
  int64_t tmp;
  struct option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
    *(ulong *)save= (ulong) getopt_ull_limit_value((uint64_t) tmp, &options,
                                                   &fixed);
  else
    *(long *)save= (long) getopt_ll_limit_value(tmp, &options, &fixed);

  return throw_bounds_warning(session, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}


static int check_func_int64_t(Session *session, drizzle_sys_var *var,
                               void *save, drizzle_value *value)
{
  bool fixed;
  int64_t tmp;
  struct option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
    *(uint64_t *)save= getopt_ull_limit_value((uint64_t) tmp, &options,
                                               &fixed);
  else
    *(int64_t *)save= getopt_ll_limit_value(tmp, &options, &fixed);

  return throw_bounds_warning(session, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}

static int check_func_str(Session *session, drizzle_sys_var *,
                          void *save, drizzle_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  int length;

  length= sizeof(buff);
  if ((str= value->val_str(value, buff, &length)))
    str= session->strmake(str, length);
  *(const char**)save= str;
  return 0;
}


static void update_func_bool(Session *, drizzle_sys_var *,
                             void *tgt, const void *save)
{
  *(bool *) tgt= *(int *) save ? 1 : 0;
}


static void update_func_int(Session *, drizzle_sys_var *,
                             void *tgt, const void *save)
{
  *(int *)tgt= *(int *) save;
}


static void update_func_long(Session *, drizzle_sys_var *,
                             void *tgt, const void *save)
{
  *(long *)tgt= *(long *) save;
}


static void update_func_int64_t(Session *, drizzle_sys_var *,
                                 void *tgt, const void *save)
{
  *(int64_t *)tgt= *(uint64_t *) save;
}


static void update_func_str(Session *, drizzle_sys_var *var,
                             void *tgt, const void *save)
{
  char *old= *(char **) tgt;
  *(char **)tgt= *(char **) save;
  if (var->flags & PLUGIN_VAR_MEMALLOC)
  {
    *(char **)tgt= strdup(*(char **) save);
    free(old);
    /*
     * There isn't a _really_ good thing to do here until this whole set_var
     * mess gets redesigned
     */
    if (tgt == NULL)
      errmsg_printf(ERRMSG_LVL_ERROR, _("Out of memory."));

  }
}


/****************************************************************************
  System Variables support
****************************************************************************/


sys_var *find_sys_var(Session *, const char *str, uint32_t length)
{
  sys_var *var;
  sys_var_pluginvar *pi= NULL;
  module::Module *module;

  if ((var= intern_find_sys_var(str, length, false)) &&
      (pi= var->cast_pluginvar()))
  {
    if (!(module= pi->plugin))
      var= NULL; /* failed to lock it, it must be uninstalling */
    else if (module->isInited == false)
    {
      var= NULL;
    }
  }

  /*
    If the variable exists but the plugin it is associated with is not ready
    then the intern_plugin_lock did not raise an error, so we do it here.
  */
  if (pi && !var)
  {
    my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);
    assert(false);
  }
  return(var);
}

static const string make_bookmark_name(const string &plugin, const char *name)
{
  string varname(plugin);
  varname.push_back('_');
  varname.append(name);

  dashes_to_underscores(varname);
  return varname;
}

/*
  called by register_var, construct_options and test_plugin_options.
  Returns the 'bookmark' for the named variable.
  LOCK_system_variables_hash should be at least read locked
*/
static Bookmark *find_bookmark(const string &plugin, const char *name, int flags)
{
  if (!(flags & PLUGIN_VAR_SessionLOCAL))
    return NULL;

  const string varname(make_bookmark_name(plugin, name));

  bookmark_unordered_map::iterator iter= bookmark_hash.find(varname);
  if (iter != bookmark_hash.end())
  {
    return &((*iter).second);
  }
  return NULL;
}


/*
  returns a bookmark for session-local variables, creating if neccessary.
  returns null for non session-local variables.
  Requires that a write lock is obtained on LOCK_system_variables_hash
*/
static Bookmark *register_var(const string &plugin, const char *name,
                                 int flags)
{
  if (!(flags & PLUGIN_VAR_SessionLOCAL))
    return NULL;

  uint32_t size= 0, offset, new_size;
  Bookmark *result= NULL;

  switch (flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:
    size= ALIGN_SIZE(sizeof(bool));
    break;
  case PLUGIN_VAR_INT:
    size= ALIGN_SIZE(sizeof(int));
    break;
  case PLUGIN_VAR_LONG:
    size= ALIGN_SIZE(sizeof(long));
    break;
  case PLUGIN_VAR_LONGLONG:
    size= ALIGN_SIZE(sizeof(uint64_t));
    break;
  case PLUGIN_VAR_STR:
    size= ALIGN_SIZE(sizeof(char*));
    break;
  default:
    assert(0);
    return NULL;
  };


  if (!(result= find_bookmark(plugin, name, flags)))
  {
    const string varname(make_bookmark_name(plugin, name));

    Bookmark new_bookmark;
    new_bookmark.key= varname;
    new_bookmark.offset= -1;

    assert(size && !(size & (size-1))); /* must be power of 2 */

    offset= global_system_variables.dynamic_variables_size;
    offset= (offset + size - 1) & ~(size - 1);
    new_bookmark.offset= (int) offset;

    new_size= (offset + size + 63) & ~63;

    if (new_size > global_variables_dynamic_size)
    {
      char* tmpptr= NULL;
      if (!(tmpptr=
              (char *)realloc(global_system_variables.dynamic_variables_ptr,
                              new_size)))
        return NULL;
      global_system_variables.dynamic_variables_ptr= tmpptr;
      tmpptr= NULL;
      if (!(tmpptr=
              (char *)realloc(max_system_variables.dynamic_variables_ptr,
                              new_size)))
        return NULL;
      max_system_variables.dynamic_variables_ptr= tmpptr;
           
      /*
        Clear the new variable value space. This is required for string
        variables. If their value is non-NULL, it must point to a valid
        string.
      */
      memset(global_system_variables.dynamic_variables_ptr +
             global_variables_dynamic_size, 0,
             new_size - global_variables_dynamic_size);
      memset(max_system_variables.dynamic_variables_ptr +
             global_variables_dynamic_size, 0,
             new_size - global_variables_dynamic_size);
      global_variables_dynamic_size= new_size;
    }

    global_system_variables.dynamic_variables_head= offset;
    max_system_variables.dynamic_variables_head= offset;
    global_system_variables.dynamic_variables_size= offset + size;
    max_system_variables.dynamic_variables_size= offset + size;
    global_system_variables.dynamic_variables_version++;
    max_system_variables.dynamic_variables_version++;

    new_bookmark.version= global_system_variables.dynamic_variables_version;
    new_bookmark.type_code= flags;

    /* this should succeed because we have already checked if a dup exists */
    bookmark_hash.insert(make_pair(varname, new_bookmark));
    result= find_bookmark(plugin, name, flags);
  }
  return result;
}


/*
  returns a pointer to the memory which holds the session-local variable or
  a pointer to the global variable if session==null.
  If required, will sync with global variables if the requested variable
  has not yet been allocated in the current thread.
*/
static unsigned char *intern_sys_var_ptr(Session* session, int offset, bool global_lock)
{
  assert(offset >= 0);
  assert((uint32_t)offset <= global_system_variables.dynamic_variables_head);

  if (!session)
    return (unsigned char*) global_system_variables.dynamic_variables_ptr + offset;

  /*
    dynamic_variables_head points to the largest valid offset
  */
  if (!session->variables.dynamic_variables_ptr ||
      (uint32_t)offset > session->variables.dynamic_variables_head)
  {
    char *tmpptr= NULL;
    if (!(tmpptr= (char *)realloc(session->variables.dynamic_variables_ptr,
                                  global_variables_dynamic_size)))
      return NULL;
    session->variables.dynamic_variables_ptr= tmpptr;

    if (global_lock)
      LOCK_global_system_variables.lock();

    //safe_mutex_assert_owner(&LOCK_global_system_variables);

    memcpy(session->variables.dynamic_variables_ptr +
             session->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_ptr +
             session->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_size -
             session->variables.dynamic_variables_size);

    /*
      now we need to iterate through any newly copied 'defaults'
      and if it is a string type with MEMALLOC flag, we need to strdup
    */
    bookmark_unordered_map::iterator iter= bookmark_hash.begin();
    for (; iter != bookmark_hash.end() ; ++iter)
    {
      sys_var_pluginvar *pi;
      sys_var *var;
      const Bookmark &v= (*iter).second;
      const string var_name((*iter).first);

      if (v.version <= session->variables.dynamic_variables_version ||
          !(var= intern_find_sys_var(var_name.c_str(), var_name.size(), true)) ||
          !(pi= var->cast_pluginvar()) ||
          v.type_code != (pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK))
        continue;

      /* Here we do anything special that may be required of the data types */

      if ((pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
          pi->plugin_var->flags & PLUGIN_VAR_MEMALLOC)
      {
         char **pp= (char**) (session->variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1));
         if ((*pp= *(char**) (global_system_variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1))))
           *pp= strdup(*pp);
         if (*pp == NULL)
           return NULL;
      }
    }

    if (global_lock)
      LOCK_global_system_variables.unlock();

    session->variables.dynamic_variables_version=
           global_system_variables.dynamic_variables_version;
    session->variables.dynamic_variables_head=
           global_system_variables.dynamic_variables_head;
    session->variables.dynamic_variables_size=
           global_system_variables.dynamic_variables_size;
  }
  return (unsigned char*)session->variables.dynamic_variables_ptr + offset;
}

static bool *mysql_sys_var_ptr_bool(Session* a_session, int offset)
{
  return (bool *)intern_sys_var_ptr(a_session, offset, true);
}

static int *mysql_sys_var_ptr_int(Session* a_session, int offset)
{
  return (int *)intern_sys_var_ptr(a_session, offset, true);
}

static long *mysql_sys_var_ptr_long(Session* a_session, int offset)
{
  return (long *)intern_sys_var_ptr(a_session, offset, true);
}

static int64_t *mysql_sys_var_ptr_int64_t(Session* a_session, int offset)
{
  return (int64_t *)intern_sys_var_ptr(a_session, offset, true);
}

static char **mysql_sys_var_ptr_str(Session* a_session, int offset)
{
  return (char **)intern_sys_var_ptr(a_session, offset, true);
}

void plugin_sessionvar_init(Session *session)
{
  session->variables.storage_engine= NULL;
  cleanup_variables(&session->variables);

  session->variables= global_system_variables;
  session->variables.storage_engine= NULL;

  /* we are going to allocate these lazily */
  session->variables.dynamic_variables_version= 0;
  session->variables.dynamic_variables_size= 0;
  session->variables.dynamic_variables_ptr= 0;

  session->variables.storage_engine= global_system_variables.storage_engine;
}


/*
  Unlocks all system variables which hold a reference
*/
static void unlock_variables(Session *, struct drizzle_system_variables *vars)
{
  vars->storage_engine= NULL;
}


/*
  Frees memory used by system variables

  Unlike plugin_vars_free_values() it frees all variables of all plugins,
  it's used on shutdown.
*/
static void cleanup_variables(drizzle_system_variables *vars)
{
  assert(vars->storage_engine == NULL);

  free(vars->dynamic_variables_ptr);
  vars->dynamic_variables_ptr= NULL;
  vars->dynamic_variables_size= 0;
  vars->dynamic_variables_version= 0;
}


void plugin_sessionvar_cleanup(Session *session)
{
  unlock_variables(session, &session->variables);
  cleanup_variables(&session->variables);
}


/**
  @brief Free values of thread variables of a plugin.

  This must be called before a plugin is deleted. Otherwise its
  variables are no longer accessible and the value space is lost. Note
  that only string values with PLUGIN_VAR_MEMALLOC are allocated and
  must be freed.

  @param[in]        vars        Chain of system variables of a plugin
*/

static void plugin_vars_free_values(module::Module::Variables &vars)
{

  for (module::Module::Variables::iterator iter= vars.begin();
       iter != vars.end();
       ++iter)
  {
    sys_var_pluginvar *piv= (*iter)->cast_pluginvar();
    if (piv &&
        ((piv->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR) &&
        (piv->plugin_var->flags & PLUGIN_VAR_MEMALLOC))
    {
      /* Free the string from global_system_variables. */
      char **valptr= (char**) piv->real_value_ptr(NULL, OPT_GLOBAL);
      free(*valptr);
      *valptr= NULL;
    }
  }
  return;
}


bool sys_var_pluginvar::check_update_type(Item_result type)
{
  if (is_readonly())
    return 1;
  switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_INT:
  case PLUGIN_VAR_LONG:
  case PLUGIN_VAR_LONGLONG:
    return type != INT_RESULT;
  case PLUGIN_VAR_STR:
    return type != STRING_RESULT;
  default:
    return 0;
  }
}


SHOW_TYPE sys_var_pluginvar::show_type()
{
  switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:
    return SHOW_MY_BOOL;
  case PLUGIN_VAR_INT:
    return SHOW_INT;
  case PLUGIN_VAR_LONG:
    return SHOW_LONG;
  case PLUGIN_VAR_LONGLONG:
    return SHOW_LONGLONG;
  case PLUGIN_VAR_STR:
    return SHOW_CHAR_PTR;
  default:
    assert(0);
    return SHOW_UNDEF;
  }
}


unsigned char* sys_var_pluginvar::real_value_ptr(Session *session, sql_var_t type)
{
  assert(session || (type == OPT_GLOBAL));
  if (plugin_var->flags & PLUGIN_VAR_SessionLOCAL)
  {
    if (type == OPT_GLOBAL)
      session= NULL;

    return intern_sys_var_ptr(session, *(int*) (plugin_var+1), false);
  }
  return *(unsigned char**) (plugin_var+1);
}


TYPELIB* sys_var_pluginvar::plugin_var_typelib(void)
{
  switch (plugin_var->flags & (PLUGIN_VAR_TYPEMASK | PLUGIN_VAR_SessionLOCAL)) {
  case PLUGIN_VAR_SessionLOCAL:
    return ((sessionvar_enum_t *)plugin_var)->typelib;
  default:
    return NULL;
  }
  return NULL;
}


unsigned char* sys_var_pluginvar::value_ptr(Session *session, sql_var_t type, const LEX_STRING *)
{
  unsigned char* result;

  result= real_value_ptr(session, type);

  return result;
}


bool sys_var_pluginvar::check(Session *session, set_var *var)
{
  st_item_value_holder value;
  assert(is_readonly() || plugin_var->check);

  value.value_type= item_value_type;
  value.val_str= item_val_str;
  value.val_int= item_val_int;
  value.val_real= item_val_real;
  value.item= var->value;

  return is_readonly() ||
         plugin_var->check(session, plugin_var, &var->save_result, &value);
}


void sys_var_pluginvar::set_default(Session *session, sql_var_t type)
{
  const void *src;
  void *tgt;

  assert(is_readonly() || plugin_var->update);

  if (is_readonly())
    return;

  LOCK_global_system_variables.lock();
  tgt= real_value_ptr(session, type);
  src= ((void **) (plugin_var + 1) + 1);

  if (plugin_var->flags & PLUGIN_VAR_SessionLOCAL)
  {
    if (type != OPT_GLOBAL)
      src= real_value_ptr(session, OPT_GLOBAL);
    else
    switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
	case PLUGIN_VAR_INT:
	  src= &((sessionvar_uint_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_LONG:
	  src= &((sessionvar_ulong_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_LONGLONG:
	  src= &((sessionvar_uint64_t_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_BOOL:
	  src= &((sessionvar_bool_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_STR:
	  src= &((sessionvar_str_t*) plugin_var)->def_val;
	  break;
	default:
	  assert(0);
	}
  }

  /* session must equal current_session if PLUGIN_VAR_SessionLOCAL flag is set */
  assert(!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) ||
              session == current_session);

  if (!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) || type == OPT_GLOBAL)
  {
    plugin_var->update(session, plugin_var, tgt, src);
    LOCK_global_system_variables.unlock();
  }
  else
  {
    LOCK_global_system_variables.unlock();
    plugin_var->update(session, plugin_var, tgt, src);
  }
}


bool sys_var_pluginvar::update(Session *session, set_var *var)
{
  void *tgt;

  assert(is_readonly() || plugin_var->update);

  /* session must equal current_session if PLUGIN_VAR_SessionLOCAL flag is set */
  assert(!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) ||
              session == current_session);

  if (is_readonly())
    return 1;

  LOCK_global_system_variables.lock();
  tgt= real_value_ptr(session, var->type);

  if (!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) || var->type == OPT_GLOBAL)
  {
    /* variable we are updating has global scope, so we unlock after updating */
    plugin_var->update(session, plugin_var, tgt, &var->save_result);
    LOCK_global_system_variables.unlock();
  }
  else
  {
    LOCK_global_system_variables.unlock();
    plugin_var->update(session, plugin_var, tgt, &var->save_result);
  }
 return 0;
}


#define OPTION_SET_LIMITS(type, options, opt) \
  options->var_type= type;                    \
  options->def_value= (opt)->def_val;         \
  options->min_value= (opt)->min_val;         \
  options->max_value= (opt)->max_val;         \
  options->block_size= (long) (opt)->blk_sz


void plugin_opt_set_limits(struct option *options,
													 const drizzle_sys_var *opt)
{
  options->sub_size= 0;

  switch (opt->flags & (PLUGIN_VAR_TYPEMASK |
                        PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL)) {
  /* global system variables */
  case PLUGIN_VAR_INT:
    OPTION_SET_LIMITS(GET_INT, options, (sysvar_int_t*) opt);
    break;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_UINT, options, (sysvar_uint_t*) opt);
    break;
  case PLUGIN_VAR_LONG:
    OPTION_SET_LIMITS(GET_LONG, options, (sysvar_long_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_ULONG_IS_FAIL, options, (sysvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG:
    OPTION_SET_LIMITS(GET_LL, options, (sysvar_int64_t_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_ULL, options, (sysvar_uint64_t_t*) opt);
    break;
  case PLUGIN_VAR_BOOL:
    options->var_type= GET_BOOL;
    options->def_value= ((sysvar_bool_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_STR:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr_t) ((sysvar_str_t*) opt)->def_val;
    break;
  /* threadlocal variables */
  case PLUGIN_VAR_INT | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_INT, options, (sessionvar_int_t*) opt);
    break;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_UINT, options, (sessionvar_uint_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_LONG, options, (sessionvar_long_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_ULONG_IS_FAIL, options, (sessionvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_LL, options, (sessionvar_int64_t_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_ULL, options, (sessionvar_uint64_t_t*) opt);
    break;
  case PLUGIN_VAR_BOOL | PLUGIN_VAR_SessionLOCAL:
    options->var_type= GET_BOOL;
    options->def_value= ((sessionvar_bool_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_STR | PLUGIN_VAR_SessionLOCAL:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr_t) ((sessionvar_str_t*) opt)->def_val;
    break;
  default:
    assert(0);
  }
  options->arg_type= REQUIRED_ARG;
  if (opt->flags & PLUGIN_VAR_NOCMDARG)
    options->arg_type= NO_ARG;
  if (opt->flags & PLUGIN_VAR_OPCMDARG)
    options->arg_type= OPT_ARG;
}

static int construct_options(memory::Root *mem_root, module::Module *tmp,
                             option *options)
{
  
  int localoptionid= 256;
  const string plugin_name(tmp->getManifest().name);

  size_t namelen= plugin_name.size(), optnamelen;

  char *optname, *p;
  int index= 0, offset= 0;
  drizzle_sys_var *opt, **plugin_option;
  Bookmark *v;

  string name(plugin_name);
  transform(name.begin(), name.end(), name.begin(), ::tolower);

  underscores_to_dashes(name);

  /*
    Two passes as the 2nd pass will take pointer addresses for use
    by my_getopt and register_var() in the first pass uses realloc
  */

  for (plugin_option= tmp->getManifest().system_vars;
       plugin_option && *plugin_option; plugin_option++, index++)
  {
    opt= *plugin_option;
    if (!(opt->flags & PLUGIN_VAR_SessionLOCAL))
      continue;
    if (!(register_var(name, opt->name, opt->flags)))
      continue;
    switch (opt->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      (((sessionvar_bool_t *)opt)->resolve)= mysql_sys_var_ptr_bool;
      break;
    case PLUGIN_VAR_INT:
      (((sessionvar_int_t *)opt)->resolve)= mysql_sys_var_ptr_int;
      break;
    case PLUGIN_VAR_LONG:
      (((sessionvar_long_t *)opt)->resolve)= mysql_sys_var_ptr_long;
      break;
    case PLUGIN_VAR_LONGLONG:
      (((sessionvar_int64_t_t *)opt)->resolve)= mysql_sys_var_ptr_int64_t;
      break;
    case PLUGIN_VAR_STR:
      (((sessionvar_str_t *)opt)->resolve)= mysql_sys_var_ptr_str;
      break;
    default:
      errmsg_printf(ERRMSG_LVL_ERROR, _("Unknown variable type code 0x%x in plugin '%s'."),
                      opt->flags, plugin_name.c_str());
      return(-1);
    };
  }

  for (plugin_option= tmp->getManifest().system_vars;
       plugin_option && *plugin_option; plugin_option++, index++)
  {
    switch ((opt= *plugin_option)->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      if (!opt->check)
        opt->check= check_func_bool;
      if (!opt->update)
        opt->update= update_func_bool;
      break;
    case PLUGIN_VAR_INT:
      if (!opt->check)
        opt->check= check_func_int;
      if (!opt->update)
        opt->update= update_func_int;
      break;
    case PLUGIN_VAR_LONG:
      if (!opt->check)
        opt->check= check_func_long;
      if (!opt->update)
        opt->update= update_func_long;
      break;
    case PLUGIN_VAR_LONGLONG:
      if (!opt->check)
        opt->check= check_func_int64_t;
      if (!opt->update)
        opt->update= update_func_int64_t;
      break;
    case PLUGIN_VAR_STR:
      if (!opt->check)
        opt->check= check_func_str;
      if (!opt->update)
      {
        opt->update= update_func_str;
        if ((opt->flags & (PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY)) == false)
        {
          opt->flags|= PLUGIN_VAR_READONLY;
          errmsg_printf(ERRMSG_LVL_WARN, _("Server variable %s of plugin %s was forced "
                            "to be read-only: string variable without "
                            "update_func and PLUGIN_VAR_MEMALLOC flag"),
                            opt->name, plugin_name.c_str());
        }
      }
      break;
    default:
      errmsg_printf(ERRMSG_LVL_ERROR, _("Unknown variable type code 0x%x in plugin '%s'."),
                      opt->flags, plugin_name.c_str());
      return(-1);
    }

    if ((opt->flags & (PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_SessionLOCAL))
                    == PLUGIN_VAR_NOCMDOPT)
      continue;

    if (!opt->name)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Missing variable name in plugin '%s'."),
                    plugin_name.c_str());
      return(-1);
    }

    if (!(opt->flags & PLUGIN_VAR_SessionLOCAL))
    {
      optnamelen= strlen(opt->name);
      optname= (char*) mem_root->alloc_root(namelen + optnamelen + 2);
      sprintf(optname, "%s-%s", name.c_str(), opt->name);
      optnamelen= namelen + optnamelen + 1;
    }
    else
    {
      /* this should not fail because register_var should create entry */
      if (!(v= find_bookmark(name, opt->name, opt->flags)))
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("Thread local variable '%s' not allocated "
                      "in plugin '%s'."), opt->name, plugin_name.c_str());
        return(-1);
      }

      *(int*)(opt + 1)= offset= v->offset;

      if (opt->flags & PLUGIN_VAR_NOCMDOPT)
        continue;

      optname= (char*) mem_root->memdup_root(v->key.c_str(), (optnamelen= v->key.size()) + 1);
    }

    /* convert '_' to '-' */
    for (p= optname; *p; p++)
      if (*p == '_')
        *p= '-';

    options->name= optname;
    options->comment= opt->comment;
    options->app_type= opt;
    options->id= localoptionid++;

    plugin_opt_set_limits(options, opt);

    if (opt->flags & PLUGIN_VAR_SessionLOCAL)
      options->value= options->u_max_value= (char**)
        (global_system_variables.dynamic_variables_ptr + offset);
    else
      options->value= options->u_max_value= *(char***) (opt + 1);

    options++;
  }

  return(0);
}


static option *construct_help_options(memory::Root *mem_root, module::Module *p)
{
  drizzle_sys_var **opt;
  option *opts;
  uint32_t count= EXTRA_OPTIONS;

  for (opt= p->getManifest().system_vars; opt && *opt; opt++, count++) {};

  opts= (option*)mem_root->alloc_root((sizeof(option) * count));
  if (opts == NULL)
    return NULL;

  memset(opts, 0, sizeof(option) * count);

  if (construct_options(mem_root, p, opts))
    return NULL;

  return(opts);
}

void drizzle_add_plugin_sysvar(sys_var_pluginvar *var)
{
  plugin_sysvar_vec.push_back(var);
}

void drizzle_del_plugin_sysvar()
{
  vector<sys_var_pluginvar *>::iterator iter= plugin_sysvar_vec.begin();
  while(iter != plugin_sysvar_vec.end())
  {
    delete *iter;
    ++iter;
  }
  plugin_sysvar_vec.clear();
}

/*
  SYNOPSIS
    test_plugin_options()
    tmp_root                    temporary scratch space
    plugin                      internal plugin structure
    default_enabled             default plugin enable status
  RETURNS:
    0 SUCCESS - plugin should be enabled/loaded
  NOTE:
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static int test_plugin_options(memory::Root *module_root,
                               module::Module *test_module,
                               po::options_description &long_options)
{
  drizzle_sys_var **opt;
  option *opts= NULL;
  int error;
  drizzle_sys_var *o;
  Bookmark *var;
  uint32_t len, count= EXTRA_OPTIONS;

  if (test_module->getManifest().init_options != NULL)
  {
    string plugin_section_title("Options used by ");
    plugin_section_title.append(test_module->getName());
    po::options_description module_options(plugin_section_title);
    module::option_context opt_ctx(test_module->getName(),
                                   module_options.add_options());
    test_module->getManifest().init_options(opt_ctx);
    long_options.add(module_options);

  }

  for (opt= test_module->getManifest().system_vars; opt && *opt; opt++)
  {
    count++;
  }

  if (count > EXTRA_OPTIONS)
  {
    if (!(opts= (option*) module_root->alloc_root(sizeof(option) * count)))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Out of memory for plugin '%s'."),
                    test_module->getName().c_str());
      return(-1);
    }
    memset(opts, 0, sizeof(option) * count);

    if (construct_options(module_root, test_module, opts))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Bad options for plugin '%s'."),
                    test_module->getName().c_str());
      return(-1);
    }

  }

  error= 1;

  {
    for (opt= test_module->getManifest().system_vars; opt && *opt; opt++)
    {
      sys_var *v;
      if (((o= *opt)->flags & PLUGIN_VAR_NOSYSVAR))
        continue;

      if ((var= find_bookmark(test_module->getName(), o->name, o->flags)))
      {
        v= new sys_var_pluginvar(var->key.c_str(), o);
      }
      else
      {
        len= test_module->getName().length() + strlen(o->name) + 2;
        string vname(test_module->getName());
        vname.push_back('-');
        vname.append(o->name);
        transform(vname.begin(), vname.end(), vname.begin(), ::tolower);
        dashes_to_underscores(vname);

        v= new sys_var_pluginvar(vname, o);
      }
      assert(v); /* check that an object was actually constructed */

      drizzle_add_plugin_sysvar(static_cast<sys_var_pluginvar *>(v));
      try
      {
        add_sys_var_to_list(v);
        test_module->addSysVar(v);
      }
      catch (...)
      {
        errmsg_printf(ERRMSG_LVL_ERROR,
                      _("Plugin '%s' has conflicting system variables"),
                      test_module->getName().c_str());
        goto err;
      }

    }

    return(0);
  }

err:
  if (opts)
    my_cleanup_options(opts);
  return(error);
}


/****************************************************************************
  Help Verbose text with Plugin System Variables
****************************************************************************/

class OptionCmp
{
public:
  bool operator() (const option &a, const option &b)
  {
    return my_strcasecmp(&my_charset_utf8_general_ci, a.name, b.name);
  }
};


void my_print_help_inc_plugins(option *main_options)
{
  module::Registry &registry= module::Registry::singleton();
  vector<option> all_options;
  memory::Root mem_root(4096);
  option *opt= NULL;


  if (initialized)
  {
    std::map<std::string, module::Module *>::const_iterator modules=
      registry.getModulesMap().begin();
    
    while (modules != registry.getModulesMap().end())
    {
      module::Module *p= (*modules).second;
      ++modules;

      /* If we have an init_options function, we are registering
         commmand line options that way, so don't do them this way */
      if (p->getManifest().init_options != NULL)
        continue;

      if (p->getManifest().system_vars == NULL)
        continue;

      opt= construct_help_options(&mem_root, p);
      if (opt == NULL)
        continue;

      /* Only options with a non-NULL comment are displayed in help text */
      for (;opt->id; opt++)
      {
        if (opt->comment)
        {
          all_options.push_back(*opt);
          
        }
      }
    }
  }

  for (;main_options->id; main_options++)
  {
    if (main_options->comment)
    {
      all_options.push_back(*main_options);
    }
  }

  /** 
   * @TODO: Fix the option building so that it doens't break sort
   *
   * sort(all_options.begin(), all_options.end(), OptionCmp());
   */

  /* main_options now points to the empty option terminator */
  all_options.push_back(*main_options);

  my_print_help(&*(all_options.begin()));

  mem_root.free_root(MYF(0));
}

} /* namespace drizzled */


